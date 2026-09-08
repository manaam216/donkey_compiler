/*
 * The control flow graph: predecessors, reverse postorder, and which blocks
 * anything can actually reach.
 *
 * Lowering records only forward edges -- a branch names its targets -- because
 * that is all it knows as it goes. Everything after it needs the other
 * direction as well: a phi has one argument per predecessor, and dominance is
 * computed by intersecting predecessors. So the reverse edges are derived here
 * rather than maintained during lowering, where an unrecorded edge would be a
 * silent bug rather than a recomputation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
#include "support/mem.h"

static void add_pred(struct ir_block *block, struct ir_block *pred)
{
    grow_array((void **)&block->preds, block->pred_count, &block->pred_capacity,
        sizeof(*block->preds), 4, "IR predecessor list");
    block->preds[block->pred_count++] = pred;
}

/*
 * Depth-first search, recording each block after its successors. Reversing
 * that order puts every block before the ones it can reach, except around a
 * back edge -- which is exactly the order the dominator computation wants, so
 * that each block sees its predecessors already updated.
 */
static void postorder(struct ir_block *block, struct ir_block **order, int *count)
{
    struct ir_block *successors[2];
    int n, i;

    if (block->visited) {
        return;
    }
    block->visited = 1;

    n = ir_successors(block, successors);
    for (i = 0; i < n; i++) {
        postorder(successors[i], order, count);
    }
    order[(*count)++] = block;
}

void ir_analyze_cfg(struct ir_func *func)
{
    struct ir_block *block;
    struct ir_block **order;
    int count = 0;
    int i;

    for (block = func->entry; block; block = block->next) {
        block->pred_count = 0;
        block->rpo_index = -1;
        block->visited = 0;
    }

    if (!func->entry) {
        func->rpo_count = 0;
        return;
    }

    order = xmalloc((size_t)func->block_count * sizeof(*order), "IR block order");
    postorder(func->entry, order, &count);

    /*
     * Reverse in place. Blocks that the search never reached keep rpo_index
     * -1: they stay on the function's layout list so they are still freed, but
     * no pass looks at them, and nothing may branch to them since the only way
     * in would have been an edge the search would have followed.
     */
    free(func->rpo);
    func->rpo = order;
    func->rpo_count = count;
    for (i = 0; i < count / 2; i++) {
        struct ir_block *swap = order[i];

        order[i] = order[count - 1 - i];
        order[count - 1 - i] = swap;
    }
    for (i = 0; i < count; i++) {
        order[i]->rpo_index = i;
    }

    /*
     * Nothing can run in a block the search never reached, so its instructions
     * are dropped. They are not merely ignored: leaving them in place would let
     * a later pass see a load from a slot it has just promoted away, and report
     * a problem in code that cannot execute. The block itself stays on the
     * layout list so it is still freed.
     */
    for (block = func->entry; block; block = block->next) {
        if (block->rpo_index >= 0) {
            continue;
        }
        while (block->first) {
            ir_remove(block->first);
        }
    }

    /* Now that reachability is known, record the reverse edges. */
    for (i = 0; i < count; i++) {
        struct ir_block *successors[2];
        int n = ir_successors(order[i], successors);
        int s;

        for (s = 0; s < n; s++) {
            add_pred(successors[s], order[i]);
        }
    }
}
