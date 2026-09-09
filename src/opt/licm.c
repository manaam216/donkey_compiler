/*
 * Loop-invariant code motion.
 *
 * A computation inside a loop whose operands never change within it produces
 * the same answer every iteration, so it can be done once before the loop
 * instead of once per turn. That is the largest win available at this level:
 * it moves work out of the part of the program that runs the most.
 *
 * Finding the loops:
 *
 * A back edge is an edge b -> h where h dominates b. Control can only get to b
 * by going through h, so following the edge means going round again -- that is
 * what a loop is, and it needs no pattern matching against `while` or `for`.
 * The loop's body is h plus every block that can reach b without passing
 * through h, found by walking predecessors backwards from b.
 *
 * Where the code goes:
 *
 * Not into a preheader created for the purpose, but into h's immediate
 * dominator, before its terminator. h's immediate dominator is outside the loop
 * -- if it were inside, it would be reachable only through h, and it could not
 * then dominate h -- and it dominates every block of the loop. So a value
 * computed there is available everywhere the loop can use it.
 *
 * What may move:
 *
 * Only operations that cannot fault. The loop may run zero times, and the block
 * it is hoisted into always runs, so a division moved out of a loop that never
 * executes would trap where the original program did not. Loads do not move
 * either: something in the loop may write to the address.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opt.h"
#include "support/mem.h"

/*
 * Whether an instruction can be executed when the original program might not
 * have executed it. Division and remainder can trap on zero; loads depend on
 * memory that the loop may change; everything else is arithmetic on values.
 */
static int is_safe_to_speculate(struct ir_instr *instr)
{
    if (!ir_is_pure(instr)) {
        return 0;
    }
    switch (instr->op) {
        case IR_DIV:
        case IR_MOD:
            return 0;
        case IR_ALLOCA:
        case IR_PARAM:
            return 0;           /* these belong where lowering put them */
        default:
            return 1;
    }
}

/*
 * The blocks of the natural loop of the back edge tail -> header: the header
 * itself, plus everything that reaches the tail without going through the
 * header. Marked into `in_loop`, indexed by block id.
 */
static void collect_loop(struct ir_block *header, struct ir_block *tail,
    char *in_loop, struct ir_block **stack)
{
    int depth = 0;

    in_loop[header->id] = 1;
    if (tail != header) {
        in_loop[tail->id] = 1;
        stack[depth++] = tail;
    }

    while (depth > 0) {
        struct ir_block *block = stack[--depth];
        int i;

        for (i = 0; i < block->pred_count; i++) {
            struct ir_block *pred = block->preds[i];

            if (!in_loop[pred->id]) {
                in_loop[pred->id] = 1;
                stack[depth++] = pred;
            }
        }
    }
}

/* A value is invariant when nothing that defines it lives inside the loop. */
static int defined_outside(struct ir_value *value, const char *in_loop)
{
    if (!value || !value->def || !value->def->block) {
        return 1;               /* a parameter, or already hoisted out */
    }
    return !in_loop[value->def->block->id];
}

static int hoist_from_loop(struct ir_func *func, struct ir_block *header,
    const char *in_loop)
{
    struct ir_block *target = header->idom;
    struct ir_instr *anchor;
    struct ir_block *block;
    int changes = 0;
    int moved_this_round;

    if (!target || in_loop[target->id]) {
        return 0;
    }
    anchor = ir_terminator(target);
    if (!anchor) {
        return 0;
    }

    /*
     * Repeat until nothing more moves: hoisting one instruction can make
     * another invariant, since its operand is now defined outside.
     */
    do {
        moved_this_round = 0;

        for (block = func->entry; block; block = block->next) {
            struct ir_instr *instr;
            struct ir_instr *next;

            if (block->rpo_index < 0 || !in_loop[block->id]) {
                continue;
            }
            for (instr = block->first; instr; instr = next) {
                struct ir_instr *moved;
                int invariant = 1;
                int i;

                next = instr->next;

                if (!is_safe_to_speculate(instr)) {
                    continue;
                }
                for (i = 0; i < instr->arg_count; i++) {
                    if (!defined_outside(instr->args[i], in_loop)) {
                        invariant = 0;
                        break;
                    }
                }
                if (!invariant) {
                    continue;
                }

                /*
                 * Relinked rather than rebuilt, so the result value keeps its
                 * identity and every use of it stays valid without a rewrite.
                 */
                ir_remove(instr);
                moved = ir_emit_before(anchor, instr->op);
                moved->dst = instr->dst;
                moved->args = instr->args;
                moved->arg_count = instr->arg_count;
                moved->arg_capacity = instr->arg_capacity;
                moved->imm = instr->imm;
                moved->text = instr->text;
                moved->sym = instr->sym;
                moved->mem_type = instr->mem_type;
                moved->location = instr->location;
                if (moved->dst) {
                    moved->dst->def = moved;
                }

                /* The husk left behind must not free what was handed over. */
                instr->args = NULL;
                instr->arg_count = 0;
                instr->arg_capacity = 0;
                instr->text = NULL;
                instr->dst = NULL;

                moved_this_round++;
            }
        }
        changes += moved_this_round;
    } while (moved_this_round > 0);

    return changes;
}

int opt_licm(struct ir_func *func)
{
    char *in_loop;
    struct ir_block **stack;
    struct ir_block *block;
    int changes = 0;

    if (!func->entry || func->block_count == 0) {
        return 0;
    }
    in_loop = xmalloc((size_t)func->block_count, "loop membership");
    stack = xmalloc((size_t)func->block_count * sizeof(*stack), "loop worklist");

    /*
     * Loops are visited by header. A header with two back edges is one loop
     * with two ways round, so the memberships are collected together before
     * anything is hoisted.
     */
    for (block = func->entry; block; block = block->next) {
        int has_back_edge = 0;
        int i;

        if (block->rpo_index < 0) {
            continue;
        }
        memset(in_loop, 0, (size_t)func->block_count);

        for (i = 0; i < block->pred_count; i++) {
            struct ir_block *pred = block->preds[i];

            /*
             * The edge goes backwards exactly when its target dominates its
             * source: control cannot have reached the source except through
             * the target.
             */
            if (pred->rpo_index >= 0 && ir_dominates(block, pred)) {
                collect_loop(block, pred, in_loop, stack);
                has_back_edge = 1;
            }
        }
        if (has_back_edge) {
            changes += hoist_from_loop(func, block, in_loop);
        }
    }

    free(in_loop);
    free(stack);
    return changes;
}
