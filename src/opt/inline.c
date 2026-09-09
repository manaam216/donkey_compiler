/*
 * Inlining.
 *
 * Replacing a call with a copy of the callee's body removes the call itself,
 * but that is the smaller half of what it buys. The larger half is that the
 * callee's code lands in the caller's context, where every other pass can see
 * it: an argument that is a constant at this call site becomes a constant
 * inside the body, and folding, value numbering and dead code elimination then
 * do the real work. Inlining is mostly a way of giving the other passes
 * something to look at.
 *
 * The transformation, at a call `%r = call @g, a, b` in block B:
 *
 *   B is split at the call. Everything after it moves into a new block, which
 *   is where control resumes once the copied body is done.
 *
 *   g's blocks and values are copied into the caller. Its parameters are not
 *   copied at all -- each one is mapped straight onto the argument at this call
 *   site, which is what makes the constants visible.
 *
 *   Each of g's returns becomes a jump to the resumption block, and the values
 *   they returned meet in a phi there. That phi is what %r becomes.
 *
 * Copying is done in two passes. A block can refer to a value defined in a
 * block that has not been copied yet -- a phi on a back edge always does -- so
 * the first pass creates every instruction and its result, and the second fills
 * in the operands once every value has something to map to.
 *
 * What is left alone: a call through a pointer, where the callee is not known;
 * a call to a function whose argument count does not match its parameters,
 * which is how a variadic callee is recognised without asking; and recursion,
 * direct or otherwise, which has no bottom to stop at.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opt.h"
#include "support/mem.h"

/*
 * How big a callee may be and still be worth copying. Small enough that the
 * copy is comparable to the call sequence it replaces, generous enough to cover
 * the accessors and helpers where inlining pays for itself several times over
 * in what it exposes.
 */
#define INLINE_MAX_INSTRUCTIONS 48

/* A caller may not grow without bound, however many small callees it has. */
#define INLINE_MAX_GROWTH_FACTOR 4

struct inline_map {
    struct ir_value **values;   /* callee value id -> caller value */
    int value_count;
    struct ir_block **blocks;   /* callee block id -> caller block */
    int block_count;
};

static int count_instructions(struct ir_func *func)
{
    struct ir_block *block;
    int count = 0;

    for (block = func->entry; block; block = block->next) {
        struct ir_instr *instr;

        if (block->rpo_index < 0) {
            continue;
        }
        for (instr = block->first; instr; instr = instr->next) {
            count++;
        }
    }
    return count;
}

static int count_params(struct ir_func *func)
{
    struct ir_instr *instr;
    int count = 0;

    if (!func->entry) {
        return 0;
    }
    for (instr = func->entry->first; instr; instr = instr->next) {
        if (instr->op == IR_PARAM) {
            count++;
        }
    }
    return count;
}

/* The function a call names, or NULL if it is not a direct call to one. */
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
        return NULL;            /* through a pointer: not known here */
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
    return NULL;                /* declared elsewhere; there is no body to copy */
}

static struct ir_value *mapped(struct inline_map *map, struct ir_value *value)
{
    if (!value || value->id >= map->value_count) {
        return value;
    }
    return map->values[value->id] ? map->values[value->id] : value;
}

/*
 * Split a block just after `at`, moving everything that follows into a new
 * block. The call itself stays behind and is removed by the caller once its
 * result has somewhere to come from.
 */
static struct ir_block *split_after(struct ir_func *func, struct ir_block *block,
    struct ir_instr *at)
{
    struct ir_block *rest = ir_block_new(func, NULL);
    struct ir_instr *instr = at->next;

    while (instr) {
        struct ir_instr *next = instr->next;

        ir_unlink(instr);
        instr->block = rest;
        instr->prev = rest->last;
        instr->next = NULL;
        if (rest->last) {
            rest->last->next = instr;
        } else {
            rest->first = instr;
        }
        rest->last = instr;
        instr = next;
    }

    /*
     * Phis in the blocks the split half branches to still name the block the
     * instructions used to be in. Control now arrives from the new one.
     */
    {
        struct ir_block *successors[2];
        int n = ir_successors(rest, successors);
        int i;

        for (i = 0; i < n; i++) {
            struct ir_instr *phi;

            for (phi = successors[i]->first; phi && phi->op == IR_PHI;
                 phi = phi->next) {
                int a;

                for (a = 0; a < phi->arg_count; a++) {
                    if (phi->phi_blocks[a] == block) {
                        phi->phi_blocks[a] = rest;
                    }
                }
            }
        }
    }
    return rest;
}

/*
 * Copy the callee into the caller and wire it up. Returns the number of
 * instructions added, or 0 if nothing was done.
 */
static int inline_one(struct ir_func *caller, struct ir_instr *call,
    struct ir_func *callee)
{
    struct inline_map map;
    struct ir_block *call_block = call->block;
    struct ir_block *resume;
    struct ir_block *block;
    struct ir_instr *instr;
    struct ir_instr *jump;
    struct ir_instr *result_phi = NULL;
    int added = 0;
    int i;

    map.value_count = callee->value_count;
    map.block_count = callee->block_count;
    map.values = xcalloc((size_t)map.value_count + 1, sizeof(*map.values),
        "inline value map");
    map.blocks = xcalloc((size_t)map.block_count + 1, sizeof(*map.blocks),
        "inline block map");

    resume = split_after(caller, call_block, call);

    /*
     * A parameter is not copied. It becomes the argument at this call site,
     * which is the whole point: a constant passed in is a constant inside.
     */
    i = 0;
    for (instr = callee->entry->first; instr; instr = instr->next) {
        if (instr->op == IR_PARAM && instr->dst) {
            map.values[instr->dst->id] = call->args[i + 1];
            i++;
        }
    }

