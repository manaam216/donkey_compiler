/*
 * Differential tests for the optimiser.
 *
 * The verifier says the IR is well formed. It does not say the optimiser
 * preserved what the program computes, and that is the property that actually
 * matters -- a pass can produce perfectly well formed code for the wrong
 * answer.
 *
 * So this interprets the IR. Each case is compiled twice from the same source,
 * once with no passes and once at -O3, and both are run on the same arguments.
 * The unoptimised run is the oracle: whatever the optimiser did, the two must
 * agree. That catches a wrong fold, a phi wired to the wrong edge, and a
 * hoisted instruction that was not really invariant, none of which the
 * verifier can see.
 *
 * The interpreter handles the integer subset the cases use. It is not a
 * general one, and it is not meant to become the compiler's -- it exists to
 * disagree with itself when a pass is wrong.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "decl.h"
#include "type.h"
#include "symbol.h"
#include "diag.h"
#include "ir.h"
#include "opt.h"
#include "unit.h"

#define MEMORY_BYTES 4096

struct machine {
    struct ir_program *program;
    struct ir_func *func;
    int depth;
    long long *values;          /* one per value id */
    char *memory;               /* backing for whatever stayed in a slot */
    int next_offset;
    int steps;
    int trapped;
};

static long long read_value(struct machine *m, struct ir_value *value)
{
    return value ? m->values[value->id] : 0;
}

static long long truncate_to(long long value, struct Type *ty)
{
    if (!ty) {
        return value;
    }
    switch (ty->size) {
        case 1: return ty->is_unsigned ? (long long)(unsigned char)value
                                       : (long long)(signed char)value;
        case 2: return ty->is_unsigned ? (long long)(unsigned short)value
                                       : (long long)(short)value;
        case 4: return ty->is_unsigned ? (long long)(unsigned int)value
                                       : (long long)(int)value;
        default: return value;
    }
}

static long long apply(struct machine *m, IROp op, long long a, long long b,
    int is_unsigned)
{
    unsigned long long ua = (unsigned long long)a;
    unsigned long long ub = (unsigned long long)b;

    switch (op) {
        case IR_ADD: return a + b;
        case IR_SUB: return a - b;
        case IR_MUL: return a * b;
        case IR_DIV:
            if (b == 0) { m->trapped = 1; return 0; }
            return is_unsigned ? (long long)(ua / ub) : a / b;
        case IR_MOD:
            if (b == 0) { m->trapped = 1; return 0; }
            return is_unsigned ? (long long)(ua % ub) : a % b;
        case IR_SHL: return (long long)(ua << (b & 63));
        case IR_SHR: return is_unsigned ? (long long)(ua >> (b & 63))
                                        : a >> (b & 63);
        case IR_AND: return a & b;
        case IR_OR:  return a | b;
        case IR_XOR: return a ^ b;
        case IR_EQ:  return a == b;
        case IR_NE:  return a != b;
        case IR_LT:  return is_unsigned ? ua <  ub : a <  b;
        case IR_LE:  return is_unsigned ? ua <= ub : a <= b;
        case IR_GT:  return is_unsigned ? ua >  ub : a >  b;
        case IR_GE:  return is_unsigned ? ua >= ub : a >= b;
        default:     return 0;
    }
}

/*
 * Run the function on the given arguments. Returns 0 if it could not be run --
 * an unsupported instruction, a trap, or a step limit that a miscompiled loop
 * would otherwise spin in forever.
 */
static int run_depth(struct ir_program *program, struct ir_func *func,
    const long long *args, int arg_count, long long *result, int depth);

/* The function a direct call names, so the interpreter can step into it. */
static struct ir_func *callee_of(struct ir_program *program,
    struct ir_instr *call)
{
    struct ir_instr *global;
    struct ir_func *func;

    if (call->arg_count < 1 || !call->args[0] || !call->args[0]->def) {
        return NULL;
    }
    global = call->args[0]->def;
    if (global->op != IR_GLOBAL) {
        return NULL;
    }
    for (func = program->first; func; func = func->next) {
        if (global->sym && func->sym) {
            if (global->sym == func->sym) {
                return func;
            }
            continue;
        }
        if (global->text && strcmp(global->text, func->name) == 0) {
            return func;
        }
    }
    return NULL;
}

