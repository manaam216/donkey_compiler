/*
 * SSA construction: frame slots become values, and phis appear where control
 * flow merges.
 *
 * Lowering put every local in memory, so the IR arrives full of loads and
 * stores that only exist because the front end had nowhere else to put a
 * variable. This pass takes them back out again.
 *
 * A slot can be promoted when every use of its address is a load or a store
 * through it. If the address escapes -- passed to a function, stored somewhere,
 * used in arithmetic -- then something outside this function's view could write
 * it, so the definitions reaching a load are not all visible here and the slot
 * stays in memory. Aggregates stay too: a struct is not a value.
 *
 * The algorithm is Cytron's. For each promotable slot:
 *
 *   1. Find the blocks that store to it.
 *   2. Place a phi in the iterated dominance frontier of that set. The
 *      frontier is where the slot's value stops being the only one that could
 *      have arrived, so it is exactly where a merge is needed. Iterating
 *      matters because a phi is itself a definition, which can force another
 *      phi further down.
 *   3. Walk the dominator tree keeping a stack of the value currently in the
 *      slot. A load becomes whatever is on top; a store pushes; a phi in a
 *      successor takes the top as the argument for this edge.
 *
 * Walking the dominator tree rather than the CFG is what makes step 3 correct:
 * a definition is visible exactly where it dominates, so the stack discipline
 * of the tree walk matches the scope of the definition.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
#include "support/mem.h"

struct promotion {
    struct ir_instr *alloca;
    struct Type *ty;

    /* The phi placed in each block for this slot, indexed by block id. */
    struct ir_instr **phis;

    /* The definitions in scope during the rename walk, innermost on top. */
    struct ir_value **stack;
    int depth;
    int capacity;

    /*
     * The value a load sees before anything has been stored. Reading an
     * uninitialised variable is undefined in C, but the IR still has to name
     * something, so it names a zero.
     */
    struct ir_value *undefined;
};

struct ssa_ctx {
    struct ir_func *func;

    /*
     * How many values existed before this pass began. The tables below are
     * indexed by value id and sized to that, because everything created after
     * it -- phis, and the zero an uninitialised read sees -- is neither a
     * promoted slot nor a removed load, so it never needs an entry.
     */
    int value_limit;

    struct promotion *promotions;
    int promotion_count;

    /* Which promotion an alloca's result belongs to, indexed by value id. */
    int *promotion_of_value;

    /*
     * What each removed load's result became. Uses are not rewritten as they
     * are found -- an operand can be read before the instruction that defines
     * it has been visited -- so replacements are recorded here and applied in
     * one pass at the end.
     */
    struct ir_value **replacement;

    /* Children in the dominator tree, so the rename walk can descend it. */
    struct ir_block ***dom_children;
    int *dom_child_count;
};

/*
 * A slot is promotable when its address never leaves the load/store pair. Any
 * other use -- as a call argument, as the value side of a store, as an operand
 * to arithmetic -- means something else can reach the memory.
 */
static int is_promotable(struct ir_func *func, struct ir_instr *alloca)
{
    struct ir_block *block;
    struct ir_value *address = alloca->dst;
    struct Type *ty = address->ty ? address->ty->base : NULL;

    if (!ty || ty->kind == TY_STRUCT || ty->kind == TY_ARRAY) {
        return 0;
    }

    for (block = func->entry; block; block = block->next) {
        struct ir_instr *instr;

        for (instr = block->first; instr; instr = instr->next) {
            int i;

            for (i = 0; i < instr->arg_count; i++) {
                if (instr->args[i] != address) {
                    continue;
                }
                if (instr->op == IR_LOAD && i == 0) {
                    continue;
                }
                if (instr->op == IR_STORE && i == 0) {
                    continue;
                }
                return 0;
            }
        }
    }
    return 1;
}

static void push_def(struct promotion *promotion, struct ir_value *value)
{
    grow_array((void **)&promotion->stack, promotion->depth,
        &promotion->capacity, sizeof(*promotion->stack), 8,
        "SSA definition stack");
    promotion->stack[promotion->depth++] = value;
}

static struct ir_value *current_def(struct ssa_ctx *ctx,
    struct promotion *promotion)
{
    if (promotion->depth > 0) {
        return promotion->stack[promotion->depth - 1];
    }
    if (!promotion->undefined) {
        struct ir_instr *zero = ir_emit_front(ctx->func->entry, IR_CONST);

        zero->imm = 0;
        promotion->undefined = ir_set_dst(ctx->func, zero, promotion->ty);
    }
    return promotion->undefined;
}

