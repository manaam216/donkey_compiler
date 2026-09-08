/*
 * Unit tests for the IR analyses.
 *
 * Dominance and phi placement are decided by graph shape alone, so they are
 * tested on graphs built directly rather than on ones a C program happens to
 * produce. That makes it possible to check the awkward shapes -- a diamond, a
 * loop, a merge that needs a phi only because of another phi -- without first
 * finding source text that lowers to them.
 *
 * The verifier runs at the end of every case: it is the same check the
 * compiler applies to real functions, so a test that passes it is testing that
 * the IR is well formed and not merely that the numbers came out.
 */
#include <stdio.h>
#include <string.h>
#include "ir.h"
#include "unit.h"

static struct ir_func *start(struct ir_program *program, const char *name)
{
    memset(program, 0, sizeof(*program));
    return ir_func_new(program, name, NULL);
}

static void analyze(struct ir_func *func)
{
    ir_analyze_cfg(func);
    ir_compute_dominators(func);
    ir_compute_frontiers(func);
}

static struct ir_value *constant(struct ir_func *func, struct ir_block *block,
    long value)
{
    struct ir_instr *instr = ir_emit(block, IR_CONST);

    instr->imm = value;
    return ir_set_dst(func, instr, ty_int);
}

static void jump(struct ir_block *from, struct ir_block *to)
{
    struct ir_instr *instr = ir_emit(from, IR_JMP);

    instr->target = to;
}

static void branch(struct ir_func *func, struct ir_block *from,
    struct ir_block *then_block, struct ir_block *else_block)
{
    /* The condition is emitted first: nothing may follow a terminator. */
    struct ir_value *condition = constant(func, from, 1);
    struct ir_instr *instr = ir_emit(from, IR_BR);

    ir_add_arg(instr, condition);
    instr->then_block = then_block;
    instr->else_block = else_block;
}

static struct ir_value *alloca_slot(struct ir_func *func)
{
    struct ir_instr *instr = ir_emit_front(func->entry, IR_ALLOCA);

    return ir_set_dst(func, instr, ty_pointer_to(ty_int));
}

static void store(struct ir_func *func, struct ir_block *block,
    struct ir_value *slot, long value)
{
    /* The stored value exists before the store that consumes it. */
    struct ir_value *stored = constant(func, block, value);
    struct ir_instr *instr = ir_emit(block, IR_STORE);

    ir_add_arg(instr, slot);
    ir_add_arg(instr, stored);
    instr->mem_type = ty_int;
}

static struct ir_value *load(struct ir_func *func, struct ir_block *block,
    struct ir_value *slot)
{
    struct ir_instr *instr = ir_emit(block, IR_LOAD);

    ir_add_arg(instr, slot);
    instr->mem_type = ty_int;
    return ir_set_dst(func, instr, ty_int);
}

static void ret(struct ir_block *block, struct ir_value *value)
{
    struct ir_instr *instr = ir_emit(block, IR_RET);

    if (value) {
        ir_add_arg(instr, value);
    }
}

static int count_phis(struct ir_block *block)
{
    struct ir_instr *instr;
    int count = 0;

    for (instr = block->first; instr; instr = instr->next) {
        if (instr->op == IR_PHI) {
            count++;
        }
    }
    return count;
}

static int count_op(struct ir_func *func, IROp op)
{
    struct ir_block *block;
    int count = 0;

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
    return count;
}

static void check_verifies(const char *label, struct ir_func *func)
{
    check_int(label, ir_verify(func, stderr), 0);
}

/*
 * entry -> then -> merge
 *       -> else -> merge
 *
 * The classic diamond. The merge is dominated by the entry and by nothing
 * else, and it is on the frontier of both arms: that is where the two arms'
 * definitions stop being the only one that could have arrived.
 */
