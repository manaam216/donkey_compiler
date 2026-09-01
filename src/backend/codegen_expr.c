/*
 * Code generator: expressions. Values, addresses, operators, and the call
 * sequence that puts arguments where the ABI wants them.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "decl.h"
#include "type.h"
#include "symbol.h"
#include "diag.h"
#include "support/mem.h"
#include "codegen_internal.h"

void generate_incdec_lvalue(struct cg_ctx *ctx, struct ast_node *node,
    int is_increment, int is_prefix, FILE *output)
{
    struct Type *ty = node->ty;

    generate_lvalue_address(ctx, node->left, output);
    fprintf(output, "    pushq   %%rax\n");          /* the address */
    emit_load_indirect(ty, output);

    if (!is_prefix) {
        fprintf(output, "    pushq   %%rax\n");      /* the value before */
    }

    emit_step(ty, is_increment, output);
    generate_cast(codegen_type_name(node->data_type), output);
    fprintf(output, "    movq    %%rax, %%rdx\n");   /* the value to store */

    if (!is_prefix) {
        fprintf(output, "    popq    %%rcx\n");      /* the value before */
    }
    fprintf(output, "    popq    %%rax\n");          /* the address */
    emit_store_indirect(ty, output);

    fprintf(output, is_prefix ? "    movq    %%rdx, %%rax\n"
                              : "    movq    %%rcx, %%rax\n");
}

void generate_identifier_load(struct ast_node *node, FILE *output)
{
    struct Symbol *sym = node->sym;
    int is_array = sym->ty && sym->ty->kind == TY_ARRAY;

    /* A function's name is its address; there is nothing to load from. */
    if (sym->kind == SYM_FUNCTION) {
        fprintf(output, "    leaq    %s(%%rip), %%rax\n", sym->name);
        return;
    }

    if (ty_is_float(sym->ty)) {
        if (is_frame_symbol(sym)) {
            emit_fp_load_frame(sym->ty, sym->offset, output);
        } else {
            fprintf(output, "    mov%s   %s(%%rip), %%xmm0\n",
                fp_suffix(sym->ty), sym->name);
        }
        return;
    }

    if (is_frame_symbol(sym)) {
        /* An array's value is its address; anything else is loaded. */
        if (is_array) {
            fprintf(output, "    leaq    %d(%%rbp), %%rax\n", sym->offset);
        } else {
            emit_load_frame(sym->ty, sym->offset, output);
        }
        return;
    }

    if (is_array) {
        fprintf(output, "    leaq    %s(%%rip), %%rax\n", sym->name);
    } else {
        emit_load_global(sym->ty, sym->name, output);
    }
}

void generate_identifier_store(struct ast_node *node, FILE *output)
{
    struct Symbol *sym = node->sym;

    if (ty_is_float(sym->ty)) {
        if (is_frame_symbol(sym)) {
            emit_fp_store_frame(sym->ty, sym->offset, output);
        } else {
            fprintf(output, "    mov%s   %%xmm0, %s(%%rip)\n",
                fp_suffix(sym->ty), sym->name);
        }
        return;
    }

    if (is_frame_symbol(sym)) {
        emit_store_slot(sym->ty, sym->offset, output);
        return;
    }

    emit_store_global(sym->ty, sym->name, output);
}