static int run_depth(struct ir_program *program, struct ir_func *func,
    const long long *args, int arg_count, long long *result, int depth)
{
    struct machine m;
    struct ir_block *block = func->entry;
    struct ir_block *previous = NULL;
    int ok = 1;

    memset(&m, 0, sizeof(m));
    m.program = program;
    m.func = func;
    m.depth = depth;
    /*
     * Deep enough for the recursion the cases use, shallow enough that an
     * unoptimised tail-recursive run cannot take the host's stack with it.
     */
    if (depth > 2000) {
        return 0;
    }
    m.values = calloc((size_t)func->value_count + 1, sizeof(*m.values));
    m.memory = calloc(MEMORY_BYTES, 1);
    if (!m.values || !m.memory) {
        free(m.values);
        free(m.memory);
        return 0;
    }

    while (block && ok) {
        struct ir_instr *instr;
        struct ir_block *next = NULL;
        long long phi_results[64];
        int phi_count = 0;

        /*
         * Every phi in a block takes effect at once, before any of them is
         * visible. A loop header's phis routinely name each other -- the phi
         * for `a` takes the phi for `b` along the back edge -- so evaluating
         * them one at a time would feed the new value of one into the old
         * value of the next, and quietly swap them.
         */
        for (instr = block->first; instr && instr->op == IR_PHI;
             instr = instr->next) {
            int i;

            for (i = 0; i < instr->arg_count; i++) {
                if (instr->phi_blocks[i] == previous) {
                    break;
                }
            }
            if (i == instr->arg_count || phi_count >= 64) {
                ok = 0;                 /* no argument for the edge taken */
                break;
            }
            phi_results[phi_count++] = read_value(&m, instr->args[i]);
        }
        phi_count = 0;
        for (instr = block->first; ok && instr && instr->op == IR_PHI;
             instr = instr->next) {
            m.values[instr->dst->id] = phi_results[phi_count++];
        }

        for (instr = block->first; instr && ok; instr = instr->next) {
            long long a = instr->arg_count > 0
                ? read_value(&m, instr->args[0]) : 0;
            long long b = instr->arg_count > 1 ? read_value(&m, instr->args[1]) : 0;
            int is_unsigned = instr->args && instr->arg_count > 0 &&
                instr->args[0]->ty && instr->args[0]->ty->is_unsigned;

            if (++m.steps > 2000000) {
                ok = 0;
                break;
            }

            switch (instr->op) {
                case IR_CONST:
                    m.values[instr->dst->id] = instr->imm;
                    break;
                case IR_PARAM:
                    m.values[instr->dst->id] =
                        instr->imm < arg_count ? args[instr->imm] : 0;
                    break;
                case IR_ALLOCA:
                    /* Addresses are offsets into the machine's own memory. */
                    m.values[instr->dst->id] = m.next_offset;
                    m.next_offset += 8;
                    if (m.next_offset >= MEMORY_BYTES) {
                        ok = 0;
                    }
                    break;
                case IR_LOAD:
                    if (a < 0 || a + 8 > MEMORY_BYTES) {
                        ok = 0;
                        break;
                    }
                    memcpy(&m.values[instr->dst->id], m.memory + a, 8);
                    break;
                case IR_STORE:
                    if (a < 0 || a + 8 > MEMORY_BYTES) {
                        ok = 0;
                        break;
                    }
                    memcpy(m.memory + a, &b, 8);
                    break;
                case IR_COPY:
                case IR_CAST:
                    m.values[instr->dst->id] =
                        truncate_to(a, instr->dst->ty);
                    break;
                case IR_NEG:
                    m.values[instr->dst->id] = -a;
                    break;
                case IR_COMPLEMENT:
                    m.values[instr->dst->id] = ~a;
                    break;
                case IR_LOGNOT:
                    m.values[instr->dst->id] = !a;
                    break;
                case IR_PHI:
                    break;              /* already committed, all at once */
                case IR_CALL: {
                    struct ir_func *target = callee_of(program, instr);
                    long long call_args[8];
                    long long returned = 0;
                    int n = instr->arg_count - 1;
                    int c;

                    if (!target || n > 8) {
                        ok = 0;
                        break;
                    }
                    for (c = 0; c < n; c++) {
                        call_args[c] = read_value(&m, instr->args[c + 1]);
                    }
                    if (!run_depth(program, target, call_args, n, &returned,
                            depth + 1)) {
                        ok = 0;
                        break;
                    }
                    if (instr->dst) {
                        m.values[instr->dst->id] =
                            truncate_to(returned, instr->dst->ty);
                    }
                    break;
                }
                case IR_JMP:
                    next = instr->target;
                    break;
                case IR_BR:
                    next = a ? instr->then_block : instr->else_block;
                    break;
                case IR_RET:
                    *result = instr->arg_count > 0 ? a : 0;
                    free(m.values);
                    free(m.memory);
                    return !m.trapped;
                default:
                    if (!instr->dst) {
                        break;          /* nothing observable */
                    }
                    m.values[instr->dst->id] = truncate_to(
                        apply(&m, instr->op, a, b, is_unsigned),
                        instr->dst->ty);
                    if (m.trapped) {
                        ok = 0;
                    }
                    break;
            }
        }

        previous = block;
        block = next;
    }

    free(m.values);
    free(m.memory);
    return 0;
}

