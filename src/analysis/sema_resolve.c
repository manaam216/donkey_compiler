#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "decl.h"
#include "type.h"
#include "symbol.h"
#include "diag.h"
#include "support/mem.h"
#include "sema_internal.h"

void sema_analyze_expression(struct sema_ctx *ctx, struct ast_node *node)
{
    int symbol;
    int actual_count;

    if (!node) {
        return;
    }

    switch (node->type) {
        case AST_INTLIT:
        case AST_SIZEOF:
            return;
        case AST_INITIALIZER_LIST:
            for (struct ast_node *item = initializer_items(node); item; item = item->right) {
                sema_analyze_expression(ctx, item->left);
            }
            return;
        case AST_COMPOUND_LITERAL: {
            struct ast_node *item;

            /*
             * An unnamed object with the same storage duration as a local, so
             * it needs a frame slot even though no declaration asked for one.
             * Reserved here because this is the pass that tracks the frame.
             */
            if (!node->sym) {
                struct Type *resolved = sema_resolve_type(ctx, node);
                int size = resolved->size > 0 ? resolved->size : 4;
                int align = resolved->align > 4 ? resolved->align : 4;

                node->sym = sym_new("<compound literal>", SYM_LOCAL, resolved);
                ctx->frame_offset += size;
                ctx->frame_offset = (ctx->frame_offset + align - 1) / align * align;
                node->sym->offset = -ctx->frame_offset;
                if (ctx->frame_offset > ctx->frame_max) {
                    ctx->frame_max = ctx->frame_offset;
                }
            }

            for (item = initializer_items(node->left); item; item = item->right) {
                sema_analyze_expression(ctx, item->left);
            }
            return;
        }
        case AST_IDENTIFIER:
            symbol = sema_find_global(ctx, node->value);
            /*
             * A function's name, used anywhere but in a call, is its address.
             * That is what makes `f = add;` legal.
             */
            if (sema_find_local(ctx, node->value) < 0 && symbol >= 0 &&
                ctx->globals[symbol].is_function) {
                return;
            }
            if (sema_find_local(ctx, node->value) < 0 &&
                (symbol < 0 || ctx->globals[symbol].is_function)) {
                semantic_error_at(ctx, node, "use of undeclared variable '%s'", node->value);
            }
            return;
        case AST_CALL:
            symbol = sema_find_global(ctx, node->value);
            {
                int local_index = sema_find_local(ctx, node->value);

                /*
                 * A local holding a function pointer is callable. Its
                 * arguments are not checked against a signature: the pointer's
                 * parameter list is parsed for syntax only.
                 */
                if (local_index >= 0 && ctx->locals[local_index].is_function_pointer) {
                    struct ast_node *argument;

                    for (argument = node->left; argument; argument = argument->right) {
                        sema_analyze_expression(ctx,
                            argument->type == AST_ARG_LIST ? argument->left : argument);
                    }
                    return;
                }
            }
            if (sema_find_local(ctx, node->value) >= 0) {
                semantic_error_at(ctx, node, "called object '%s' is not a function", node->value);
            } else if (symbol < 0) {
                semantic_error_at(ctx, node, "call to undeclared function '%s'", node->value);
            } else if (!ctx->globals[symbol].is_function) {
                semantic_error_at(ctx, node, "called object '%s' is not a function", node->value);
            } else {
                actual_count = sema_count_list(node->left, AST_ARG_LIST);
                if (ctx->globals[symbol].is_variadic
                        ? actual_count < ctx->globals[symbol].parameter_count
                        : actual_count != ctx->globals[symbol].parameter_count) {
                    semantic_error_at(ctx, node,
                        "function '%s' expects %s%d argument(s), but %d provided",
                        node->value,
                        ctx->globals[symbol].is_variadic ? "at least " : "",
                        ctx->globals[symbol].parameter_count, actual_count);
                }
            }
            for (struct ast_node *arg = node->left; arg; arg = arg->right) {
                sema_analyze_expression(ctx, arg->type == AST_ARG_LIST ? arg->left : arg);
                if (arg->type != AST_ARG_LIST) {
                    break;
                }
            }
            return;
        case AST_CAST:
        case AST_NEGATION:
        case AST_BITWISE_COMPLEMENT:
        case AST_LOGICAL_NEGATION:
        case AST_PRE_INCREMENT:
        case AST_PRE_DECREMENT:
        case AST_POST_INCREMENT:
        case AST_POST_DECREMENT:
            sema_analyze_expression(ctx, node->left);
            return;
        case AST_CONDITIONAL:
            sema_analyze_expression(ctx, node->left);
            sema_analyze_expression(ctx, node->right->left);
            sema_analyze_expression(ctx, node->right->right);
            return;
        default:
            sema_analyze_expression(ctx, node->left);
            sema_analyze_expression(ctx, node->right);
            return;
    }
}