void generate_initializer_into(struct cg_ctx *ctx, struct Type *ty,
    int offset, struct ast_node *initializer, FILE *output)
{
    struct ast_node *item;

    if (!ty) {
        return;
    }

    if (ty->kind == TY_ARRAY) {
        int stride = ty_element_size(ty);
        struct Type *element = ty->base;
        int index;
        char *written = calloc((size_t)ty->array_length, 1);

        if (!written) {
            return;
        }
        index = 0;
        for (item = initializer_items(initializer); item; item = item->right) {
            if (item->left && item->left->designator_index >= 0) {
                index = item->left->designator_index;
            }
            if (index >= 0 && index < ty->array_length) {
                written[index] = 1;
            }
            index++;
        }
        for (index = 0; index < ty->array_length; index++) {
            if (!written[index]) {
                emit_zero_offset(element, offset + (index * stride), output);
            }
        }
        free(written);

        index = 0;
        for (item = initializer_items(initializer); item; item = item->right) {
            if (item->left && item->left->designator_index >= 0) {
                index = item->left->designator_index;
            }
            if (index >= ty->array_length) {
                break;
            }
            generate_exp(ctx, item->left, output);
            emit_store_offset(element, offset + (index * stride), output);
            index++;
        }
        return;
    }

    if (ty->kind == TY_STRUCT) {
        struct Member *member = ty->members;
        int i;

        for (i = 0; i < ty->size; i += 4) {
            fprintf(output, "    movl    $0, %d(%%rbp)\n", offset + i);
        }
        for (item = initializer_items(initializer); item; item = item->right) {
            /*
             * A designator names its member, so it is looked up before the
             * running one is consulted. Testing that first matters: after a
             * designator that named the last member there is no next one, and
             * requiring one would drop every element after it.
             */
            if (item->left && item->left->designator_field) {
                member = ty_find_member(ty, item->left->designator_field);
            }
            if (!member) {
                break;
            }
            generate_exp(ctx, item->left, output);
            emit_store_offset(member->ty, offset + member->offset, output);
            member = member->next;
        }
        return;
    }

    /* A scalar compound literal, as in (int){5}. */
    if (initializer) {
        generate_exp(ctx, initializer_items(initializer)
            ? initializer_items(initializer)->left : initializer, output);
        emit_store_slot(ty, offset, output);
    }
}

