/*
 * Code generator: statements. Control flow, loops, switch dispatch, and the
 * labels a goto can reach.
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

int label_for_name(struct cg_ctx *ctx, const char *name)
{
    int i;

    for (i = 0; i < ctx->label_name_count; i++) {
        if (strcmp(ctx->labels[i].name, name) == 0) {
            return ctx->labels[i].number;
        }
    }

    grow_array((void **)&ctx->labels, ctx->label_name_count,
        &ctx->label_name_capacity, sizeof(*ctx->labels), 32, "code generator state");
    ctx->labels[ctx->label_name_count].name = strdup(name);
    ctx->labels[ctx->label_name_count].number = ctx->label_count++;
    return ctx->labels[ctx->label_name_count++].number;
}

void collect_labels(struct cg_ctx *ctx, struct ast_node *node)
{
    if (!node) {
        return;
    }
    if (node->type == AST_LABEL && node->value) {
        label_for_name(ctx, node->value);
    }
    collect_labels(ctx, node->left);
    collect_labels(ctx, node->right);
}

void free_labels(struct cg_ctx *ctx)
{
    int i;

    for (i = 0; i < ctx->label_name_count; i++) {
        free(ctx->labels[i].name);
    }
    free(ctx->labels);
    ctx->labels = NULL;
    ctx->label_name_count = 0;
    ctx->label_name_capacity = 0;
}

void emit_switch_tests(struct cg_ctx *ctx, struct ast_node *node,
    int *default_label, FILE *output)
{
    if (!node) {
        return;
    }

    if (node->type == AST_CASE) {
        node->string_label = ctx->label_count++;    /* reused as this case's label */
        fprintf(output, "    cmpl    $%s, %%eax\n", node->value ? node->value : "0");
        fprintf(output, "    je      .L%d\n", node->string_label);
        emit_switch_tests(ctx, node->left, default_label, output);
        return;
    }
    if (node->type == AST_DEFAULT) {
        node->string_label = ctx->label_count++;
        *default_label = node->string_label;
        emit_switch_tests(ctx, node->left, default_label, output);
        return;
    }

    /*
     * Only the statements that can contain a case label of this switch are
     * followed. A nested switch owns its own cases, so it is not entered.
     */
    if (node->type == AST_SWITCH) {
        return;
    }
    emit_switch_tests(ctx, node->left, default_label, output);
    emit_switch_tests(ctx, node->right, default_label, output);
}

void push_loop(struct cg_ctx *ctx, int break_label, int continue_label)
{
    int capacity_before = ctx->loop_capacity;

    grow_array((void **)&ctx->loop_break_labels, ctx->loop_depth,
        &ctx->loop_capacity, sizeof(*ctx->loop_break_labels), 32, "code generator state");
    /* Both stacks are kept the same size, so grow the second to match. */
    grow_array((void **)&ctx->loop_continue_labels, ctx->loop_depth,
        &capacity_before, sizeof(*ctx->loop_continue_labels), 32, "code generator state");

    ctx->loop_break_labels[ctx->loop_depth] = break_label;
    ctx->loop_continue_labels[ctx->loop_depth] = continue_label;
    ctx->loop_depth++;
}

void pop_loop(struct cg_ctx *ctx)
{
    if (ctx->loop_depth > 0) {
        ctx->loop_depth--;
    }
}

