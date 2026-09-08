/*
 * The textual form of the IR.
 *
 * An IR that cannot be read is an IR that cannot be debugged, and every later
 * pass will be judged by what this prints before and after it. The format is
 * deliberately plain: one instruction per line, values as %n, blocks as bN,
 * and types spelled out where they matter.
 */
#include <stdio.h>
#include <stdlib.h>
#include "ir.h"

static const char *op_names[IR_OP_COUNT] = {
    "nop",
    "const", "constfp", "str", "global", "alloca", "param",
    "load", "store", "copy", "memcpy",
    "add", "sub", "mul", "div", "mod",
    "shl", "shr", "and", "or", "xor",
    "neg", "not", "lognot",
    "eq", "ne", "lt", "le", "gt", "ge",
    "cast",
    "call",
    "jmp", "br", "ret",
    "phi"
};

const char *ir_op_name(IROp op)
{
    if (op < 0 || op >= IR_OP_COUNT || !op_names[op]) {
        return "?";
    }
    return op_names[op];
}

static void print_value(struct ir_value *value, FILE *output)
{
    if (!value) {
        fprintf(output, "<null>");
        return;
    }
    fprintf(output, "%%%d", value->id);
}

static void print_block_name(struct ir_block *block, FILE *output)
{
    if (!block) {
        fprintf(output, "<none>");
        return;
    }
    fprintf(output, "b%d", block->id);
}

static void print_instr(struct ir_instr *instr, FILE *output)
{
    char type_name[64];
    int i;

    fprintf(output, "    ");

    if (instr->dst) {
        print_value(instr->dst, output);
        ty_format(instr->dst->ty, type_name, sizeof(type_name));
        fprintf(output, " : %s = ", type_name);
    }

    fprintf(output, "%s", ir_op_name(instr->op));

    switch (instr->op) {
        case IR_CONST:
            fprintf(output, " %ld", instr->imm);
            break;
        case IR_CONST_FP:
        case IR_STR:
            fprintf(output, " \"%s\"", instr->text ? instr->text : "");
            break;
        case IR_GLOBAL:
            fprintf(output, " @%s", instr->text ? instr->text
                : (instr->sym ? instr->sym->name : "?"));
            break;
        case IR_ALLOCA:
            /*
             * An unnamed slot is one lowering made for itself -- the result of
             * a && or a ?: -- so it is shown by what it holds, not by a name it
             * never had.
             */
            if (instr->sym) {
                fprintf(output, " %s", instr->sym->name);
            } else {
                fprintf(output, " <temp>");
            }
            break;
        case IR_PARAM:
            fprintf(output, " #%ld %s", instr->imm,
                instr->sym ? instr->sym->name : "?");
            break;
        case IR_MEMCPY:
            fprintf(output, " %ld bytes", instr->imm);
            break;
        case IR_JMP:
            fprintf(output, " ");
            print_block_name(instr->target, output);
            break;
        default:
            break;
    }

    for (i = 0; i < instr->arg_count; i++) {
        fprintf(output, "%s ", i == 0 && instr->op != IR_JMP ? "" : ",");
        print_value(instr->args[i], output);
        if (instr->op == IR_PHI && instr->phi_blocks) {
            fprintf(output, " from ");
            print_block_name(instr->phi_blocks[i], output);
        }
    }

    if (instr->op == IR_BR) {
        fprintf(output, " ? ");
        print_block_name(instr->then_block, output);
        fprintf(output, " : ");
        print_block_name(instr->else_block, output);
    }

    fprintf(output, "\n");
}

void ir_dump_func(struct ir_func *func, FILE *output)
{
    struct ir_block *block;
    char type_name[64];

    ty_format(func->return_type, type_name, sizeof(type_name));
    fprintf(output, "function %s : %s\n", func->name, type_name);

    for (block = func->entry; block; block = block->next) {
        struct ir_instr *instr;
        int i;

        if (block->rpo_index < 0) {
            continue;               /* unreachable, and already emptied */
        }

        fprintf(output, "  ");
        print_block_name(block, output);
        if (block->label) {
            fprintf(output, " (%s)", block->label);
        }
        fprintf(output, ":");

        if (block->pred_count > 0) {
            fprintf(output, "  ; preds");
            for (i = 0; i < block->pred_count; i++) {
                fprintf(output, " ");
                print_block_name(block->preds[i], output);
            }
        }
        if (block->idom) {
            fprintf(output, "  ; idom ");
            print_block_name(block->idom, output);
        }
        fprintf(output, "\n");

        for (instr = block->first; instr; instr = instr->next) {
            print_instr(instr, output);
        }
    }
    fprintf(output, "\n");
}

void ir_dump_program(struct ir_program *program, FILE *output)
{
    struct ir_func *func;

    for (func = program->first; func; func = func->next) {
        ir_dump_func(func, output);
    }
}