static int run(struct ir_program *program, struct ir_func *func,
    const long long *args, int arg_count, long long *result)
{
    return run_depth(program, func, args, arg_count, result, 0);
}

/*
 * Compile one function's source into SSA, optionally optimised. Everything the
 * front end allocates is left for the caller's cleanup, which is the whole
 * process: these are short-lived test runs, not a compiler pipeline.
 */
static struct ir_func *build(const char *source, struct ir_program *program,
    int level)
{
    struct token *tokens = NULL;
    struct ast_node *ast;
    struct ir_func *func;
    int token_count = 0;
    int index = 0;

    memset(program, 0, sizeof(*program));
    parser_reset_typedefs();
    parser_reset_enums();
    diag_init("<test>");

    lex_text(source, strlen(source), "<test>", &tokens, &token_count);
    ast = parse_program(tokens, &index, "<test>");
    if (!ast || diag_has_errors()) {
        return NULL;
    }
    if (!semantic_analyze(ast, "<test>") || diag_has_errors()) {
        return NULL;
    }

    ir_lower_program(program, ast);
    for (func = program->first; func; func = func->next) {
        ir_analyze_cfg(func);
        ir_compute_dominators(func);
        ir_compute_frontiers(func);
        ir_build_ssa(func);
        ir_analyze_cfg(func);
        ir_compute_dominators(func);
        ir_compute_frontiers(func);
    }

    /*
     * Whole-program, so inlining can copy one of these functions into another.
     * The cases with helpers depend on it, and running per function would make
     * that pass untestable here.
     */
    {
        struct opt_stats stats;

        opt_run_program(program, level, &stats, NULL);
    }

    /* The case under test is always called f; the rest are what it calls. */
    for (func = program->first; func; func = func->next) {
        if (strcmp(func->name, "f") == 0) {
            return func;
        }
    }
    return program->first;
}

/*
 * Compile the same source twice and check the two agree on every argument.
 * The unoptimised build is the oracle, so a case only needs to say what to
 * run, not what the answer should be -- which means a case can be added
 * without working out by hand what it computes.
 */
static void check_same(const char *label, const char *source,
    const long long *args, int arg_count, int cases)
{
    int i;

    for (i = 0; i < cases; i++) {
        struct ir_program plain;
        struct ir_program optimised;
        struct ir_func *a;
        struct ir_func *b;
        long long expected = 0;
        long long actual = 0;
        int ran_plain;
        int ran_optimised;
        char message[160];

        a = build(source, &plain, 0);
        ran_plain = a && run(&plain, a, &args[i * arg_count], arg_count,
            &expected);
        ir_program_free(&plain);

        b = build(source, &optimised, 3);
        ran_optimised = b && run(&optimised, b, &args[i * arg_count],
            arg_count, &actual);
        ir_program_free(&optimised);

        if (!ran_plain || !ran_optimised) {
            sprintf(message, "%s: case %d could not be run (%d/%d)", label, i,
                ran_plain, ran_optimised);
            check_int(message, 0, 1);
            continue;
        }
        sprintf(message, "%s: case %d", label, i);
        check_int(message, (long)actual, (long)expected);
    }
}

/*
 * Also check the optimiser actually did something. A pass that silently did
 * nothing would agree with the oracle on every case, so equivalence alone
 * cannot tell a correct optimiser from an absent one.
 */