void generate_lvalue_address(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
{
    switch (node->type) {
        case AST_COMPOUND_LITERAL:
            /*
             * The literal is written into its slot at the point it appears,
             * then behaves like any other object at that address.
             */
            generate_initializer_into(ctx, node->ty, node->sym->offset,
                node->left, output);
            fprintf(output, "    leaq    %d(%%rbp), %%rax\n", node->sym->offset);
            return;
        case AST_IDENTIFIER:
            if (is_frame_symbol(node->sym)) {
                fprintf(output, "    leaq    %d(%%rbp), %%rax\n", node->sym->offset);
            } else {
                fprintf(output, "    leaq    %s(%%rip), %%rax\n", node->sym->name);
            }
            return;
        case AST_DEREFERENCE:
            generate_exp(ctx, node->left, output);
            return;
        case AST_ARRAY_SUBSCRIPT:
            if (node->left->array_length > 0) {
                generate_lvalue_address(ctx, node->left, output);
            } else {
                generate_exp(ctx, node->left, output);
            }
            fprintf(output, "    pushq   %%rax\n");
            generate_exp(ctx, node->right, output);
            /*
             * Widen the index before scaling: addresses are 64-bit, and a
             * 32-bit add would truncate a stack address to garbage.
             */
            fprintf(output, "    cltq\n");
            fprintf(output, "    imulq   $%d, %%rax\n",
                ty_element_size(node->left->ty));
            fprintf(output, "    popq    %%rdx\n");
            fprintf(output, "    addq    %%rdx, %%rax\n");
            return;
        case AST_FIELD_ACCESS:
            generate_lvalue_address(ctx, node->left, output);
            /* Adding to an address: 64-bit, or the pointer is truncated. */
            fprintf(output, "    addq    $%d, %%rax\n",
                struct_field_offset(node->left->ty, node->value));
            return;
        default:
            diag_internal(node->location, "expression is not assignable");
    }
}

int generate_float_binop(struct cg_ctx *ctx, struct ast_node *node,
    FILE *output)
{
    struct Type *ty = ty_is_float(node->left->ty) ? node->left->ty : node->right->ty;
    const char *suffix = fp_suffix(ty);
    const char *op = NULL;
    const char *set = NULL;
    int is_comparison = 0;

    switch (node->type) {
        case AST_ADD: op = "add"; break;
        case AST_SUB: op = "sub"; break;
        case AST_MUL: op = "mul"; break;
        case AST_DIV: op = "div"; break;
        case AST_EQUAL:         set = "sete";  is_comparison = 1; break;
        case AST_NOT_EQUAL:     set = "setne"; is_comparison = 1; break;
        case AST_LESS:          set = "seta";  is_comparison = 1; break;
        case AST_LESS_EQUAL:    set = "setae"; is_comparison = 1; break;
        case AST_GREATER:       set = "seta";  is_comparison = 1; break;
        case AST_GREATER_EQUAL: set = "setae"; is_comparison = 1; break;
        default: return 0;
    }

    generate_exp(ctx, node->left, output);
    emit_fp_push(ty, output);
    generate_exp(ctx, node->right, output);
    emit_fp_pop(ty, "xmm1", output);

    if (is_comparison) {
        /*
         * ucomis compares its second operand against its first and sets the
         * unsigned flags, so ordering the operands turns every relation into an
         * above or above-or-equal test.
         */
        if (node->type == AST_GREATER || node->type == AST_GREATER_EQUAL) {
            fprintf(output, "    ucomi%s %%xmm0, %%xmm1\n", suffix);
        } else {
            fprintf(output, "    ucomi%s %%xmm1, %%xmm0\n", suffix);
        }
        fprintf(output, "    movl    $0, %%eax\n");
        fprintf(output, "    %s   %%al\n", set);
        return 1;
    }

    if (node->type == AST_SUB || node->type == AST_DIV) {
        fprintf(output, "    %s%s   %%xmm0, %%xmm1\n", op, suffix);
        fprintf(output, "    mov%s   %%xmm1, %%xmm0\n", suffix);
    } else {
        fprintf(output, "    %s%s   %%xmm1, %%xmm0\n", op, suffix);
    }
    return 1;
}

void generate_binop(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
{
    /* Either operand being floating point makes the whole operation so. */
    if (ty_is_float(node->left->ty) || ty_is_float(node->right->ty)) {
        if (generate_float_binop(ctx, node, output)) {
            return;
        }
    }

    int is_unsigned = is_unsigned_type(node->left->data_type);
    int left_is_pointer = node->left->pointer_depth > 0 || node->left->array_length > 0;
    int right_is_pointer = node->right->pointer_depth > 0 || node->right->array_length > 0;

    if ((node->type == AST_ADD || node->type == AST_SUB) &&
        (left_is_pointer || right_is_pointer)) {
        generate_exp(ctx, node->left, output);
        fprintf(output, "    pushq   %%rax\n");
        generate_exp(ctx, node->right, output);
        fprintf(output, "    popq    %%rdx\n");

        if (left_is_pointer && right_is_pointer && node->type == AST_SUB) {
            fprintf(output, "    subq    %%rax, %%rdx\n");
            fprintf(output, "    movq    %%rdx, %%rax\n");
            fprintf(output, "    cqto\n");
            fprintf(output, "    movq    $%d, %%rcx\n",
                ty_element_size(node->left->ty));
            fprintf(output, "    idivq   %%rcx\n");
            return;
        }
        if (left_is_pointer && !right_is_pointer) {
            fprintf(output, "    cltq\n");
            fprintf(output, "    imulq   $%d, %%rax\n",
                ty_element_size(node->left->ty));
            if (node->type == AST_ADD) {
                fprintf(output, "    addq    %%rdx, %%rax\n");
            } else {
                fprintf(output, "    subq    %%rax, %%rdx\n");
                fprintf(output, "    movq    %%rdx, %%rax\n");
            }
            return;
        }
        if (right_is_pointer && !left_is_pointer && node->type == AST_ADD) {
            fprintf(output, "    imulq   $%d, %%rdx\n",
                ty_element_size(node->right->ty));
            fprintf(output, "    addq    %%rdx, %%rax\n");
            return;
        }
    }

    generate_exp(ctx, node->left, output);
    fprintf(output, "    pushq   %%rax\n");

    generate_exp(ctx, node->right, output);
    fprintf(output, "    popq    %%rdx\n");

    switch (node->type) {
        case AST_ADD:
            fprintf(output, "    addl    %%edx, %%eax\n");
            break;
        case AST_SUB:
            fprintf(output, "    subl    %%eax, %%edx\n");
            fprintf(output, "    movl    %%edx, %%eax\n");
            break;
        case AST_MUL:
            fprintf(output, "    imull   %%edx, %%eax\n");
            break;
        case AST_DIV:
            fprintf(output, "    pushq   %%rax\n");
            fprintf(output, "    movl    %%edx, %%eax\n");
            fprintf(output, "    popq    %%rcx\n");
            if (is_unsigned) {
                fprintf(output, "    xorl    %%edx, %%edx\n");
                fprintf(output, "    divl    %%ecx\n");
            } else {
                fprintf(output, "    cdq\n");
                fprintf(output, "    idivl   %%ecx\n");
            }
            break;
        case AST_MOD:
            fprintf(output, "    pushq   %%rax\n");
            fprintf(output, "    movl    %%edx, %%eax\n");
            fprintf(output, "    popq    %%rcx\n");
            if (is_unsigned) {
                fprintf(output, "    xorl    %%edx, %%edx\n");
                fprintf(output, "    divl    %%ecx\n");
            } else {
                fprintf(output, "    cdq\n");
                fprintf(output, "    idivl   %%ecx\n");
            }
            fprintf(output, "    movl    %%edx, %%eax\n");
            break;
        case AST_SHIFT_LEFT:
            fprintf(output, "    movl    %%eax, %%ecx\n");
            fprintf(output, "    movl    %%edx, %%eax\n");
            fprintf(output, "    sall    %%cl, %%eax\n");
            break;
        case AST_SHIFT_RIGHT:
            fprintf(output, "    movl    %%eax, %%ecx\n");
            fprintf(output, "    movl    %%edx, %%eax\n");
            fprintf(output, is_unsigned ? "    shrl    %%cl, %%eax\n" : "    sarl    %%cl, %%eax\n");
            break;
        case AST_BITWISE_AND:
            fprintf(output, "    andl    %%edx, %%eax\n");
            break;
        case AST_BITWISE_OR:
            fprintf(output, "    orl     %%edx, %%eax\n");
            break;
        case AST_BITWISE_XOR:
            fprintf(output, "    xorl    %%edx, %%eax\n");
            break;
        case AST_EQUAL:
            emit_compare(node->left->ty, node->right->ty, output);
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    sete    %%al\n");
            break;
        case AST_NOT_EQUAL:
            emit_compare(node->left->ty, node->right->ty, output);
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    setne   %%al\n");
            break;
        case AST_LESS:
            emit_compare(node->left->ty, node->right->ty, output);
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, is_unsigned ? "    setb    %%al\n" : "    setl    %%al\n");
            break;
        case AST_LESS_EQUAL:
            emit_compare(node->left->ty, node->right->ty, output);
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, is_unsigned ? "    setbe   %%al\n" : "    setle   %%al\n");
            break;
        case AST_GREATER:
            emit_compare(node->left->ty, node->right->ty, output);
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, is_unsigned ? "    seta    %%al\n" : "    setg    %%al\n");
            break;
        case AST_GREATER_EQUAL:
            emit_compare(node->left->ty, node->right->ty, output);
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, is_unsigned ? "    setae   %%al\n" : "    setge   %%al\n");
            break;
        default:
            diag_internal(node->location, "unsupported operation in AST");
    }
}