static void test_diamond(void)
{
    struct ir_program program;
    struct ir_func *func = start(&program, "diamond");
    struct ir_block *entry = ir_block_new(func, "entry");
    struct ir_block *then_block = ir_block_new(func, NULL);
    struct ir_block *else_block = ir_block_new(func, NULL);
    struct ir_block *merge = ir_block_new(func, NULL);

    branch(func, entry, then_block, else_block);
    jump(then_block, merge);
    jump(else_block, merge);
    ret(merge, NULL);
    analyze(func);

    check_int("diamond: reachable blocks", func->rpo_count, 4);
    check_int("diamond: entry is first in reverse postorder", entry->rpo_index, 0);
    check_int("diamond: merge is last", merge->rpo_index, 3);

    check_int("diamond: then is dominated by entry",
        ir_dominates(entry, then_block), 1);
    check_int("diamond: merge is dominated by entry",
        ir_dominates(entry, merge), 1);
    check_int("diamond: merge is not dominated by then",
        ir_dominates(then_block, merge), 0);
    check_int("diamond: merge's immediate dominator is the entry",
        merge->idom == entry, 1);

    check_int("diamond: then's frontier is the merge", then_block->frontier_count, 1);
    check_int("diamond: then's frontier names the merge",
        then_block->frontier[0] == merge, 1);
    check_int("diamond: the entry has an empty frontier", entry->frontier_count, 0);

    check_verifies("diamond verifies", func);
    ir_program_free(&program);
}

/*
 * A variable written in both arms of a diamond needs one phi at the merge, and
 * the load there must become that phi.
 */
static void test_phi_at_merge(void)
{
    struct ir_program program;
    struct ir_func *func = start(&program, "merge");
    struct ir_block *entry = ir_block_new(func, "entry");
    struct ir_block *then_block = ir_block_new(func, NULL);
    struct ir_block *else_block = ir_block_new(func, NULL);
    struct ir_block *merge = ir_block_new(func, NULL);
    struct ir_value *slot = alloca_slot(func);

    branch(func, entry, then_block, else_block);
    store(func, then_block, slot, 1);
    jump(then_block, merge);
    store(func, else_block, slot, 2);
    jump(else_block, merge);
    ret(merge, load(func, merge, slot));

    analyze(func);
    ir_build_ssa(func);
    analyze(func);

    check_int("merge: one phi at the merge", count_phis(merge), 1);
    check_int("merge: no phi in the entry", count_phis(entry), 0);
    check_int("merge: every load is gone", count_op(func, IR_LOAD), 0);
    check_int("merge: every store is gone", count_op(func, IR_STORE), 0);
    check_int("merge: the slot is gone", count_op(func, IR_ALLOCA), 0);
    check_int("merge: the phi has one argument per predecessor",
        merge->first->arg_count, 2);
    check_int("merge: the return reads the phi",
        merge->last->args[0] == merge->first->dst, 1);

    check_verifies("merge verifies", func);
    ir_program_free(&program);
}

/*
 * entry -> header -> body -> header
 *                 -> exit
 *
 * A loop. The header is its own frontier's member -- reached from the entry and
 * from the back edge -- so a variable written in the body needs a phi there,
 * and that phi is one of its own arguments.
 */
static void test_loop(void)
{
    struct ir_program program;
    struct ir_func *func = start(&program, "loop");
    struct ir_block *entry = ir_block_new(func, "entry");
    struct ir_block *header = ir_block_new(func, NULL);
    struct ir_block *body = ir_block_new(func, NULL);
    struct ir_block *exit_block = ir_block_new(func, NULL);
    struct ir_value *slot = alloca_slot(func);
    struct ir_instr *phi;

    store(func, entry, slot, 0);
    jump(entry, header);
    branch(func, header, body, exit_block);
    store(func, body, slot, 1);
    jump(body, header);
    ret(exit_block, load(func, exit_block, slot));

    analyze(func);
    check_int("loop: the header has two predecessors", header->pred_count, 2);
    check_int("loop: the header dominates the body",
        ir_dominates(header, body), 1);
    check_int("loop: the body's frontier is the header",
        body->frontier_count == 1 && body->frontier[0] == header, 1);

    ir_build_ssa(func);
    analyze(func);

    check_int("loop: one phi in the header", count_phis(header), 1);
    check_int("loop: no phi in the exit", count_phis(exit_block), 0);
    phi = header->first;
    check_int("loop: the phi has an argument for each way in", phi->arg_count, 2);
    check_int("loop: the phi names the blocks its arguments come from",
        phi->phi_blocks[0] != NULL && phi->phi_blocks[1] != NULL, 1);
    check_int("loop: the exit reads the phi",
        exit_block->last->args[0] == phi->dst, 1);

    check_verifies("loop verifies", func);
    ir_program_free(&program);
}