static void check_shrinks(const char *label, const char *source)
{
    struct ir_program plain;
    struct ir_program optimised;
    struct ir_func *a = build(source, &plain, 0);
    struct ir_func *b;
    int before = 0;
    int after = 0;
    char message[160];

    if (a) {
        struct ir_block *block;

        for (block = a->entry; block; block = block->next) {
            struct ir_instr *instr;

            if (block->rpo_index < 0) {
                continue;
            }
            for (instr = block->first; instr; instr = instr->next) {
                before++;
            }
        }
    }
    ir_program_free(&plain);

    b = build(source, &optimised, 3);
    if (b) {
        struct ir_block *block;

        for (block = b->entry; block; block = block->next) {
            struct ir_instr *instr;

            if (block->rpo_index < 0) {
                continue;
            }
            for (instr = block->first; instr; instr = instr->next) {
                after++;
            }
        }
    }
    ir_program_free(&optimised);

    sprintf(message, "%s: -O3 removes instructions (%d -> %d)", label,
        before, after);
    check_int(message, after < before, 1);
}

/*
 * Count the phis, at each level. Equivalence cannot see a pass that simply
 * did nothing, and the total instruction count is too blunt to tell which
 * pass did the removing -- a phi that a loop never needed is specifically what
 * the self-reference rule takes out.
 */
static int count_phis(const char *source, int level)
{
    struct ir_program program;
    struct ir_func *func = build(source, &program, level);
    int count = 0;

    if (func) {
        struct ir_block *block;

        for (block = func->entry; block; block = block->next) {
            struct ir_instr *instr;

            if (block->rpo_index < 0) {
                continue;
            }
            for (instr = block->first; instr && instr->op == IR_PHI;
                 instr = instr->next) {
                count++;
            }
        }
    }
    ir_program_free(&program);
    return count;
}

/*
 * How many of one opcode a named function contains, at a given level. The name
 * matters for the recursion cases: what has to disappear is the call inside the
 * recursive function, not the one that starts it off.
 */
static int count_op_in(const char *source, int level, const char *name, IROp op)
{
    struct ir_program program;
    struct ir_func *func;
    int count = 0;

    build(source, &program, level);
    for (func = program.first; func; func = func->next) {
        if (strcmp(func->name, name) == 0) {
            break;
        }
    }

    if (func) {
        struct ir_block *block;

        for (block = func->entry; block; block = block->next) {
            struct ir_instr *instr;

            if (block->rpo_index < 0) {
                continue;
            }
            for (instr = block->first; instr; instr = instr->next) {
                if (instr->op == op) {
                    count++;
                }
            }
        }
    }
    ir_program_free(&program);
    return count;
}

static const long long two_args[] = {
    0, 0,   1, 1,   2, 3,   7, 5,   -4, 9,   100, -7,   -1, -1,   6, 2
};
static const long long one_arg[] = { 0, 1, 2, 3, 7, 10, -5, 64 };