static void analyze_block(struct sema_ctx *ctx, struct ast_node *node, int creates_scope)
{
    if (creates_scope) {
        sema_enter_scope(ctx);
    }
    if (node) {
        sema_analyze_statement(ctx, node->left);
    }
    if (creates_scope) {
        sema_leave_scope(ctx);
    }
}

/*
 * The first statement of a list, looking through nested list nodes, so a
 * warning can point at the statement itself rather than the list holding it.
 */
static struct ast_node *first_statement(struct ast_node *node)
{
    while (node && node->type == AST_STATEMENT_LIST) {
        node = node->left;
    }
    return node;
}

/*
 * Anything after return, break, or continue in the same block cannot run.
 * Reported once per block, at the first unreachable statement.
 */
static void warn_if_unreachable(struct sema_ctx *ctx, struct ast_node *statement,
    struct ast_node *rest)
{
    struct ast_node *next;
    const char *keyword;

    if (!statement || !rest) {
        return;
    }

    switch (statement->type) {
        case AST_RETURN:   keyword = "return"; break;
        case AST_BREAK:    keyword = "break"; break;
        case AST_CONTINUE: keyword = "continue"; break;
        default:           return;
    }

    next = first_statement(rest);
    if (!next) {
        return;
    }

    /*
     * A case label, a default label, or a goto target is reachable by jumping
     * to it, so what precedes it says nothing about whether it runs. Only
     * straight-line code after a jump is genuinely unreachable.
     */
    if (next->type == AST_CASE || next->type == AST_DEFAULT ||
        next->type == AST_LABEL) {
        return;
    }

    diag_set_function(ctx->current_function);
    diag_at(DIAG_WARNING, next->location,
        "unreachable statement after '%s'", keyword);
}

void sema_analyze_statement(struct sema_ctx *ctx, struct ast_node *node)
{
    struct ast_node *parts;
    struct ast_node *condition_and_post;

    if (!node) {
        return;
    }

    switch (node->type) {
        case AST_BLOCK:
            analyze_block(ctx, node, 1);
            break;
        case AST_STATEMENT_LIST:
            sema_analyze_statement(ctx, node->left);
            warn_if_unreachable(ctx, node->left, node->right);
            sema_analyze_statement(ctx, node->right);
            break;
        case AST_DECL:
            sema_add_local(ctx, node, node->data_type);
            sema_analyze_expression(ctx, node->left);
            break;
        case AST_EXPR_STMT:
        case AST_RETURN:
            sema_analyze_expression(ctx, node->left);
            break;
        case AST_IF:
            sema_analyze_expression(ctx, node->left);
            sema_analyze_statement(ctx, node->right->left);
            sema_analyze_statement(ctx, node->right->right);
            break;
        case AST_EMPTY:
            break;
        case AST_LABEL:
            sema_analyze_statement(ctx, node->left);
            break;
        case AST_GOTO:
            /* Labels are resolved by the code generator, which sees them all. */
            break;
        case AST_DO_WHILE:
            /*
             * The body runs before the condition is first tested, but both are
             * inside the loop for the purposes of break and continue.
             */
            ctx->loop_depth++;
            sema_analyze_statement(ctx, node->right);
            ctx->loop_depth--;
            sema_analyze_expression(ctx, node->left);
            break;
        case AST_SWITCH:
            sema_analyze_expression(ctx, node->left);
            /*
             * break inside a switch leaves the switch, so it counts as being
             * inside a breakable construct even outside any loop.
             */
            ctx->loop_depth++;
            sema_analyze_statement(ctx, node->right);
            ctx->loop_depth--;
            break;
        case AST_CASE:
        case AST_DEFAULT:
            sema_analyze_statement(ctx, node->left);
            break;
        case AST_WHILE:
            sema_analyze_expression(ctx, node->left);
            ctx->loop_depth++;
            sema_analyze_statement(ctx, node->right);
            ctx->loop_depth--;
            break;
        case AST_FOR:
            sema_enter_scope(ctx);
            parts = node->left;
            condition_and_post = parts->right;
            if (parts->left && parts->left->type == AST_DECL) {
                sema_analyze_statement(ctx, parts->left);
            } else {
                sema_analyze_expression(ctx, parts->left);
            }
            sema_analyze_expression(ctx, condition_and_post->left);
            sema_analyze_expression(ctx, condition_and_post->right);
            ctx->loop_depth++;
            sema_analyze_statement(ctx, node->right);
            ctx->loop_depth--;
            sema_leave_scope(ctx);
            break;
        case AST_BREAK:
            if (ctx->loop_depth == 0) {
                semantic_error_at(ctx, node, "'break' statement is not inside a loop");
            }
            break;
        case AST_CONTINUE:
            if (ctx->loop_depth == 0) {
                semantic_error_at(ctx, node, "'continue' statement is not inside a loop");
            }
            break;
        default:
            sema_analyze_expression(ctx, node);
            break;
    }
}

