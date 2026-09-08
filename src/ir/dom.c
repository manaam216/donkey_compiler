/*
 * Dominators and dominance frontiers.
 *
 * A block A dominates B when every path from the entry to B goes through A.
 * That is the property SSA is built on: a definition may only be used where it
 * is guaranteed to have run, which is exactly where it dominates.
 *
 * The immediate dominator of B is the closest such A other than B itself, and
 * following idom pointers upward gives a tree rooted at the entry.
 *
 * The dominance frontier of A is the set of blocks where A's influence stops:
 * blocks that A does not strictly dominate, but which have a predecessor A
 * does dominate. It is the first place a value defined in A can meet a value
 * that arrived some other way -- so it is precisely where a phi is needed.
 *
 * Both use the Cooper, Harvey and Kennedy formulation: iterate to a fixed
 * point over blocks in reverse postorder, representing the dominator tree by
 * parent pointers and intersecting two blocks by walking both up the tree
 * until they meet. It is simple enough to read, and on the shapes real
 * functions have it converges in a couple of passes.
 */
#include <stdio.h>
#include <stdlib.h>
#include "ir.h"
#include "support/mem.h"

/*
 * The nearest common ancestor of two blocks in the partly-built tree. Walking
 * up from the block with the larger reverse-postorder number moves toward the
 * entry, so the two walks meet at their common dominator.
 */
static struct ir_block *intersect(struct ir_block *a, struct ir_block *b)
{
    while (a != b) {
        while (a->rpo_index > b->rpo_index) {
            a = a->idom;
        }
        while (b->rpo_index > a->rpo_index) {
            b = b->idom;
        }
    }
    return a;
}

void ir_compute_dominators(struct ir_func *func)
{
    int changed = 1;
    int i;

    for (i = 0; i < func->rpo_count; i++) {
        func->rpo[i]->idom = NULL;
    }
    if (func->rpo_count == 0) {
        return;
    }

    /* The entry dominates itself; that is the base the rest is derived from. */
    func->entry->idom = func->entry;

    while (changed) {
        changed = 0;

        for (i = 1; i < func->rpo_count; i++) {     /* skip the entry */
            struct ir_block *block = func->rpo[i];
            struct ir_block *new_idom = NULL;
            int p;

            for (p = 0; p < block->pred_count; p++) {
                struct ir_block *pred = block->preds[p];

                /*
                 * A predecessor with no dominator yet has not been processed in
                 * this pass. Skipping it is safe: reverse postorder guarantees
                 * at least one predecessor is already done unless the only way
                 * in is a back edge, and the next iteration will pick it up.
                 */
                if (!pred->idom) {
                    continue;
                }
                new_idom = new_idom ? intersect(pred, new_idom) : pred;
            }

            if (new_idom && block->idom != new_idom) {
                block->idom = new_idom;
                changed = 1;
            }
        }
    }

    /*
     * The entry's self-dominance was scaffolding for the intersection walk.
     * Clearing it now makes "no immediate dominator" mean the root, so walking
     * the tree upward terminates rather than spinning on the entry.
     */
    func->entry->idom = NULL;
}

int ir_dominates(struct ir_block *a, struct ir_block *b)
{
    while (b) {
        if (a == b) {
            return 1;
        }
        b = b->idom;
    }
    return 0;
}

static void add_frontier(struct ir_block *block, struct ir_block *member)
{
    int i;

    for (i = 0; i < block->frontier_count; i++) {
        if (block->frontier[i] == member) {
            return;
        }
    }
    grow_array((void **)&block->frontier, block->frontier_count,
        &block->frontier_capacity, sizeof(*block->frontier), 4,
        "IR dominance frontier");
    block->frontier[block->frontier_count++] = member;
}

void ir_compute_frontiers(struct ir_func *func)
{
    int i;

    for (i = 0; i < func->rpo_count; i++) {
        func->rpo[i]->frontier_count = 0;
    }

    /*
     * Only a block with more than one predecessor can be on any frontier: with
     * a single way in, whatever dominates the predecessor dominates the block
     * too, so nothing stops there.
     *
     * For each such merge point, walk up from every predecessor towards the
     * merge's own immediate dominator. Each block passed on the way dominates
     * that predecessor but not the merge, so the merge is on its frontier.
     */
    for (i = 0; i < func->rpo_count; i++) {
        struct ir_block *block = func->rpo[i];
        int p;

        if (block->pred_count < 2) {
            continue;
        }
        for (p = 0; p < block->pred_count; p++) {
            struct ir_block *runner = block->preds[p];

            while (runner && runner != block->idom) {
                add_frontier(runner, block);
                runner = runner->idom;
            }
        }
    }
}
