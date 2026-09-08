/*
 * The IR verifier.
 *
 * Every pass after this one is allowed to assume the IR is well formed, and a
 * pass that quietly breaks that assumption produces wrong code somewhere far
 * away from the mistake. Checking the invariants directly turns those into a
 * message naming the instruction.
 *
 * What is checked:
 *
 *   Each reachable block ends with exactly one terminator, and has none in the
 *   middle. A block that falls off its end has no defined successor.
 *
 *   Every operand is defined, and its definition dominates the use. This is
 *   the SSA property itself: if it holds, a value can be read wherever it is
 *   named, and no pass has to ask whether the definition ran.
 *
 *   A phi has one argument per incoming edge, each naming a real predecessor,
 *   and its arguments are checked against the end of the block they come from
 *   rather than against the phi's own position -- a phi argument is live on the
 *   edge, not at the merge.
 *
 *   Instructions have the operands their opcode requires.
 */
#include <stdio.h>
#include <stdlib.h>
#include "ir.h"

struct verify_ctx {
    struct ir_func *func;
    FILE *output;
    int problems;
};

static void problem(struct verify_ctx *ctx, struct ir_instr *instr,
    const char *message)
{
    fprintf(ctx->output, "ir: %s: ", ctx->func->name);
    if (instr && instr->block) {
        fprintf(ctx->output, "b%d: ", instr->block->id);
    }
    if (instr) {
        fprintf(ctx->output, "%s: ", ir_op_name(instr->op));
    }
    fprintf(ctx->output, "%s\n", message);
    ctx->problems++;
}

/* How many operands an opcode must have; -1 where any number is allowed. */
static int required_args(IROp op)
{
    switch (op) {
        case IR_NOP:
        case IR_CONST:
        case IR_CONST_FP:
        case IR_STR:
        case IR_GLOBAL:
        case IR_ALLOCA:
        case IR_PARAM:
        case IR_JMP:
            return 0;
        case IR_LOAD:
        case IR_COPY:
        case IR_NEG:
        case IR_COMPLEMENT:
        case IR_LOGNOT:
        case IR_CAST:
        case IR_BR:
            return 1;
        case IR_STORE:
        case IR_MEMCPY:
        case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV: case IR_MOD:
        case IR_SHL: case IR_SHR: case IR_AND: case IR_OR: case IR_XOR:
        case IR_EQ: case IR_NE: case IR_LT: case IR_LE: case IR_GT: case IR_GE:
            return 2;
        default:
            return -1;          /* IR_CALL, IR_RET, IR_PHI */
    }
}

/*
 * Whether a definition is visible at a use inside `at`. Within one block it is
 * a matter of order; across blocks it is dominance. A value with no defining
 * instruction is an incoming parameter, which is visible everywhere.
 */
static int def_reaches(struct ir_value *value, struct ir_block *at,
    struct ir_instr *use)
{
    struct ir_instr *def = value->def;
    struct ir_instr *scan;

    if (!def) {
        return 1;
    }
    if (!def->block) {
        return 0;               /* the defining instruction was removed */
    }
    if (def->block != at) {
        return ir_dominates(def->block, at);
    }

    /*
     * Same block: the definition must come first. A phi is the exception --
     * its arguments are evaluated on the incoming edges, before the block
     * starts -- but a phi's operands are checked against the predecessor, so
     * that case never reaches here.
     */
    for (scan = def; scan; scan = scan->next) {
        if (scan == use) {
            return 1;
        }
    }
    return 0;
}

static int is_predecessor(struct ir_block *block, struct ir_block *candidate)
{
    int i;

    for (i = 0; i < block->pred_count; i++) {
        if (block->preds[i] == candidate) {
            return 1;
        }
    }
    return 0;
}

static void verify_phi(struct verify_ctx *ctx, struct ir_block *block,
    struct ir_instr *phi)
{
    int i;

    if (phi->arg_count != block->pred_count) {
        problem(ctx, phi, "argument count does not match the number of predecessors");
        return;
    }
    if (!phi->phi_blocks) {
        problem(ctx, phi, "arguments do not name the edges they arrive on");
        return;
    }

    for (i = 0; i < phi->arg_count; i++) {
        struct ir_block *from = phi->phi_blocks[i];

        if (!from || !is_predecessor(block, from)) {
            problem(ctx, phi, "argument arrives from a block that is not a predecessor");
            continue;
        }
        if (!phi->args[i]) {
            problem(ctx, phi, "argument is missing");
            continue;
        }

        /*
         * The argument has to be available at the end of the block it comes
         * from, not at the phi. Requiring it at the phi would reject the
         * ordinary case of a loop variable defined in the body.
         */
        if (!def_reaches(phi->args[i], from, from->last)) {
            problem(ctx, phi, "argument is not available on the edge it arrives on");
        }
    }
}

static void verify_block(struct verify_ctx *ctx, struct ir_block *block)
{
    struct ir_instr *instr;
    int seen_non_phi = 0;

    if (!ir_terminator(block)) {
        fprintf(ctx->output, "ir: %s: b%d: block does not end with a terminator\n",
            ctx->func->name, block->id);
        ctx->problems++;
    }

    for (instr = block->first; instr; instr = instr->next) {
        int required = required_args(instr->op);
        int i;

        if (ir_is_terminator(instr->op) && instr != block->last) {
            problem(ctx, instr, "terminator is not the last instruction in the block");
        }
        if (required >= 0 && instr->arg_count != required) {
            problem(ctx, instr, "wrong number of operands");
        }
        if (instr->op == IR_CALL && instr->arg_count < 1) {
            problem(ctx, instr, "call has no callee");
        }
        if (instr->op == IR_JMP && !instr->target) {
            problem(ctx, instr, "jump has no target");
        }
        if (instr->op == IR_BR && (!instr->then_block || !instr->else_block)) {
            problem(ctx, instr, "branch is missing an arm");
        }

        if (instr->op == IR_PHI) {
            if (seen_non_phi) {
                problem(ctx, instr, "phi does not come before the other instructions");
            }
            verify_phi(ctx, block, instr);
            continue;
        }
        seen_non_phi = 1;

        for (i = 0; i < instr->arg_count; i++) {
            if (!instr->args[i]) {
                problem(ctx, instr, "operand is missing");
                continue;
            }
            if (!def_reaches(instr->args[i], block, instr)) {
                problem(ctx, instr,
                    "operand is used where its definition may not have run");
            }
        }

        if (instr->dst && instr->dst->def != instr) {
            problem(ctx, instr, "result value does not point back at its definition");
        }
    }
}

int ir_verify(struct ir_func *func, FILE *output)
{
    struct verify_ctx ctx;
    int i;

    ctx.func = func;
    ctx.output = output;
    ctx.problems = 0;

    if (!func->entry) {
        fprintf(output, "ir: %s: function has no entry block\n", func->name);
        return 1;
    }
    if (func->entry->pred_count > 0) {
        fprintf(output, "ir: %s: something branches back to the entry block\n",
            func->name);
        ctx.problems++;
    }

    /* Only reachable blocks: the rest were emptied by the CFG pass. */
    for (i = 0; i < func->rpo_count; i++) {
        verify_block(&ctx, func->rpo[i]);
    }
    return ctx.problems;
}

int ir_verify_program(struct ir_program *program, FILE *output)
{
    struct ir_func *func;
    int problems = 0;

    for (func = program->first; func; func = func->next) {
        problems += ir_verify(func, output);
    }
    return problems;
}