void sema_collect_top_level(struct sema_ctx *ctx, struct ast_node *node)
{
    if (!node) {
        return;
    }
    if (node->type == AST_PROGRAM) {
        sema_collect_top_level(ctx, node->left);
    } else if (node->type == AST_FUNCTION_LIST) {
        sema_collect_top_level(ctx, node->left);
        sema_collect_top_level(ctx, node->right);
    } else if (node->type == AST_FUNCTION || node->type == AST_FUNCTION_DECL) {
        sema_add_global(ctx, node);
    } else if (node->type == AST_STRUCT_DEF) {
        sema_add_struct(ctx, node);
    } else if (node->type == AST_GLOBAL_DECL) {
        sema_add_global(ctx, node);
    }
}

static int is_constant_expression(struct ast_node *node)
{
    if (!node) {
        return 1;
    }

    switch (node->type) {
        case AST_INTLIT:
        case AST_SIZEOF:
            return 1;
        case AST_INITIALIZER_LIST:
            for (struct ast_node *item = initializer_items(node); item; item = item->right) {
                if (!is_constant_expression(item->left)) {
                    return 0;
                }
            }
            return 1;
        case AST_NEGATION:
        case AST_BITWISE_COMPLEMENT:
        case AST_LOGICAL_NEGATION:
        case AST_CAST:
            return is_constant_expression(node->left);
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
        case AST_LOGICAL_AND:
        case AST_LOGICAL_OR:
        case AST_EQUAL:
        case AST_NOT_EQUAL:
        case AST_LESS:
        case AST_LESS_EQUAL:
        case AST_GREATER:
        case AST_GREATER_EQUAL:
        case AST_COMMA:
            return is_constant_expression(node->left) && is_constant_expression(node->right);
        case AST_CONDITIONAL:
            return is_constant_expression(node->left) &&
                is_constant_expression(node->right->left) &&
                is_constant_expression(node->right->right);
        default:
            return 0;
    }
}

void sema_analyze_top_level(struct sema_ctx *ctx, struct ast_node *node)
{
    struct ast_node *param;

    if (!node) {
        return;
    }
    if (node->type == AST_PROGRAM) {
        sema_analyze_top_level(ctx, node->left);
    } else if (node->type == AST_FUNCTION_LIST) {
        sema_analyze_top_level(ctx, node->left);
        sema_analyze_top_level(ctx, node->right);
    } else if (node->type == AST_GLOBAL_DECL) {
        if (!is_constant_expression(node->left)) {
            semantic_error_at(ctx, node, "initializer for global '%s' is not a constant expression", node->value);
        }
    } else if (node->type == AST_STRUCT_DEF || node->type == AST_FUNCTION_DECL) {
        return;
    } else if (node->type == AST_FUNCTION) {
        int param_index = 0;

        ctx->current_function = node->value;
        ctx->local_count = 0;
        ctx->scope_depth = 1;
        ctx->loop_depth = 0;
        ctx->frame_offset = 0;
        ctx->frame_max = 0;
        ctx->float_param_count = 0;
        ctx->integer_param_count = 0;

        for (param = node->left; param; param = param->right) {
            if (param->type == AST_PARAM_LIST) {
                sema_add_parameter(ctx, param->left, param_index++);
            }
        }
        analyze_block(ctx, node->right, 0);

        /* The frame the code generator must reserve for this function. */
        if (node->sym) {
            node->sym->frame_size = ctx->frame_max;
        }
        ctx->current_function = NULL;
    }
}
