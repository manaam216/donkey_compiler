/*
 * Common subexpression elimination, by global value numbering over the
 * dominator tree.
 *
 * Two pure instructions compute the same thing when they have the same opcode
 * and the same operands. In SSA that comparison is pointer equality: an operand
 * names a definition, not a variable that may have been reassigned since, so
 * there is no need to prove that nothing wrote to anything in between.
 *
 * The second one can be deleted only where the first is guaranteed to have run
 * -- which is where the first dominates the second. Walking the dominator tree
 * and keeping a table that is pushed on the way down and popped on the way back
 * up makes that guarantee structural: everything in the table at any point is
 * exactly what dominates the block being visited. Nothing has to test dominance
 * directly.
 *
 * Loads are left alone. Two loads of the same address are not the same value if
 * anything could have written to it between them, and knowing that needs an
 * alias analysis this compiler does not have.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opt.h"
#include "support/mem.h"

struct available {
    struct ir_instr *instr;
};

struct cse_ctx {
    struct ir_func *func;

    /*
     * The expressions in scope, innermost last. Entries are pushed as a block
     * is entered and popped as it is left, so the stack is the path from the
     * entry to the current block through the dominator tree.
     */
    struct available *table;
    int count;
    int capacity;

    struct ir_block ***children;
    int *child_count;

    int changes;
};

/* Same opcode, same operands, same immediate: the same value. */
static int same_expression(struct ir_instr *a, struct ir_instr *b)
{
    int i;

    if (a->op != b->op || a->arg_count != b->arg_count) {
        return 0;
    }
    if (a->imm != b->imm) {
        return 0;
    }
    if (a->dst && b->dst && a->dst->ty != b->dst->ty) {
        /*
         * Same bits, different type: not a substitute. Nothing the front end
         * currently produces reaches this -- it inserts an explicit cast rather
         * than letting two operations on the same operands disagree about their
         * result type -- so this guards against a future lowering, not against
         * anything today.
         */
        return 0;
    }
    if ((a->text != NULL) != (b->text != NULL)) {
        return 0;
    }
    if (a->text && strcmp(a->text, b->text) != 0) {
        return 0;
    }
    if (a->sym != b->sym) {
        return 0;
    }
    for (i = 0; i < a->arg_count; i++) {
        if (a->args[i] != b->args[i]) {
            return 0;
        }
    }
    return 1;
}

static struct ir_instr *find_available(struct cse_ctx *ctx,
    struct ir_instr *instr)
{
    int i;

    for (i = ctx->count - 1; i >= 0; i--) {
        if (same_expression(ctx->table[i].instr, instr)) {
            return ctx->table[i].instr;
        }
    }
    return NULL;
}

static void push_available(struct cse_ctx *ctx, struct ir_instr *instr)
{
    grow_array((void **)&ctx->table, ctx->count, &ctx->capacity,
        sizeof(*ctx->table), 32, "available expressions");
    ctx->table[ctx->count++].instr = instr;
}

static void visit(struct cse_ctx *ctx, struct ir_block *block)
{
    int depth_on_entry = ctx->count;
    struct ir_instr *instr;
    struct ir_instr *next;
    int i;

    for (instr = block->first; instr; instr = next) {
        struct ir_instr *earlier;

        next = instr->next;

        if (!ir_is_pure(instr)) {
            continue;
        }
        earlier = find_available(ctx, instr);
        if (earlier) {
            ir_replace_uses(ctx->func, instr->dst, earlier->dst);
            ir_remove(instr);
            ctx->changes++;
            continue;
        }
        push_available(ctx, instr);
    }

    for (i = 0; i < ctx->child_count[block->id]; i++) {
        visit(ctx, ctx->children[block->id][i]);
    }

    /* Leaving the block ends the scope of everything it made available. */
    ctx->count = depth_on_entry;
}

static void build_children(struct cse_ctx *ctx)
{
    struct ir_func *func = ctx->func;
    struct ir_block *block;

    ctx->children = xcalloc((size_t)func->block_count, sizeof(*ctx->children),
        "dominator tree");
    ctx->child_count = xcalloc((size_t)func->block_count,
        sizeof(*ctx->child_count), "dominator tree");

    for (block = func->entry; block; block = block->next) {
        if (block->rpo_index >= 0 && block->idom) {
            ctx->child_count[block->idom->id]++;
        }
    }
    for (block = func->entry; block; block = block->next) {
        int n = ctx->child_count[block->id];

        ctx->children[block->id] = n
            ? xmalloc((size_t)n * sizeof(**ctx->children), "dominator tree")
            : NULL;
        ctx->child_count[block->id] = 0;
    }
    for (block = func->entry; block; block = block->next) {
        if (block->rpo_index >= 0 && block->idom) {
            struct ir_block *parent = block->idom;

            ctx->children[parent->id][ctx->child_count[parent->id]++] = block;
        }
    }
}

int opt_cse(struct ir_func *func)
{
    struct cse_ctx ctx;
    int i;

    if (!func->entry || func->rpo_count == 0) {
        return 0;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.func = func;
    build_children(&ctx);
    visit(&ctx, func->entry);

    for (i = 0; i < func->block_count; i++) {
        free(ctx.children[i]);
    }
    free(ctx.children);
    free(ctx.child_count);
    free(ctx.table);
    return ctx.changes;
}