void generate_statement(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
{
    if (!node) {
        return;
    }

    switch (node->type) {
        case AST_BLOCK:
            generate_statement(ctx, node->left, output);
            break;
        case AST_STATEMENT_LIST:
            generate_statement(ctx, node->left, output);
            generate_statement(ctx, node->right, output);
            break;
        case AST_DECL:
            if (node->array_length > 0) {
                int offset = node->sym->offset;
                /*
                 * Step by the element's real size. Striding by 4 through a
                 * char array would run past its end -- straight over the saved
                 * frame pointer and return address.
                 */
                int stride = ty_element_size(node->ty);
                struct Type *element = node->ty ? node->ty->base : NULL;
                int index;
                struct ast_node *item;

                /*
                 * Work out which elements the initialisers cover before
                 * emitting anything, so only the gaps are zeroed. Designators
                 * can leave holes -- `{[3] = 1}` writes one element of four --
                 * while a plain list usually covers everything and needs no
                 * zeroing at all.
                 */
                char *written = calloc((size_t)node->array_length, 1);

                if (!written) {
                    fprintf(output, "    /* out of memory */\n");
                    break;
                }
                index = 0;
                for (item = initializer_items(node->left); item; item = item->right) {
                    if (item->left && item->left->designator_index >= 0) {
                        index = item->left->designator_index;
                    }
                    if (index >= 0 && index < node->array_length) {
                        written[index] = 1;
                    }
                    index++;
                }
                for (index = 0; index < node->array_length; index++) {
                    if (!written[index]) {
                        emit_zero_offset(element, offset + (index * stride), output);
                    }
                }
                free(written);

                index = 0;
                for (item = initializer_items(node->left); item; item = item->right) {
                    /*
                     * A designator says where its element goes; the ones after
                     * it continue from there, so the running index is set
                     * rather than stepped.
                     */
                    if (item->left && item->left->designator_index >= 0) {
                        index = item->left->designator_index;
                    }
                    if (index >= node->array_length) {
                        diag_at(DIAG_ERROR, item->left->location,
                            "initializer index %d is outside '%s'",
                            index, node->value);
                        break;
                    }
                    generate_exp(ctx, item->left, output);
                    emit_store_offset(element, offset + (index * stride), output);
                    index++;
                }
            } else if (node->ty && node->ty->kind == TY_STRUCT && node->left &&
                       node->left->type == AST_CALL) {
                /* Initialised from a call: the result arrived in %rax:%rdx. */
                int slots = (node->ty->size + 7) / 8;

                generate_exp(ctx, node->left, output);
                fprintf(output, "    movq    %%rax, %d(%%rbp)\n", node->sym->offset);
                if (slots > 1) {
                    fprintf(output, "    movq    %%rdx, %d(%%rbp)\n",
                        node->sym->offset + 8);
                }
            } else if (node->ty && node->ty->kind == TY_STRUCT && node->left) {
                /*
                 * Brace initialisation of a struct. Without a designator each
                 * value fills the next member in declaration order; with one it
                 * fills the member it names, and the rest follow from there.
                 */
                int offset = node->sym->offset;
                struct Member *member = node->ty->members;
                struct ast_node *item;
                int i;

                for (i = 0; i < node->ty->size; i += 4) {
                    fprintf(output, "    movl    $0, %d(%%rbp)\n", offset + i);
                }

                for (item = initializer_items(node->left); item; item = item->right) {
                    /*
                     * Look the designator up before consulting the running
                     * member: after one that named the last field there is no
                     * next, and requiring one would drop what follows.
                     */
                    if (item->left && item->left->designator_field) {
                        member = ty_find_member(node->ty,
                            item->left->designator_field);
                        if (!member) {
                            diag_at(DIAG_ERROR, item->left->location,
                                "'%s' has no field '%s'",
                                node->struct_name ? node->struct_name : "struct",
                                item->left->designator_field);
                            break;
                        }
                    }
                    if (!member) {
                        break;
                    }
                    generate_exp(ctx, item->left, output);
                    emit_store_offset(member->ty, offset + member->offset, output);
                    member = member->next;
                }
            } else if (node->left) {
                /*
                 * A scalar local owns a whole 4-byte slot and is read back with
                 * a 4-byte load, so write all four bytes. The value has already
                 * been narrowed and extended to its declared type. Narrow
                 * stores are only for packed array elements, above.
                 */
                generate_exp(ctx, node->left, output);
                emit_store_slot(node->ty, node->sym->offset, output);
            } else {
                emit_zero_slot(node->ty, node->sym->offset, output);
            }
            break;
        case AST_EXPR_STMT:
            generate_exp(ctx, node->left, output);
            break;
        case AST_RETURN:
            /*
             * `return;` carries no expression. The value in %eax is then
             * whatever the caller must not look at, which is exactly what a
             * void return means.
             */
            if (node->left && node->left->ty &&
                node->left->ty->kind == TY_STRUCT) {
                /*
                 * A struct of up to sixteen bytes comes back in %rax and %rdx,
                 * one eightbyte each, the same classification used to pass one.
                 */
                int slots = (node->left->ty->size + 7) / 8;

                generate_lvalue_address(ctx, node->left, output);
                if (slots > 1) {
                    fprintf(output, "    movq    8(%%rax), %%rdx\n");
                }
                fprintf(output, "    movq    (%%rax), %%rax\n");
            } else if (node->left) {
                generate_exp(ctx, node->left, output);
            }
            fprintf(output, "    jmp     .L%d\n", ctx->current_function_end_label);
            break;
        case AST_IF: {
            int else_label = ctx->label_count++;
            int end_label = ctx->label_count++;

            generate_exp(ctx, node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    je      .L%d\n", else_label);
            generate_statement(ctx, node->right->left, output);
            fprintf(output, "    jmp     .L%d\n", end_label);
            fprintf(output, ".L%d:\n", else_label);
            if (node->right->right) {
                generate_statement(ctx, node->right->right, output);
            }
            fprintf(output, ".L%d:\n", end_label);
            break;
        }
        case AST_EMPTY:
            break;
        case AST_LABEL:
            fprintf(output, ".L%d:\n", label_for_name(ctx, node->value));
            generate_statement(ctx, node->left, output);
            break;
        case AST_GOTO:
            fprintf(output, "    jmp     .L%d\n", label_for_name(ctx, node->value));
            break;
        case AST_DO_WHILE: {
            int body_label = ctx->label_count++;
            int condition_label = ctx->label_count++;
            int end_label = ctx->label_count++;

            /*
             * The body comes first and the test last, so the body always runs
             * once. continue goes to the test, not to the top.
             */
            fprintf(output, ".L%d:\n", body_label);
            push_loop(ctx, end_label, condition_label);
            generate_statement(ctx, node->right, output);
            pop_loop(ctx);
            fprintf(output, ".L%d:\n", condition_label);
            generate_exp(ctx, node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    jne     .L%d\n", body_label);
            fprintf(output, ".L%d:\n", end_label);
            break;
        }
        case AST_SWITCH: {
            int end_label = ctx->label_count++;
            int default_label = -1;
            int saved_break = ctx->switch_break_label;
            int saved_in_switch = ctx->in_switch;

            /*
             * Evaluate the control value once, then compare it against each
             * case. The tests are emitted before the body, so control reaches
             * the matching label without running the statements above it.
             */
            generate_exp(ctx, node->left, output);
            emit_switch_tests(ctx, node->right, &default_label, output);
            fprintf(output, "    jmp     .L%d\n",
                default_label >= 0 ? default_label : end_label);

            ctx->switch_break_label = end_label;
            ctx->in_switch = 1;
            push_loop(ctx, end_label, ctx->loop_depth > 0
                ? ctx->loop_continue_labels[ctx->loop_depth - 1] : end_label);
            generate_statement(ctx, node->right, output);
            pop_loop(ctx);
            ctx->switch_break_label = saved_break;
            ctx->in_switch = saved_in_switch;

            fprintf(output, ".L%d:\n", end_label);
            break;
        }
        case AST_CASE:
        case AST_DEFAULT:
            /* The label number was assigned while the tests were emitted. */
            fprintf(output, ".L%d:\n", node->string_label);
            generate_statement(ctx, node->left, output);
            break;
        case AST_WHILE: {
            int start_label = ctx->label_count++;
            int end_label = ctx->label_count++;

            push_loop(ctx, end_label, start_label);
            fprintf(output, ".L%d:\n", start_label);
            generate_exp(ctx, node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    je      .L%d\n", end_label);
            generate_statement(ctx, node->right, output);
            fprintf(output, "    jmp     .L%d\n", start_label);
            fprintf(output, ".L%d:\n", end_label);
            pop_loop(ctx);
            break;
        }
        case AST_FOR: {
            struct ast_node *init = node->left->left;
            struct ast_node *cond = node->left->right->left;
            struct ast_node *post = node->left->right->right;
            int start_label = ctx->label_count++;
            int post_label = ctx->label_count++;
            int end_label = ctx->label_count++;

            if (init) {
                if (init->type == AST_DECL) {
                    generate_statement(ctx, init, output);
                } else {
                    generate_exp(ctx, init, output);
                }
            }

            push_loop(ctx, end_label, post_label);
            fprintf(output, ".L%d:\n", start_label);
            if (cond) {
                generate_exp(ctx, cond, output);
                fprintf(output, "    cmpl    $0, %%eax\n");
                fprintf(output, "    je      .L%d\n", end_label);
            }
            generate_statement(ctx, node->right, output);
            fprintf(output, ".L%d:\n", post_label);
            if (post) {
                generate_exp(ctx, post, output);
            }
            fprintf(output, "    jmp     .L%d\n", start_label);
            fprintf(output, ".L%d:\n", end_label);
            pop_loop(ctx);
            break;
        }
        case AST_BREAK:
            if (ctx->loop_depth == 0) {
                diag_internal(node->location, "'break' outside a loop");
            }
            fprintf(output, "    jmp     .L%d\n", ctx->loop_break_labels[ctx->loop_depth - 1]);
            break;
        case AST_CONTINUE:
            if (ctx->loop_depth == 0) {
                diag_internal(node->location, "'continue' outside a loop");
            }
            fprintf(output, "    jmp     .L%d\n", ctx->loop_continue_labels[ctx->loop_depth - 1]);
            break;
        default:
            diag_internal(node->location, "unsupported statement node type %d",
                node->type);
    }
}