/* Which promotion an address belongs to, or -1 if it is not a promoted slot. */
static int promotion_index(struct ssa_ctx *ctx, struct ir_value *address)
{
    if (!address || address->id >= ctx->value_limit) {
        return -1;
    }
    return ctx->promotion_of_value[address->id];
}

/* ------------------------------------------------------- phi placement -- */

static void place_phis(struct ssa_ctx *ctx, struct promotion *promotion)
{
    struct ir_func *func = ctx->func;
    struct ir_block **worklist =
        xmalloc((size_t)func->block_count * sizeof(*worklist), "SSA worklist");
    char *on_worklist = xcalloc((size_t)func->block_count, 1, "SSA worklist marks");
    char *has_phi = xcalloc((size_t)func->block_count, 1, "SSA phi marks");
    struct ir_block *block;
    int count = 0;

    for (block = func->entry; block; block = block->next) {
        struct ir_instr *instr;

        if (block->rpo_index < 0) {
            continue;
        }
        for (instr = block->first; instr; instr = instr->next) {
            if (instr->op == IR_STORE &&
                promotion_index(ctx, instr->args[0]) ==
                    (int)(promotion - ctx->promotions)) {
                if (!on_worklist[block->id]) {
                    on_worklist[block->id] = 1;
                    worklist[count++] = block;
                }
                break;
            }
        }
    }

    while (count > 0) {
        struct ir_block *defining = worklist[--count];
        int i;

        for (i = 0; i < defining->frontier_count; i++) {
            struct ir_block *merge = defining->frontier[i];
            struct ir_instr *phi;

            if (has_phi[merge->id]) {
                continue;
            }
            has_phi[merge->id] = 1;

            phi = ir_emit_front(merge, IR_PHI);
            ir_set_dst(func, phi, promotion->ty);
            promotion->phis[merge->id] = phi;

            /*
             * The phi is itself a definition of the slot, so the block it
             * lands in joins the worklist. Without this a value merged once
             * would not be merged again further down.
             */
            if (!on_worklist[merge->id]) {
                on_worklist[merge->id] = 1;
                worklist[count++] = merge;
            }
        }
    }

    free(worklist);
    free(on_worklist);
    free(has_phi);
}

/* ----------------------------------------------------------- renaming -- */

static void rename_block(struct ssa_ctx *ctx, struct ir_block *block)
{
    struct ir_instr *instr;
    struct ir_instr *next;
    struct ir_block *successors[2];
    int *pushed = xcalloc((size_t)(ctx->promotion_count + 1), sizeof(int),
        "SSA rename depths");
    int successor_count;
    int i;

    /* A phi at the top of this block defines the slot from here down. */
    for (i = 0; i < ctx->promotion_count; i++) {
        struct ir_instr *phi = ctx->promotions[i].phis[block->id];

        if (phi) {
            push_def(&ctx->promotions[i], phi->dst);
            pushed[i]++;
        }
    }

    for (instr = block->first; instr; instr = next) {
        int index;

        next = instr->next;

        if (instr->op == IR_LOAD) {
            index = promotion_index(ctx, instr->args[0]);
            if (index >= 0) {
                ctx->replacement[instr->dst->id] =
                    current_def(ctx, &ctx->promotions[index]);
                ir_remove(instr);
            }
            continue;
        }
        if (instr->op == IR_STORE) {
            index = promotion_index(ctx, instr->args[0]);
            if (index >= 0) {
                push_def(&ctx->promotions[index], instr->args[1]);
                pushed[index]++;
                ir_remove(instr);
            }
            continue;
        }
    }

    /* Tell each successor's phis what arrives along the edge from here. */
    successor_count = ir_successors(block, successors);
    for (i = 0; i < successor_count; i++) {
        int p;

        for (p = 0; p < ctx->promotion_count; p++) {
            struct ir_instr *phi = ctx->promotions[p].phis[successors[i]->id];

            if (phi) {
                ir_add_phi_arg(phi, current_def(ctx, &ctx->promotions[p]), block);
            }
        }
    }

    for (i = 0; i < ctx->dom_child_count[block->id]; i++) {
        rename_block(ctx, ctx->dom_children[block->id][i]);
    }

    /*
     * Leaving the block ends the scope of everything defined in it. The stack
     * returns to what it was, which is what the sibling subtree must see.
     */
    for (i = 0; i < ctx->promotion_count; i++) {
        ctx->promotions[i].depth -= pushed[i];
    }
    free(pushed);
}

/*
 * Resolve every operand through the replacement table. A removed load's value
 * may itself have been the value a store pushed, so a replacement can point at
 * another replacement and the chain is followed to the end.
 */
