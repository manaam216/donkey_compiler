/*
 * The IR itself: creating functions, blocks, values and instructions, and the
 * few questions that are answered by looking at an instruction rather than by
 * running an analysis.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
#include "support/mem.h"

struct ir_func *ir_func_new(struct ir_program *program, const char *name,
    struct Symbol *sym)
{
    struct ir_func *func = xcalloc(1, sizeof(*func), "IR function");

    func->name = xstrdup(name, "IR function name");
    func->sym = sym;
    func->return_type = sym && sym->ty ? sym->ty : NULL;

    if (program->last) {
        program->last->next = func;
    } else {
        program->first = func;
    }
    program->last = func;
    return func;
}

struct ir_block *ir_block_new(struct ir_func *func, const char *label)
{
    struct ir_block *block = xcalloc(1, sizeof(*block), "IR block");

    block->id = func->block_count++;
    block->label = label ? xstrdup(label, "IR block label") : NULL;
    block->rpo_index = -1;

    if (func->last_block) {
        func->last_block->next = block;
    } else {
        func->entry = block;
    }
    func->last_block = block;
    return block;
}

struct ir_value *ir_value_new(struct ir_func *func, struct Type *ty)
{
    struct ir_value *value = xcalloc(1, sizeof(*value), "IR value");

    value->id = func->value_count;
    value->ty = ty;

    grow_array((void **)&func->values, func->value_count, &func->value_capacity,
        sizeof(*func->values), 64, "IR value table");
    func->values[func->value_count++] = value;
    return value;
}

struct ir_instr *ir_emit(struct ir_block *block, IROp op)
{
    struct ir_instr *instr = xcalloc(1, sizeof(*instr), "IR instruction");

    instr->op = op;
    instr->block = block;
    instr->prev = block->last;

    if (block->last) {
        block->last->next = instr;
    } else {
        block->first = instr;
    }
    block->last = instr;
    return instr;
}

struct ir_instr *ir_emit_front(struct ir_block *block, IROp op)
{
    struct ir_instr *instr = xcalloc(1, sizeof(*instr), "IR instruction");

    instr->op = op;
    instr->block = block;
    instr->next = block->first;

    if (block->first) {
        block->first->prev = instr;
    } else {
        block->last = instr;
    }
    block->first = instr;
    return instr;
}

void ir_add_arg(struct ir_instr *instr, struct ir_value *value)
{
    grow_array((void **)&instr->args, instr->arg_count, &instr->arg_capacity,
        sizeof(*instr->args), 4, "IR operand list");
    instr->args[instr->arg_count++] = value;
}

void ir_add_phi_arg(struct ir_instr *instr, struct ir_value *value,
    struct ir_block *from)
{
    int capacity_before = instr->arg_capacity;

    grow_array((void **)&instr->args, instr->arg_count, &instr->arg_capacity,
        sizeof(*instr->args), 4, "IR operand list");
    /* The two arrays are indexed together, so the second grows to match. */
    grow_array((void **)&instr->phi_blocks, instr->arg_count, &capacity_before,
        sizeof(*instr->phi_blocks), 4, "IR phi edge list");

    instr->phi_blocks[instr->arg_count] = from;
    instr->args[instr->arg_count++] = value;
}

struct ir_value *ir_set_dst(struct ir_func *func, struct ir_instr *instr,
    struct Type *ty)
{
    instr->dst = ir_value_new(func, ty);
    instr->dst->def = instr;
    return instr->dst;
}

void ir_unlink(struct ir_instr *instr)
{
    struct ir_block *block = instr->block;

    if (instr->prev) {
        instr->prev->next = instr->next;
    } else if (block) {
        block->first = instr->next;
    }
    if (instr->next) {
        instr->next->prev = instr->prev;
    } else if (block) {
        block->last = instr->prev;
    }
    instr->prev = NULL;
    instr->next = NULL;
    instr->block = NULL;
}

void ir_remove(struct ir_instr *instr)
{
    struct ir_block *block = instr->block;

    if (instr->prev) {
        instr->prev->next = instr->next;
    } else if (block) {
        block->first = instr->next;
    }
    if (instr->next) {
        instr->next->prev = instr->prev;
    } else if (block) {
        block->last = instr->prev;
    }

    /*
     * Unlinked but not freed. A pass mid-rewrite may still hold the result
     * value, and a use that has not been updated yet would follow a dangling
     * def pointer. The block keeps it until the function goes away.
     */
    instr->prev = NULL;
    instr->next = block ? block->dead : NULL;
    instr->block = NULL;
    if (block) {
        block->dead = instr;
    }
}

struct ir_instr *ir_emit_before(struct ir_instr *at, IROp op)
{
    struct ir_instr *instr = xcalloc(1, sizeof(*instr), "IR instruction");

    instr->op = op;
    instr->block = at->block;
    instr->prev = at->prev;
    instr->next = at;

    if (at->prev) {
        at->prev->next = instr;
    } else if (at->block) {
        at->block->first = instr;
    }
    at->prev = instr;
    return instr;
}

int ir_replace_uses(struct ir_func *func, struct ir_value *from,
    struct ir_value *to)
{
    struct ir_block *block;
    int replaced = 0;