int main(void)
{
    /* Folding, identities, and strength reduction on a straight line. */
    static const char arithmetic[] =
        "int f(int a, int b) {"
        "  int x = 2 * 3 + 4;"
        "  int y = a * 8;"
        "  int z = b + 0;"
        "  int w = a * 1 - b * 0;"
        "  return x + y + z + w;"
        "}";

    /*
     * Every folding rule on constants, so a wrong one is a wrong answer rather
     * than merely an instruction that stayed. Written with an argument added at
     * the end so the whole function cannot itself fold to one number.
     */
    static const char constants[] =
        "int f(int a, int b) {"
        "  int s = 20 - 7;"
        "  int d = 100 / 8;"
        "  int m = 100 % 7;"
        "  int l = 3 << 4;"
        "  int r = 4096 >> 5;"
        "  int c = (9 > 4) + (9 < 4) + (9 == 9) + (9 != 9) + (2 <= 2);"
        "  int u = -5 + ~6 + !0;"
        "  int k = (7 & 12) | (7 ^ 12);"
        "  return s + d + m + l + r + c + u + k + a - b;"
        "}";

    /*
     * `x = x` is deliberate, not a typo. Phi placement is decided by the shape
     * of the graph and not by whether the values differ, so a store inside the
     * `if` puts a phi at the merge and another at the loop header whatever it
     * stores. Storing what is already there makes the merge phi's arguments
     * agree, which makes the header phi name only itself and `a` -- and a phi
     * that names itself is not a choice. Collapsing that chain is what lets the
     * optimiser return `a` directly.
     */
    static const char unchanging[] =
        "int f(int a, int b) {"
        "  int x = a;"
        "  int i;"
        "  for (i = 0; i < 5; i = i + 1) { if (b > 0) { x = x; } }"
        "  return x;"
        "}";

    /* Branches the optimiser can decide, and one it cannot. */
    static const char branches[] =
        "int f(int a, int b) {"
        "  int r = 0;"
        "  if (1) r = r + 1; else r = 999;"
        "  if (a > b) r = r + a; else r = r + b;"
        "  while (0) r = r + 100;"
        "  return r;"
        "}";

    /* A loop with an invariant subexpression, for LICM to move. */
    static const char loop[] =
        "int f(int a, int b) {"
        "  int total = 0;"
        "  int i = 0;"
        "  for (i = 0; i < 10; i = i + 1) {"
        "    total = total + (a * b) + i;"
        "  }"
        "  return total;"
        "}";

    /* The same expression twice, for value numbering to unify. */
    static const char redundant[] =
        "int f(int a, int b) {"
        "  int p = a * b + a;"
        "  int q = a * b + b;"
        "  return p + q;"
        "}";

    /* Short circuit and the conditional operator, which lower to phis. */
    static const char control[] =
        "int f(int a, int b) {"
        "  int r = (a > 0 && b > 0) ? a + b : a - b;"
        "  if (a > b || b == 0) r = r * 2;"
        "  return r;"
        "}";

    /* A slot whose address escapes must survive every pass untouched. */
    static const char addressed[] =
        "int f(int a, int b) {"
        "  int x = a;"
        "  int *p = &x;"
        "  *p = *p + b;"
        "  return x;"
        "}";

    /* A nested loop, where the inner loop's invariant is the outer's variable. */
    static const char nested[] =
        "int f(int a, int b) {"
        "  int total = 0;"
        "  int i;"
        "  int j;"
        "  for (i = 0; i < 4; i = i + 1) {"
        "    for (j = 0; j < 3; j = j + 1) {"
        "      total = total + i * a + b;"
        "    }"
        "  }"
        "  return total;"
        "}";

    /*
     * Signed division by a power of two is not a shift. C rounds toward zero
     * and an arithmetic shift rounds toward negative infinity, so -7 / 2 is -3
     * while -7 >> 1 is -4. The arguments include negatives precisely so that
     * reducing this to a shift would show up as a wrong answer.
     */
    static const char signed_division[] =
        "int f(int a, int b) {"
        "  int n = a - b;"
        "  return n / 4 + n % 4 + n / 8 + n % 2;"
        "}";

    /*
     * A division inside a loop that may run zero times. `100 / b` does not
     * change within the loop, but hoisting it to before the loop would divide
     * by b even when the body never runs -- and the arguments include a case
     * where the loop is skipped and b is zero, so doing that is a trap the
     * original program does not have.
     */
    static const char trapping[] =
        "int f(int a, int b) {"
        "  int total = 0;"
        "  int i;"
        "  for (i = 0; i < a; i = i + 1) total = total + 100 / b;"
        "  return total;"
        "}";

    /* Division stays put: it can trap, and folding it must respect that. */
    static const char division[] =
        "int f(int a, int b) {"
        "  int n = a * 4;"
        "  if (b == 0) return n;"
        "  return n / b + n % b;"
        "}";

    /* A switch, whose fallthrough and case blocks the CFG pass rearranges. */
    static const char selection[] =
        "int f(int a, int b) {"
        "  int r = b;"
        "  switch (a) {"
        "    case 0: r = r + 1; break;"
        "    case 1: r = r + 2;"
        "    case 2: r = r + 4; break;"
        "    default: r = r - 1;"
        "  }"
        "  return r;"
        "}";

    /*
     * Helpers to copy into f. Small enough to inline, and arranged so that
     * doing so exposes work: once `scale` is copied in, its multiply has a
     * constant on one side.
     */
    static const char inlining[] =
        "int scale(int v, int by) { return v * by; }"
        "int clamp(int v) { if (v > 100) return 100; return v; }"
        "int f(int a, int b) {"
        "  return clamp(scale(a, 4)) + clamp(scale(b, 2)) + scale(3, 3);"
        "}";

    /*
     * A callee that returns from two places. Both returns have to meet in a phi
     * at the point the call resumed, and picking the wrong one is a wrong
     * answer rather than a malformed function.
     */
    static const char inlining_branches[] =
        "int pick(int v, int w) { if (v > w) return v - w; return w - v; }"
        "int f(int a, int b) { return pick(a, b) + pick(b, a) + pick(a, a); }";

    /*
     * Tail recursion, which becomes a loop. The unoptimised side really does
     * recurse, so the two agree only if the loop carries the accumulator the
     * same way the recursion did.
     */
    static const char tail_recursion[] =
        "int down(int n, int acc) {"
        "  if (n <= 0) return acc;"
        "  return down(n - 1, acc + n);"
        "}"
        "int f(int a, int b) { return down(a, b); }";

    /*
     * A recursive call that is not in tail position: the multiply happens after
     * it comes back, so turning it into a jump would skip the work. It must be
     * left alone.
     */
    static const char not_tail[] =
        "int power(int base, int n) {"
        "  if (n <= 0) return 1;"
        "  return base * power(base, n - 1);"
        "}"
        "int f(int a, int b) { return power(a, 3) + power(2, b); }";

    /*
     * A variadic callee, which must not be copied. Lowering gives the ellipsis
     * a parameter of its own, so a call that passes a different number of
     * arguments than the callee has parameters is how one is recognised without
     * anything having to record that it was variadic. The second call passes
     * fewer, which is the direction that reads past the argument list if the
     * check is not made.
     */
    static const char variadic[] =
        "int first(int a, ...) { return a; }"
        "int f(int a, int b) { return first(a, b, 7) + first(b); }";

    int pairs = (int)(sizeof(two_args) / sizeof(two_args[0])) / 2;

    /* Results here are numbers, so the shared string check goes unused. */
    (void)check_str;

    int singles = (int)(sizeof(one_arg) / sizeof(one_arg[0]));

    check_same("arithmetic", arithmetic, two_args, 2, pairs);
    check_same("constants", constants, two_args, 2, pairs);
    check_same("unchanging", unchanging, two_args, 2, pairs);
    check_same("branches", branches, two_args, 2, pairs);
    check_same("loop", loop, two_args, 2, pairs);
    check_same("redundant", redundant, two_args, 2, pairs);
    check_same("control", control, two_args, 2, pairs);
    check_same("addressed", addressed, two_args, 2, pairs);
    check_same("nested", nested, two_args, 2, pairs);
    check_same("division", division, two_args, 2, pairs);
    check_same("signed division", signed_division, two_args, 2, pairs);
    check_same("trapping", trapping, two_args, 2, pairs);
    check_same("selection", selection, two_args, 2, pairs);
    check_same("inlining", inlining, two_args, 2, pairs);
    check_same("inlining branches", inlining_branches, two_args, 2, pairs);
    check_same("tail recursion", tail_recursion, two_args, 2, pairs);
    check_same("not tail", not_tail, two_args, 2, pairs);
    check_same("variadic", variadic, two_args, 2, pairs);

    check_shrinks("arithmetic", arithmetic);
    check_shrinks("constants", constants);
    check_shrinks("unchanging", unchanging);
    check_shrinks("branches", branches);
    check_shrinks("loop", loop);
    check_shrinks("redundant", redundant);
    check_shrinks("selection", selection);

    /*
     * Inlining is the one pass that makes a function bigger, so check_shrinks
     * is the wrong question for it. What has to be true is that the calls went
     * away.
     */
    check_int("inlining: no call is left in f",
        count_op_in(inlining, 3, "f", IR_CALL), 0);
    check_int("inlining branches: no call is left in f",
        count_op_in(inlining_branches, 3, "f", IR_CALL), 0);

    /*
     * The recursion must actually be gone, not merely still correct: a program
     * that still calls itself has not been optimised at all. And the call that
     * is not in tail position has to stay, since the work after it would
     * otherwise be skipped.
     */
    check_int("tail recursion: down no longer calls itself",
        count_op_in(tail_recursion, 3, "down", IR_CALL), 0);
    check_int("tail recursion: down becomes a loop",
        count_op_in(tail_recursion, 3, "down", IR_PHI) > 0, 1);
    check_int("not tail: power keeps its call",
        count_op_in(not_tail, 3, "power", IR_CALL) > 0, 1);
    check_int("variadic: the calls are not inlined",
        count_op_in(variadic, 3, "f", IR_CALL), 2);

    /*
     * Exactly one phi should survive -- the loop counter, which really does
     * differ per iteration. The two for `x` must both go, and an inequality
     * would still pass if only one of them did.
     */
    check_int("unchanging: -O0 needs three phis", count_phis(unchanging, 0), 3);
    check_int("unchanging: -O3 leaves only the loop counter's",
        count_phis(unchanging, 3), 1);

    (void)one_arg;
    (void)singles;
    ty_cleanup();
    sym_cleanup();
    diag_cleanup();
    return unit_report("optimiser");
}