    /* Pass one: every block and every result value, with no operands yet. */
    for (block = callee->entry; block; block = block->next) {
        if (block->rpo_index < 0) {
            continue;
        }
        map.blocks[block->id] = ir_block_new(caller, block->label);
    }

    for (block = callee->entry; block; block = block->next) {
        if (block->rpo_index < 0) {
            continue;
        }
        for (instr = block->first; instr; instr = instr->next) {
            struct ir_instr *copy;

            if (instr->op == IR_PARAM) {
                continue;
            }

            /*
             * A frame slot belongs in the caller's entry, not in the copied
             * body: it is storage reserved once when the function is entered,
             * and a block in the middle may be reached many times.
             */
            if (instr->op == IR_ALLOCA) {
                copy = ir_emit_front(caller->entry, IR_ALLOCA);
            } else if (instr->op == IR_RET) {
                copy = ir_emit(map.blocks[block->id], IR_JMP);
                copy->target = resume;
            } else {
                copy = ir_emit(map.blocks[block->id], instr->op);
            }

            copy->imm = instr->imm;
            copy->sym = instr->sym;
            copy->mem_type = instr->mem_type;
            copy->location = instr->location;
            if (instr->text) {
                copy->text = xstrdup(instr->text, "inlined text");
            }
            if (instr->dst && instr->op != IR_RET) {
                map.values[instr->dst->id] =
                    ir_set_dst(caller, copy, instr->dst->ty);
            }
            added++;
        }
    }

    /*
     * Pass two: operands, branch targets and phi edges, now that every value
     * and block the callee mentions has something in the caller to map to.
     */
    if (call->dst) {
        result_phi = ir_emit_front(resume, IR_PHI);
        ir_set_dst(caller, result_phi, call->dst->ty);
    }

    for (block = callee->entry; block; block = block->next) {
        struct ir_instr *copy;

        if (block->rpo_index < 0) {
            continue;
        }
        copy = map.blocks[block->id]->first;

        for (instr = block->first; instr; instr = instr->next) {
            if (instr->op == IR_PARAM) {
                continue;
            }
            if (instr->op == IR_ALLOCA) {
                continue;       /* moved to the entry, and takes no operands */
            }
            if (!copy) {
                break;
            }

            if (instr->op == IR_RET) {
                if (result_phi) {
                    struct ir_value *returned;

                    if (instr->arg_count > 0) {
                        returned = mapped(&map, instr->args[0]);
                    } else {
                        /*
                         * A `return;` in a function whose result is used: the
                         * value is undefined in C, but the phi still needs an
                         * argument on this edge, so it names a zero.
                         */
                        struct ir_instr *zero =
                            ir_emit_before(copy, IR_CONST);

                        zero->imm = 0;
                        returned = ir_set_dst(caller, zero, call->dst->ty);
                    }
                    ir_add_phi_arg(result_phi, returned, map.blocks[block->id]);
                }
            } else {
                for (i = 0; i < instr->arg_count; i++) {
                    if (instr->op == IR_PHI) {
                        ir_add_phi_arg(copy, mapped(&map, instr->args[i]),
                            map.blocks[instr->phi_blocks[i]->id]);
                    } else {
                        ir_add_arg(copy, mapped(&map, instr->args[i]));
                    }
                }
                if (instr->op == IR_JMP) {
                    copy->target = map.blocks[instr->target->id];
                } else if (instr->op == IR_BR) {
                    copy->then_block = map.blocks[instr->then_block->id];
                    copy->else_block = map.blocks[instr->else_block->id];
                }
            }
            copy = copy->next;
        }
    }

    /* Control enters the copied body instead of making the call. */
    if (result_phi) {
        ir_replace_uses(caller, call->dst, result_phi->dst);
    }
    ir_remove(call);
    jump = ir_emit(call_block, IR_JMP);
    jump->target = map.blocks[callee->entry->id];

    free(map.values);
    free(map.blocks);
    return added;
}

int opt_inline(struct ir_program *program, int level)
{
    struct ir_func *caller;
    int changes = 0;

    if (level < 2) {
        return 0;
    }

    for (caller = program->first; caller; caller = caller->next) {
        struct ir_block *block;
        int budget;
        int inlined_here = 1;

        budget = count_instructions(caller) * (INLINE_MAX_GROWTH_FACTOR - 1);

        /*
         * One call is inlined per sweep, because copying a body invalidates
         * the block list being walked. Sweeping again picks up the next one,
         * including calls that came in with the body just copied.
         */
        while (inlined_here && budget > 0) {
            inlined_here = 0;

            ir_analyze_cfg(caller);

            for (block = caller->entry; block && !inlined_here;
                 block = block->next) {
                struct ir_instr *instr;

                if (block->rpo_index < 0) {
                    continue;
                }
                for (instr = block->first; instr; instr = instr->next) {
                    struct ir_func *callee;
                    int size;

                    if (instr->op != IR_CALL) {
                        continue;
                    }
                    callee = callee_of(program, instr);
                    if (!callee || callee == caller || !callee->entry) {
                        continue;
                    }

                    /*
                     * An argument count that does not match the parameters
                     * means the callee takes a variable number of them, and a
                     * copied body would read parameters that were never mapped.
                     */
                    if (instr->arg_count - 1 != count_params(callee)) {
                        continue;
                    }

                    ir_analyze_cfg(callee);
                    size = count_instructions(callee);
                    if (size > INLINE_MAX_INSTRUCTIONS || size > budget) {
                        continue;
                    }

                    budget -= inline_one(caller, instr, callee);
                    inlined_here = 1;
                    changes++;
                    break;
                }
            }
        }
    }

    return changes;
}
