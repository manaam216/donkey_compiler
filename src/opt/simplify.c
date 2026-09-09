/*
 * Control flow simplification.
 *
 * Lowering produces more blocks than the program needs: each arm of an `if`
 * gets one whether or not anything branches over it, `break` opens a block
 * nothing reaches, and a `for` with a constant condition still gets a test.
 * The other passes then make more of them redundant -- folding a comparison
 * turns a branch into a jump, and one side of that branch becomes unreachable.
 *
 * Three rewrites, smallest first:
 *
 *   A branch on a constant is a jump. The arm not taken loses an edge, and
 *   often its last one.
 *
 *   A block nothing reaches is dropped, which is ir_analyze_cfg's job -- so
 *   this pass runs it and then repairs the phis that named the vanished edges.
 *
 *   A block whose only predecessor has only it as a successor is merged into
 *   that predecessor. That is what turns the four blocks an `if` without an
 *   `else` lowers to back into two.
 */
#include <stdio.h>
#include <stdlib.h>
#include "opt.h"

static int is_const(struct ir_value *value)
{
    return value && value->def && value->def->op == IR_CONST;
}

/* A branch whose condition is known is a jump to the arm it would take. */
static int straighten_branches(struct ir_func *func)
{
    struct ir_block *block;
    int changes = 0;

    for (block = func->entry; block; block = block->next) {
        struct ir_instr *terminator = ir_terminator(block);

        if (block->rpo_index < 0 || !terminator) {
            continue;
        }
        if (terminator->op != IR_BR) {
            continue;
        }

        if (is_const(terminator->args[0])) {
            terminator->target = terminator->args[0]->def->imm
                ? terminator->then_block : terminator->else_block;
            terminator->op = IR_JMP;
            terminator->arg_count = 0;
            changes++;
            continue;
        }

        /*
         * Both arms leading to the same place is a jump too, and the condition
         * -- which may be an expensive comparison -- becomes dead. This turns
         * up after the arms of an `if` have both been emptied.
         */
        if (terminator->then_block == terminator->else_block) {
            terminator->target = terminator->then_block;
            terminator->op = IR_JMP;
            terminator->arg_count = 0;
            changes++;
        }
    }
    return changes;
}

/*
 * Move every instruction of `from` onto the end of `into`, which must end in a
 * jump to it. The jump goes away with the block boundary it was there to cross.
 */
static void absorb(struct ir_func *func, struct ir_block *into,
    struct ir_block *from)
{
    struct ir_instr *jump = ir_terminator(into);
    struct ir_instr *instr;
    struct ir_block *successors[2];
    int successor_count;
    int i;

    ir_remove(jump);

    while ((instr = from->first) != NULL) {
        ir_unlink(instr);

        instr->block = into;
        instr->prev = into->last;
        instr->next = NULL;
        if (into->last) {
            into->last->next = instr;
        } else {
            into->first = instr;
        }
        into->last = instr;
    }

    /*
     * Anything that merged in may be the target of a phi further down, and
     * those phis name the block the value came from. It now comes from the
     * block that absorbed it.
     */
    successor_count = ir_successors(into, successors);
    for (i = 0; i < successor_count; i++) {
        struct ir_instr *phi;

        for (phi = successors[i]->first; phi && phi->op == IR_PHI;
             phi = phi->next) {
            int a;

            for (a = 0; a < phi->arg_count; a++) {
                if (phi->phi_blocks[a] == from) {
                    phi->phi_blocks[a] = into;
                }
            }
        }
    }

    /* Emptied and detached; ir_analyze_cfg will stop reaching it. */
    from->rpo_index = -1;
    (void)func;
}

static int merge_blocks(struct ir_func *func)
{
    struct ir_block *block;
    int changes = 0;

    for (block = func->entry; block; block = block->next) {
        if (block->rpo_index < 0) {
            continue;
        }

        /*
         * Keep absorbing into the same block rather than moving on. A run of
         * blocks that each jump to the next collapses in one visit this way;
         * advancing after the first merge would fold one boundary per pass and
         * need as many rounds of the whole pipeline as the chain is long.
         */
        for (;;) {
            struct ir_block *successors[2];
            struct ir_block *only;

            if (ir_successors(block, successors) != 1) {
                break;
            }
            only = successors[0];

            /*
             * One way in and one way out, and not a self-loop -- a block that
             * jumps to itself has itself as a predecessor, and merging it would
             * mean merging it into itself forever.
             */
            if (only == block || only == func->entry || only->pred_count != 1) {
                break;
            }

            /*
             * A phi in the merged block would be choosing between edges that no
             * longer exist. With a single predecessor it should already have
             * been reduced to its one argument by copy propagation; if it has
             * not been, leave the merge for the next round rather than dropping
             * information.
             */
            if (only->first && only->first->op == IR_PHI) {
                break;
            }

            absorb(func, block, only);
            changes++;
        }
    }
    return changes;
}

int opt_simplify_cfg(struct ir_func *func)
{
    int changes = 0;
    int reachable_before = func->rpo_count;

    changes += straighten_branches(func);

    /*
     * Re-derive the graph before merging: straightening a branch can leave a
     * block with one predecessor where it had two, and merging reads
     * predecessor counts.
     */
    ir_analyze_cfg(func);
    ir_fix_phis(func);

    changes += merge_blocks(func);

    ir_analyze_cfg(func);
    ir_fix_phis(func);

    /* Blocks that stopped being reachable are a change in their own right. */
    if (func->rpo_count < reachable_before) {
        changes += reachable_before - func->rpo_count;
    }
    return changes;
}