static struct ir_value *resolve(struct ssa_ctx *ctx, struct ir_value *value)
{
    while (value && value->id < ctx->value_limit &&
           ctx->replacement[value->id]) {
        value = ctx->replacement[value->id];
    }
    return value;
}

static void apply_replacements(struct ssa_ctx *ctx)
{
    struct ir_block *block;

    for (block = ctx->func->entry; block; block = block->next) {
        struct ir_instr *instr;

        for (instr = block->first; instr; instr = instr->next) {
            int i;

            for (i = 0; i < instr->arg_count; i++) {
                instr->args[i] = resolve(ctx, instr->args[i]);
            }
        }
    }
}

/* ------------------------------------------------------------- driver -- */

static void build_dom_children(struct ssa_ctx *ctx)
{
    struct ir_func *func = ctx->func;
    struct ir_block *block;

    ctx->dom_children = xcalloc((size_t)func->block_count,
        sizeof(*ctx->dom_children), "SSA dominator tree");
    ctx->dom_child_count = xcalloc((size_t)func->block_count,
        sizeof(*ctx->dom_child_count), "SSA dominator tree");

    for (block = func->entry; block; block = block->next) {
        if (block->rpo_index >= 0 && block->idom) {
            ctx->dom_child_count[block->idom->id]++;
        }
    }
    for (block = func->entry; block; block = block->next) {
        int n = ctx->dom_child_count[block->id];

        ctx->dom_children[block->id] = n
            ? xmalloc((size_t)n * sizeof(**ctx->dom_children),
                "SSA dominator tree")
            : NULL;
        ctx->dom_child_count[block->id] = 0;
    }
    for (block = func->entry; block; block = block->next) {
        if (block->rpo_index >= 0 && block->idom) {
            struct ir_block *parent = block->idom;

            ctx->dom_children[parent->id][ctx->dom_child_count[parent->id]++] =
                block;
        }
    }
}

void ir_build_ssa(struct ir_func *func)
{
    struct ssa_ctx ctx;
    struct ir_instr *instr;
    int candidate_count = 0;
    int values_before;
    int i;

    if (!func->entry) {
        return;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.func = func;

    for (instr = func->entry->first; instr; instr = instr->next) {
        if (instr->op == IR_ALLOCA && is_promotable(func, instr)) {
            candidate_count++;
        }
    }
    if (candidate_count == 0) {
        return;
    }

    /*
     * Phi placement and renaming both create values, so the tables indexed by
     * value id are sized with room to grow. Anything created after this point
     * is a phi or an undefined zero, neither of which is ever a promoted slot
     * or a removed load, so the extra entries only ever hold their zero.
     */
    values_before = func->value_count;
    ctx.value_limit = values_before;
    ctx.promotions = xcalloc((size_t)candidate_count, sizeof(*ctx.promotions),
        "SSA promotion table");
    ctx.promotion_of_value = xmalloc((size_t)values_before *
        sizeof(*ctx.promotion_of_value), "SSA slot map");
    for (i = 0; i < values_before; i++) {
        ctx.promotion_of_value[i] = -1;
    }

    for (instr = func->entry->first; instr; instr = instr->next) {
        struct promotion *promotion;

        if (instr->op != IR_ALLOCA || !is_promotable(func, instr)) {
            continue;
        }
        promotion = &ctx.promotions[ctx.promotion_count];
        promotion->alloca = instr;
        promotion->ty = instr->dst->ty->base;
        promotion->phis = xcalloc((size_t)func->block_count,
            sizeof(*promotion->phis), "SSA phi table");
        ctx.promotion_of_value[instr->dst->id] = ctx.promotion_count;
        ctx.promotion_count++;
    }

    for (i = 0; i < ctx.promotion_count; i++) {
        place_phis(&ctx, &ctx.promotions[i]);
    }

    build_dom_children(&ctx);

    ctx.replacement = xcalloc((size_t)values_before,
        sizeof(*ctx.replacement), "SSA replacement table");
    rename_block(&ctx, func->entry);
    apply_replacements(&ctx);

    /* The slots are gone: nothing loads or stores through them any more. */
    for (i = 0; i < ctx.promotion_count; i++) {
        ir_remove(ctx.promotions[i].alloca);
        free(ctx.promotions[i].phis);
        free(ctx.promotions[i].stack);
    }
    for (i = 0; i < func->block_count; i++) {
        free(ctx.dom_children[i]);
    }
    free(ctx.dom_children);
    free(ctx.dom_child_count);
    free(ctx.promotions);
    free(ctx.promotion_of_value);
    free(ctx.replacement);
}
