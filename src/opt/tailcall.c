/*
 * Tail call elimination for self-recursion.
 *
 * A call in tail position -- one whose result is returned immediately, with
 * nothing left to do afterwards -- does not need a new frame. The caller has no
 * remaining work, so the callee can reuse the frame it is standing in.
 *
 * In general that needs the back end: reusing a frame means overwriting the
 * arguments and jumping rather than calling, and only instruction selection can
 * emit that. But when the callee is the function itself, the whole thing can be
 * done here, in the IR, with no back end support at all: recursion becomes a
 * loop. The arguments are assigned to the parameters and control jumps back to
 * the top.
 *
 * That is the case worth having anyway. It is what turns a tail-recursive
 * function from something that runs out of stack on a large input into
 * something that does not, and it is a difference in what the program can do
 * rather than only in how fast it does it.
 *
 * The shape being matched:
 *
 *     %r = call @self, a, b
 *     ret %r
 *
 * becomes a jump to a loop header whose phis choose between the original
 * parameters and a and b.
 *
 * Two conditions have to hold. Nothing may sit between the call and the return,
 * because it would still have to run after the callee came back. And the
 * function must have no frame slots left: a loop reuses one frame, so a local
 * whose address escaped into the call would be shared between iterations that
 * the original program kept apart.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opt.h"
#include "support/mem.h"

/* Whether a call names this very function rather than another one. */
static int calls_itself(struct ir_func *func, struct ir_instr *call)
{
    struct ir_instr *callee;

    if (call->arg_count < 1 || !call->args[0] || !call->args[0]->def) {
        return 0;
    }
    callee = call->args[0]->def;
    if (callee->op != IR_GLOBAL) {
        return 0;               /* through a pointer: not known to be us */
    }
    if (func->sym && callee->sym) {
        return func->sym == callee->sym;
    }
    return callee->text && strcmp(callee->text, func->name) == 0;
}

/*
 * The call a block returns the result of, or NULL.
 *
 * The call must be the instruction directly before the return: anything in
 * between is work the caller still owes, and a jump would skip it. With frame
 * slots already ruled out that intervening work can only be pure and unused,
 * so this is a second conservative guard rather than one that is currently
 * doing any work on its own.
 */
static struct ir_instr *tail_call_in(struct ir_func *func,
    struct ir_block *block)
{
    struct ir_instr *ret = ir_terminator(block);
    struct ir_instr *call;

    if (!ret || ret->op != IR_RET) {
        return NULL;
    }
    call = ret->prev;
    if (!call || call->op != IR_CALL || !calls_itself(func, call)) {
        return NULL;
    }

    /* The returned value must be the call's result, and nothing else. */
    if (ret->arg_count == 0) {
        return call->dst ? NULL : call;
    }
    return ret->args[0] == call->dst ? call : NULL;
}

/*
 * A frame that gets reused cannot hold anything the callee could still see.
 *
 * This is conservative rather than load-bearing today. A tail call has nothing
 * after it by definition, so a slot the caller wrote cannot be read again in
 * this language subset, and refusing here mostly means waiting a round for
 * promotion to remove the slot. It is the guard that would matter first if
 * anything ever took a caller frame's address across a call.
 */
static int has_frame_slots(struct ir_func *func)
{
    struct ir_block *block;

    for (block = func->entry; block; block = block->next) {
        struct ir_instr *instr;

        for (instr = block->first; instr; instr = instr->next) {
            if (instr->op == IR_ALLOCA) {
                return 1;
            }
        }
    }
    return 0;
}

/*
 * Split the entry block into a prologue and a loop header.
 *
 * The parameters stay in the entry, which now runs once; everything else moves
 * into a header the tail calls can jump back to. Splitting rather than making a
 * new entry block keeps func->entry where it is, which matters because the
 * function's blocks are freed by walking forward from it.
 */
static struct ir_block *split_entry(struct ir_func *func)
{
    struct ir_block *entry = func->entry;
    struct ir_block *header = ir_block_new(func, "tail");
    struct ir_instr *instr;
    struct ir_instr *jump;

    instr = entry->first;
    while (instr) {
        struct ir_instr *next = instr->next;

        if (instr->op != IR_PARAM) {
            ir_unlink(instr);

            instr->block = header;
            instr->prev = header->last;
            instr->next = NULL;
            if (header->last) {
                header->last->next = instr;
            } else {
                header->first = instr;
            }
            header->last = instr;
        }
        instr = next;
    }

    jump = ir_emit(entry, IR_JMP);
    jump->target = header;
    return header;
}

int opt_tail_calls(struct ir_func *func)
{
    struct ir_block *block;
    struct ir_block **tail_blocks;
    struct ir_instr **tail_calls;
    struct ir_instr *param;
    struct ir_block *header;
    int tail_count = 0;
    int param_count = 0;
    int changes = 0;
    int i;

    if (!func->entry || has_frame_slots(func)) {
        return 0;
    }

    tail_blocks = xmalloc((size_t)func->block_count * sizeof(*tail_blocks),
        "tail call blocks");
    tail_calls = xmalloc((size_t)func->block_count * sizeof(*tail_calls),
        "tail calls");

    for (block = func->entry; block; block = block->next) {
        struct ir_instr *call;

        if (block->rpo_index < 0) {
            continue;
        }
        call = tail_call_in(func, block);
        if (call) {
            tail_blocks[tail_count] = block;
            tail_calls[tail_count] = call;
            tail_count++;
        }
    }

    if (tail_count == 0) {
        free(tail_blocks);
        free(tail_calls);
        return 0;
    }

    for (param = func->entry->first; param; param = param->next) {
        if (param->op == IR_PARAM) {
            param_count++;
        }
    }

    /*
     * Every tail call must hand over exactly the arguments the function takes.
     * A mismatch means the call is to something with a different signature that
     * happens to share the name, and turning it into a jump would be wrong.
     */
    for (i = 0; i < tail_count; i++) {
        if (tail_calls[i]->arg_count - 1 != param_count) {
            free(tail_blocks);
            free(tail_calls);
            return 0;
        }
    }

    header = split_entry(func);

    /*
     * A phi per parameter, choosing between the value the function was entered
     * with and the one the tail call passes. This is the assignment that makes
     * the jump legal: without it the body would keep reading the arguments from
     * the first call.
     */
    i = 0;
    for (param = func->entry->first; param; param = param->next) {
        struct ir_instr *phi;
        int t;

        if (param->op != IR_PARAM) {
            continue;
        }

        phi = ir_emit_front(header, IR_PHI);
        ir_set_dst(func, phi, param->dst->ty);

        /*
         * Redirect the body onto the phi before wiring the phi up, so that the
         * incoming edge from the entry keeps naming the parameter itself rather
         * than being rewritten to the phi it feeds.
         */
        ir_replace_uses(func, param->dst, phi->dst);
        ir_add_phi_arg(phi, param->dst, func->entry);
        for (t = 0; t < tail_count; t++) {
            ir_add_phi_arg(phi, tail_calls[t]->args[i + 1], tail_blocks[t]);
        }
        i++;
    }

    /* The call and its return become a jump back to the top of the loop. */
    for (i = 0; i < tail_count; i++) {
        struct ir_instr *jump;

        ir_remove(ir_terminator(tail_blocks[i]));
        ir_remove(tail_calls[i]);
        jump = ir_emit(tail_blocks[i], IR_JMP);
        jump->target = header;
        changes++;
    }

    free(tail_blocks);
    free(tail_calls);
    return changes;
}