    if (from == to) {
        return 0;
    }
    for (block = func->entry; block; block = block->next) {
        struct ir_instr *instr;

        for (instr = block->first; instr; instr = instr->next) {
            int i;

            for (i = 0; i < instr->arg_count; i++) {
                if (instr->args[i] == from) {
                    instr->args[i] = to;
                    replaced++;
                }
            }
        }
    }
    return replaced;
}

void ir_count_uses(struct ir_func *func, int *counts)
{
    struct ir_block *block;
    int i;

    for (i = 0; i < func->value_count; i++) {
        counts[i] = 0;
    }
    for (block = func->entry; block; block = block->next) {
        struct ir_instr *instr;

        for (instr = block->first; instr; instr = instr->next) {
            for (i = 0; i < instr->arg_count; i++) {
                if (instr->args[i]) {
                    counts[instr->args[i]->id]++;
                }
            }
        }
    }
}

int ir_has_side_effects(struct ir_instr *instr)
{
    switch (instr->op) {
        case IR_STORE:
        case IR_MEMCPY:
        case IR_CALL:
        case IR_PARAM:
            /*
             * A parameter describes the function's interface rather than
             * computing anything, so it stays even when the body ignores it.
             */
            return 1;
        default:
            return ir_is_terminator(instr->op);
    }
}

int ir_is_pure(struct ir_instr *instr)
{
    if (ir_has_side_effects(instr) || !instr->dst) {
        return 0;
    }
    switch (instr->op) {
        case IR_LOAD:
            /* Memory can change between two loads of the same address. */
            return 0;
        case IR_ALLOCA:
            /* Two allocas are two distinct slots however alike they look. */
            return 0;
        case IR_PHI:
            /*
             * A phi's value depends on the edge control arrived along, so two
             * phis are equal only if they are in the same block -- which the
             * callers that care check for themselves.
             */
            return 0;
        default:
            return 1;
    }
}

void ir_fix_phis(struct ir_func *func)
{
    struct ir_block *block;

    for (block = func->entry; block; block = block->next) {
        struct ir_instr *instr;

        if (block->rpo_index < 0) {
            continue;
        }
        for (instr = block->first; instr; instr = instr->next) {
            int kept = 0;
            int i;

            if (instr->op != IR_PHI) {
                break;          /* phis are all at the top of the block */
            }
            for (i = 0; i < instr->arg_count; i++) {
                struct ir_block *from = instr->phi_blocks[i];
                int p;

                for (p = 0; p < block->pred_count; p++) {
                    if (block->preds[p] == from) {
                        break;
                    }
                }
                if (p == block->pred_count) {
                    continue;   /* the edge is gone, and so is its value */
                }
                instr->args[kept] = instr->args[i];
                instr->phi_blocks[kept] = from;
                kept++;
            }
            instr->arg_count = kept;
        }
    }
}

int ir_is_terminator(IROp op)
{
    return op == IR_JMP || op == IR_BR || op == IR_RET;
}

struct ir_instr *ir_terminator(struct ir_block *block)
{
    if (block->last && ir_is_terminator(block->last->op)) {
        return block->last;
    }
    return NULL;
}

int ir_successors(struct ir_block *block, struct ir_block *out[2])
{
    struct ir_instr *terminator = ir_terminator(block);

    if (!terminator) {
        return 0;
    }
    if (terminator->op == IR_JMP) {
        out[0] = terminator->target;
        return 1;
    }
    if (terminator->op == IR_BR) {
        out[0] = terminator->then_block;
        out[1] = terminator->else_block;

        /*
         * A branch whose arms are the same block is one edge, not two. Left as
         * two, the block would appear twice in the predecessor list and every
         * phi would need a duplicate argument for an edge that cannot be told
         * apart from its twin.
         */
        return out[0] == out[1] ? 1 : 2;
    }
    return 0;                   /* IR_RET leaves the function */
}

static void free_instr(struct ir_instr *instr)
{
    free(instr->args);
    free(instr->phi_blocks);
    free(instr->text);
    free(instr);
}

static void free_block(struct ir_block *block)
{
    struct ir_instr *instr = block->first;

    while (instr) {
        struct ir_instr *next = instr->next;

        free_instr(instr);
        instr = next;
    }
    instr = block->dead;
    while (instr) {
        struct ir_instr *next = instr->next;

        free_instr(instr);
        instr = next;
    }

    free(block->preds);
    free(block->frontier);
    free(block->label);
    free(block);
}

static void free_func(struct ir_func *func)
{
    struct ir_block *block = func->entry;
    int i;

    while (block) {
        struct ir_block *next = block->next;

        free_block(block);
        block = next;
    }
    for (i = 0; i < func->value_count; i++) {
        free(func->values[i]);
    }
    free(func->values);
    free(func->rpo);
    free(func->name);
    free(func);
}

void ir_program_free(struct ir_program *program)
{
    struct ir_func *func = program->first;

    while (func) {
        struct ir_func *next = func->next;

        free_func(func);
        func = next;
    }
    program->first = NULL;
    program->last = NULL;
}