void generate_exp(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
{
    switch (node->type) {
        case AST_INTLIT:
            fprintf(output, "    movl    $%s, %%eax\n", node->value);
            break;
        case AST_FLOATLIT: {
            int is_float = node->ty && node->ty->size == 4;
            int label = intern_fp_constant(node->value, is_float);

            fprintf(output, "    mov%s   .LF%d(%%rip), %%xmm0\n",
                fp_suffix(node->ty), label);
            break;
        }
        case AST_STRINGLIT:
            fprintf(output, "    leaq    .LC%d(%%rip), %%rax\n", node->string_label);
            break;
        case AST_IDENTIFIER:
            generate_identifier_load(node, output);
            break;
        case AST_CALL: {
            int arg_count = count_args(node->left);
            /* Registers are assigned per eightbyte, not per argument. */
            int slot_count = count_arg_slots(node->left);
            int stack_args = slot_count > 6 ? slot_count - 6 : 0;
            /*
             * %rsp is 16-byte aligned here. Each push moves it by 8, so an odd
             * number of stack arguments would leave it misaligned at the call,
             * which System V forbids.
             */
            int padding = (stack_args % 2) ? 8 : 0;
            int registers = slot_count < 6 ? slot_count : 6;

            (void)arg_count;
            int float_registers = 0;
            int i;

            if (padding) {
                fprintf(output, "    subq    $8, %%rsp\n");
            }

            /*
             * Arguments are pushed last-first, so the first one ends up on top
             * and the register arguments pop off in order.
             */
            generate_call_args(ctx, node->left, output);
            /*
             * System V has separate register sequences for integer and
             * floating arguments, so each is assigned from its own list in
             * argument order.
             */
            {
                struct ast_node *arg = node->left;
                int integer_index = 0;
                int float_index = 0;

                for (i = 0; i < registers && arg; arg = arg->right) {
                    struct ast_node *value = arg->type == AST_ARG_LIST ? arg->left : arg;

                    if (ty_is_float(value->ty)) {
                        fprintf(output, "    movsd   (%%rsp), %%xmm%d\n", float_index++);
                        fprintf(output, "    addq    $8, %%rsp\n");
                        i++;
                    } else {
                        /* A struct takes one register per eightbyte. */
                        int slots = arg_slots(value);
                        int slot;

                        for (slot = 0; slot < slots && i < registers; slot++, i++) {
                            fprintf(output, "    popq    %s\n",
                                arg_reg64[integer_index++]);
                        }
                    }
                }
                float_registers = float_index;
            }

            /*
             * A call through a function pointer loads the target and calls
             * through the register. The pointer is loaded after the arguments
             * are in place, so evaluating it cannot disturb them.
             */
            if (node->is_indirect_call) {
                fprintf(output, "    movq    %d(%%rbp), %%r10\n", node->sym->offset);
                fprintf(output, "    movl    $0, %%eax\n");
                fprintf(output, "    call    *%%r10\n");
            } else {
                /* A variadic callee reads %al for the vector register count. */
                fprintf(output, "    movl    $%d, %%eax\n", float_registers);
                fprintf(output, "    call    %s\n", node->value);
            }

            if (stack_args > 0 || padding) {
                fprintf(output, "    addq    $%d, %%rsp\n",
                    stack_args * 8 + padding);
            }
            break;
        }
        case AST_ADD:
        case AST_SUB:
        case AST_MUL:
        case AST_DIV:
        case AST_MOD:
        case AST_SHIFT_LEFT:
        case AST_SHIFT_RIGHT:
        case AST_BITWISE_AND:
        case AST_BITWISE_OR:
        case AST_BITWISE_XOR:
        case AST_EQUAL:
        case AST_NOT_EQUAL:
        case AST_LESS:
        case AST_LESS_EQUAL:
        case AST_GREATER:
        case AST_GREATER_EQUAL:
            generate_binop(ctx, node, output);
            break;
        case AST_CONDITIONAL: {
            int else_label = ctx->label_count++;
            int end_label = ctx->label_count++;

            generate_exp(ctx, node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    je      .L%d\n", else_label);
            generate_exp(ctx, node->right->left, output);
            fprintf(output, "    jmp     .L%d\n", end_label);
            fprintf(output, ".L%d:\n", else_label);
            generate_exp(ctx, node->right->right, output);
            fprintf(output, ".L%d:\n", end_label);
            break;
        }
        case AST_COMMA:
            generate_exp(ctx, node->left, output);
            generate_exp(ctx, node->right, output);
            break;
        case AST_LOGICAL_AND: {
            int false_label = ctx->label_count++;
            int end_label = ctx->label_count++;

            generate_exp(ctx, node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    je      .L%d\n", false_label);
            generate_exp(ctx, node->right, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    je      .L%d\n", false_label);
            fprintf(output, "    movl    $1, %%eax\n");
            fprintf(output, "    jmp     .L%d\n", end_label);
            fprintf(output, ".L%d:\n", false_label);
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, ".L%d:\n", end_label);
            break;
        }
        case AST_LOGICAL_OR: {
            int true_label = ctx->label_count++;
            int end_label = ctx->label_count++;

            generate_exp(ctx, node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    jne     .L%d\n", true_label);
            generate_exp(ctx, node->right, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    jne     .L%d\n", true_label);
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    jmp     .L%d\n", end_label);
            fprintf(output, ".L%d:\n", true_label);
            fprintf(output, "    movl    $1, %%eax\n");
            fprintf(output, ".L%d:\n", end_label);
            break;
        }
        case AST_ASSIGN:
            /*
             * A struct does not fit in a register, so it is copied rather than
             * moved. Both sides are addresses in that case, not values.
             */
            if (node->left->ty && node->left->ty->kind == TY_STRUCT) {
                /*
                 * A call returns its struct in %rax and %rdx, so there is no
                 * address to copy from -- the value is already in registers.
                 */
                if (node->right->type == AST_CALL) {
                    int slots = (node->left->ty->size + 7) / 8;

                    generate_exp(ctx, node->right, output);
                    fprintf(output, "    movq    %%rax, %%rcx\n");
                    if (slots > 1) {
                        fprintf(output, "    movq    %%rdx, %%rsi\n");
                    }
                    generate_lvalue_address(ctx, node->left, output);
                    fprintf(output, "    movq    %%rcx, (%%rax)\n");
                    if (slots > 1) {
                        fprintf(output, "    movq    %%rsi, 8(%%rax)\n");
                    }
                    break;
                }
                generate_lvalue_address(ctx, node->right, output);
                fprintf(output, "    pushq   %%rax\n");
                generate_lvalue_address(ctx, node->left, output);
                fprintf(output, "    popq    %%rdx\n");
                emit_struct_copy(node->left->ty->size, output);
                break;
            }
            generate_exp(ctx, node->right, output);
            fprintf(output, "    pushq   %%rax\n");
            generate_lvalue_address(ctx, node->left, output);
            fprintf(output, "    popq    %%rdx\n");
            emit_store_indirect(node->left->ty, output);
            fprintf(output, node->left->ty && node->left->ty->size == 8
                ? "    movq    %%rdx, %%rax\n" : "    movl    %%edx, %%eax\n");
            break;
        case AST_ADDRESS_OF:
            generate_lvalue_address(ctx, node->left, output);
            break;
        case AST_DEREFERENCE:
            generate_exp(ctx, node->left, output);
            emit_load_indirect(node->ty, output);
            break;
        case AST_ARRAY_SUBSCRIPT:
            generate_lvalue_address(ctx, node, output);
            emit_load_indirect(node->ty, output);
            break;
        case AST_FIELD_ACCESS:
            generate_lvalue_address(ctx, node, output);
            emit_load_indirect(node->ty, output);
            break;
        case AST_PRE_INCREMENT:
            if (node->left->type != AST_IDENTIFIER) {
                generate_incdec_lvalue(ctx, node, 1, 1, output);
                break;
            }
            generate_identifier_load(node->left, output);
            emit_step(node->ty, 1, output);
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left, output);
            break;
        case AST_PRE_DECREMENT:
            if (node->left->type != AST_IDENTIFIER) {
                generate_incdec_lvalue(ctx, node, 0, 1, output);
                break;
            }
            generate_identifier_load(node->left, output);
            emit_step(node->ty, 0, output);
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left, output);
            break;
        case AST_POST_INCREMENT:
            if (node->left->type != AST_IDENTIFIER) {
                generate_incdec_lvalue(ctx, node, 1, 0, output);
                break;
            }
            generate_identifier_load(node->left, output);
            fprintf(output, "    pushq   %%rax\n");
            emit_step(node->ty, 1, output);
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left, output);
            fprintf(output, "    popq    %%rax\n");
            break;
        case AST_POST_DECREMENT:
            if (node->left->type != AST_IDENTIFIER) {
                generate_incdec_lvalue(ctx, node, 0, 0, output);
                break;
            }
            generate_identifier_load(node->left, output);
            fprintf(output, "    pushq   %%rax\n");
            emit_step(node->ty, 0, output);
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left, output);
            fprintf(output, "    popq    %%rax\n");
            break;
        case AST_SIZEOF:
            fprintf(output, "    movl    $%d, %%eax\n", sizeof_node(node));
            break;
        case AST_CAST:
            generate_exp(ctx, node->left, output);
            if (ty_is_float(node->ty) || ty_is_float(node->left->ty)) {
                generate_cast_between(node->left->ty, node->ty, output);
            } else {
                generate_cast(node->value, output);
            }
            break;
        case AST_NEGATION:
            generate_exp(ctx, node->left, output);
            fprintf(output, "    negl    %%eax\n");
            break;
        case AST_BITWISE_COMPLEMENT:
            generate_exp(ctx, node->left, output);
            fprintf(output, "    notl    %%eax\n");
            break;
        case AST_LOGICAL_NEGATION:
            generate_exp(ctx, node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    sete    %%al\n");
            break;
        default:
            diag_internal(node->location, "unsupported AST node type %d",
                node->type);
    }
}

int count_args(struct ast_node *node)
{
    int count = 0;

    for (; node; node = node->right) {
        count++;
    }
    return count;
}

int arg_slots(struct ast_node *value)
{
    if (value->ty && value->ty->kind == TY_STRUCT) {
        return (value->ty->size + 7) / 8;
    }
    return 1;
}

int count_arg_slots(struct ast_node *node)
{
    int slots = 0;

    for (; node; node = node->right) {
        slots += arg_slots(node->type == AST_ARG_LIST ? node->left : node);
    }
    return slots;
}

int generate_call_args(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
{
    if (!node) {
        return 0;
    }

    if (node->type != AST_ARG_LIST) {
        diag_internal(node->location, "unsupported argument node type %d",
            node->type);
    }

    int count = generate_call_args(ctx, node->right, output);

    /*
     * A struct of eight bytes or fewer is passed as one register-sized value,
     * so load its contents rather than evaluating it as an address.
     */
    if (node->left->ty && node->left->ty->kind == TY_STRUCT) {
        int slots = arg_slots(node->left);
        int slot;

        /* A call already left the struct in %rax and %rdx. */
        if (node->left->type == AST_CALL) {
            generate_exp(ctx, node->left, output);
            if (slots > 1) {
                fprintf(output, "    pushq   %%rdx\n");
            }
            fprintf(output, "    pushq   %%rax\n");
            return count + 1;
        }

        generate_lvalue_address(ctx, node->left, output);
        /*
         * Push the eightbytes highest first, so the lowest ends up on top and
         * pops into the first register.
         */
        for (slot = slots - 1; slot >= 0; slot--) {
            fprintf(output, "    movq    %d(%%rax), %%rdx\n", slot * 8);
            fprintf(output, "    pushq   %%rdx\n");
        }
    } else if (ty_is_float(node->left->ty)) {
        /*
         * A floating argument is always passed as a double: that is what the
         * ABI requires for a variadic call, and it is harmless otherwise.
         */
        generate_exp(ctx, node->left, output);
        if (node->left->ty->size == 4) {
            fprintf(output, "    cvtss2sd %%xmm0, %%xmm0\n");
        }
        fprintf(output, "    subq    $8, %%rsp\n");
        fprintf(output, "    movsd   %%xmm0, (%%rsp)\n");
    } else {
        generate_exp(ctx, node->left, output);
        fprintf(output, "    pushq   %%rax\n");
    }

    return count + 1;
}