/*
 * A slot whose address is used for anything but a load or a store may be
 * written by something this function cannot see, so it must stay in memory.
 */
static void test_escaping_slot_is_not_promoted(void)
{
    struct ir_program program;
    struct ir_func *func = start(&program, "escapes");
    struct ir_block *entry = ir_block_new(func, "entry");
    struct ir_value *slot = alloca_slot(func);
    struct ir_value *callee;
    struct ir_instr *call;

    store(func, entry, slot, 7);
    callee = constant(func, entry, 0);
    call = ir_emit(entry, IR_CALL);
    ir_add_arg(call, callee);
    ir_add_arg(call, slot);                         /* the address escapes */
    ret(entry, load(func, entry, slot));

    analyze(func);
    ir_build_ssa(func);
    analyze(func);

    check_int("escapes: the slot survives", count_op(func, IR_ALLOCA), 1);
    check_int("escapes: the load survives", count_op(func, IR_LOAD), 1);
    check_int("escapes: the store survives", count_op(func, IR_STORE), 1);

    check_verifies("escapes verifies", func);
    ir_program_free(&program);
}

/*
 * A block nothing branches to is dropped rather than carried along, so no
 * later pass has to reason about code that cannot run.
 */
static void test_unreachable_block_is_dropped(void)
{
    struct ir_program program;
    struct ir_func *func = start(&program, "unreachable");
    struct ir_block *entry = ir_block_new(func, "entry");
    struct ir_block *orphan = ir_block_new(func, NULL);

    ret(entry, constant(func, entry, 0));
    ret(orphan, constant(func, orphan, 1));
    analyze(func);

    check_int("unreachable: only the entry is reachable", func->rpo_count, 1);
    check_int("unreachable: the orphan has no reverse postorder number",
        orphan->rpo_index, -1);
    check_int("unreachable: the orphan is empty", orphan->first == NULL, 1);

    check_verifies("unreachable verifies", func);
    ir_program_free(&program);
}

/*
 * A definition that reaches a use only along one path is not usable at the
 * use, and the verifier has to say so -- this is the invariant every later
 * pass is allowed to rely on, so a silent failure here would be invisible
 * until the generated code was wrong.
 */
static void test_verifier_catches_a_broken_use(void)
{
    struct ir_program program;
    struct ir_func *func = start(&program, "broken");
    struct ir_block *entry = ir_block_new(func, "entry");
    struct ir_block *then_block = ir_block_new(func, NULL);
    struct ir_block *else_block = ir_block_new(func, NULL);
    struct ir_block *merge = ir_block_new(func, NULL);
    struct ir_value *only_on_one_path;

    branch(func, entry, then_block, else_block);
    only_on_one_path = constant(func, then_block, 1);
    jump(then_block, merge);
    jump(else_block, merge);
    ret(merge, only_on_one_path);
    analyze(func);

    check_int("broken: the verifier reports the use",
        ir_verify(func, stdout) > 0, 1);
    ir_program_free(&program);
}

/* The dump is how every later pass will be read, so its names are pinned. */
static void test_op_names(void)
{
    check_str("op name: phi", ir_op_name(IR_PHI), "phi");
    check_str("op name: alloca", ir_op_name(IR_ALLOCA), "alloca");
    check_str("op name: br", ir_op_name(IR_BR), "br");
    check_str("op name: memcpy", ir_op_name(IR_MEMCPY), "memcpy");
}

int main(void)
{
    test_op_names();
    test_diamond();
    test_phi_at_merge();
    test_loop();
    test_escaping_slot_is_not_promoted();
    test_unreachable_block_is_dropped();
    test_verifier_catches_a_broken_use();
    ty_cleanup();
    return unit_report("IR");
}
