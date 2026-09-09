/*
 * Dead code elimination, and copy propagation.
 *
 * Both are one line of reasoning in SSA and are kept together because each
 * exposes work for the other: propagating a copy drops the last use of the
 * thing copied, and deleting that may make another copy trivial.
 *
 * Dead code is a use count. A value is written exactly once, so an instruction
 * whose result nothing reads and which has no side effect cannot matter --
 * there is no other way to observe it. Removing it can drop the last use of
 * its operands, so the count is recomputed and the sweep repeated until
 * nothing more goes.
 *
 * This is what cleans up after promotion. The `&&` and `?:` temporaries become
 * phis whether or not the result is used, and an unread local becomes a phi
 * with no readers at all.
 */
#include <stdio.h>
#include <stdlib.h>
#include "opt.h"
#include "support/mem.h"

int opt_dce(struct ir_func *func)
{
    int *uses;
    int changes = 0;
    int removed;

    if (func->value_count == 0) {
        return 0;
    }
    uses = xmalloc((size_t)func->value_count * sizeof(*uses), "use counts");

    do {
        struct ir_block *block;

        removed = 0;
        ir_count_uses(func, uses);

        for (block = func->entry; block; block = block->next) {
            struct ir_instr *instr;
            struct ir_instr *next;

            if (block->rpo_index < 0) {
                continue;
            }
            for (instr = block->first; instr; instr = next) {
                next = instr->next;

                if (!instr->dst || ir_has_side_effects(instr)) {
                    continue;
                }
                if (uses[instr->dst->id] != 0) {
                    continue;
                }

                /*
                 * Dropping it may have been the last use of its operands, so
                 * the counts are stale from here on. Rather than adjust them
                 * in place -- which is easy to get wrong when the same value
                 * appears twice in one instruction -- the whole sweep runs
                 * again on fresh counts.
                 */
                ir_remove(instr);
                removed++;
            }
        }
        changes += removed;
    } while (removed > 0);

    free(uses);
    return changes;
}

/*
 * A phi whose arguments all name the same value is not a choice at all. This
 * happens constantly: promotion places a phi wherever the dominance frontier
 * says one might be needed, which is a question about the shape of the graph
 * and not about whether the values actually differ. A variable assigned once
 * before an `if` gets a phi at the merge whose two arguments are the same
 * definition.
 *
 * A phi that names itself is still trivial -- `x = phi(x, y)` can only ever be
 * y -- so self-references are skipped when looking for disagreement. That is
 * what removes the phi a loop gets for a variable the loop never changes.
 */
static struct ir_value *trivial_phi_value(struct ir_instr *phi)
{
    struct ir_value *only = NULL;
    int i;

    for (i = 0; i < phi->arg_count; i++) {
        struct ir_value *arg = phi->args[i];

        if (!arg || arg == phi->dst) {
            continue;
        }
        if (!only) {
            only = arg;
        } else if (only != arg) {
            return NULL;
        }
    }
    return only;
}

int opt_propagate_copies(struct ir_func *func)
{
    int changes = 0;
    int changed_this_round;

    do {
        struct ir_block *block;

        changed_this_round = 0;

        for (block = func->entry; block; block = block->next) {
            struct ir_instr *instr;
            struct ir_instr *next;

            if (block->rpo_index < 0) {
                continue;
            }
            for (instr = block->first; instr; instr = next) {
                struct ir_value *replacement = NULL;

                next = instr->next;

                if (instr->op == IR_COPY && instr->arg_count == 1) {
                    replacement = instr->args[0];
                } else if (instr->op == IR_PHI) {
                    replacement = trivial_phi_value(instr);
                }

                if (!replacement || replacement == instr->dst) {
                    continue;
                }
                ir_replace_uses(func, instr->dst, replacement);
                ir_remove(instr);
                changed_this_round++;
            }
        }
        changes += changed_this_round;
    } while (changed_this_round > 0);

    return changes;
}
