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

static CType integer_promotion(CType type)
{
    if (type == TYPE_CHAR || type == TYPE_UCHAR ||
        type == TYPE_SHORT || type == TYPE_USHORT) {
        return TYPE_INT;
    }
    return type;
}

static CType usual_arithmetic_type(CType left, CType right)
{
    /*
     * Floating point outranks every integer type, and double outranks float,
     * so a mixed expression is done at the wider of the two.
     */
    if (left == TYPE_DOUBLE || right == TYPE_DOUBLE) return TYPE_DOUBLE;
    if (left == TYPE_FLOAT || right == TYPE_FLOAT) return TYPE_FLOAT;

    left = integer_promotion(left);
    right = integer_promotion(right);
    if (left == right) return left;
    if (left == TYPE_ULONG || right == TYPE_ULONG) return TYPE_ULONG;
    /*
     * On LP64 a long is wider than an unsigned int and can represent every one
     * of its values, so the unsigned operand converts to long rather than both
     * becoming unsigned. (Where long and int are the same width -- ILP32 --
     * the result would be unsigned long instead.)
     */
    if ((left == TYPE_LONG && right == TYPE_UINT) ||
        (left == TYPE_UINT && right == TYPE_LONG)) return TYPE_LONG;
    if (left == TYPE_UINT || right == TYPE_UINT) return TYPE_UINT;
    if (left == TYPE_LONG || right == TYPE_LONG) return TYPE_LONG;
    return TYPE_INT;
}

static void insert_conversion(struct ast_node **slot, CType target)
{
    struct ast_node *cast;

    if (!slot || !*slot || target == TYPE_INVALID || (*slot)->data_type == target) {
        return;
    }
    cast = create_ast_node(AST_CAST, (char *)semantic_type_name(target), *slot, NULL);
    cast->data_type = target;
    /*
     * Resolve the inserted cast's type here. It is created after its operand
     * has been checked, so nothing else will visit it -- and a floating
     * conversion needs the type, not just the name, to pick its instruction.
     */
    cast->ty = ty_from_name(semantic_type_name(target));
    *slot = cast;
}

static CType check_expression_type(struct sema_ctx *ctx, struct ast_node **slot);
static CType check_expression_type_inner(struct sema_ctx *ctx, struct ast_node **slot);
static void check_initializer_list_types(struct sema_ctx *ctx, struct ast_node *declaration);

static CType check_binary_type(struct sema_ctx *ctx, struct ast_node *node)
{
    CType left = check_expression_type(ctx, &node->left);
    CType right = check_expression_type(ctx, &node->right);
    CType common;
    int left_pointer_depth = semantic_effective_pointer_depth(node->left);
    int right_pointer_depth = semantic_effective_pointer_depth(node->right);

    if (left == TYPE_INVALID || right == TYPE_INVALID) {
        return node->data_type = TYPE_INVALID;
    }
    if (left_pointer_depth > 0 || right_pointer_depth > 0) {
        if (node->type == AST_ADD &&
            left_pointer_depth > 0 &&
            semantic_is_integer(right, right_pointer_depth, node->right->array_length)) {
            node->pointer_depth = left_pointer_depth;
            node->array_length = 0;
            return node->data_type = left;
        }
        if (node->type == AST_ADD &&
            right_pointer_depth > 0 &&
            semantic_is_integer(left, left_pointer_depth, node->left->array_length)) {
            node->pointer_depth = right_pointer_depth;
            node->array_length = 0;
            return node->data_type = right;
        }
        if (node->type == AST_SUB &&
            left_pointer_depth > 0 &&
            right_pointer_depth > 0) {
            if (left != right || left_pointer_depth != right_pointer_depth) {
                semantic_error_at(ctx, node, "cannot subtract incompatible pointer types");
                return node->data_type = TYPE_INVALID;
            }
            node->pointer_depth = 0;
            node->array_length = 0;
            return node->data_type = TYPE_INT;
        }
        if (node->type == AST_SUB &&
            left_pointer_depth > 0 &&
            semantic_is_integer(right, right_pointer_depth, node->right->array_length)) {
            node->pointer_depth = left_pointer_depth;
            node->array_length = 0;
            return node->data_type = left;
        }
        semantic_error_at(ctx, node, "invalid operands to pointer arithmetic");
        return node->data_type = TYPE_INVALID;
    }
    if (node->type == AST_LOGICAL_AND || node->type == AST_LOGICAL_OR) {
        return node->data_type = TYPE_INT;
    }
    if (node->type == AST_SHIFT_LEFT || node->type == AST_SHIFT_RIGHT) {
        left = integer_promotion(left);
        right = integer_promotion(right);
        insert_conversion(&node->left, left);
        insert_conversion(&node->right, right);
        return node->data_type = left;
    }

    common = usual_arithmetic_type(left, right);
    insert_conversion(&node->left, common);
    insert_conversion(&node->right, common);
    if (node->type == AST_EQUAL || node->type == AST_NOT_EQUAL ||
        node->type == AST_LESS || node->type == AST_LESS_EQUAL ||
        node->type == AST_GREATER || node->type == AST_GREATER_EQUAL) {
        return node->data_type = TYPE_INT;
    }
    return node->data_type = common;
}

static CType check_expression_type_inner(struct sema_ctx *ctx, struct ast_node **slot)
{
    struct ast_node *node;
    struct ast_node *argument;
    int local;
    int global;
    int argument_index;
    CType left;
    CType right;
    CType common;
    char left_name[64];
    char right_name[64];

    if (!slot || !*slot) return TYPE_INVALID;
    node = *slot;

    switch (node->type) {
        case AST_INTLIT:
            return node->data_type = TYPE_INT;
        case AST_FLOATLIT:
            /* The parser already decided float or double from the suffix. */
            return node->data_type;
        case AST_STRINGLIT:
            node->pointer_depth = 1;
            return node->data_type = TYPE_CHAR;
        case AST_SIZEOF:
            /* Type the operand so its size is known; it is never evaluated. */
            if (node->left) {
                check_expression_type(ctx, &node->left);
            } else if (node->struct_name) {
                /*
                 * sizeof(struct X): resolve the tag now that the definition
                 * has been collected, and hang the layout on the node.
                 */
                int index = sema_find_struct(ctx, node->struct_name);

                if (index >= 0) {
                    node->ty = ctx->structs[index].ty;
                } else {
                    semantic_error_at(ctx, node, "unknown struct type '%s'",
                        node->struct_name);
                }
            }
            return node->data_type = TYPE_UINT;
        case AST_COMPOUND_LITERAL: {
            /*
             * A compound literal is an unnamed object with the same storage
             * duration as a local, so it is given a frame slot here even though
             * no declaration asked for one.
             */
            struct ast_node *item;

            /*
             * The slot was reserved during name resolution, which is the pass
             * that tracks the frame. Allocating here instead would hand out
             * offsets already given to locals.
             */
            for (item = sema_initializer_items(node->left); item; item = item->right) {
                check_expression_type(ctx, &item->left);
            }
            return node->data_type;
        }
        case AST_INITIALIZER_LIST:
            semantic_error_at(ctx, node, "initializer list is not valid in this expression");
            return node->data_type = TYPE_INVALID;
        case AST_IDENTIFIER:
            local = sema_find_local(ctx, node->value);
            global = sema_find_global(ctx, node->value);
            if (local >= 0) {
                node->sym = ctx->locals[local].sym;
                node->data_type = ctx->locals[local].type;
                node->pointer_depth = ctx->locals[local].pointer_depth;
                node->array_length = ctx->locals[local].array_length;
                memcpy(node->array_dims, ctx->locals[local].array_dims,
                    sizeof(node->array_dims));
                node->array_dim_count = ctx->locals[local].array_dim_count;
                node->is_function_pointer = ctx->locals[local].is_function_pointer;
                node->struct_name = ctx->locals[local].struct_name ? strdup(ctx->locals[local].struct_name) : NULL;
                return node->data_type;
            }
            if (global >= 0 && ctx->globals[global].is_function) {
                node->sym = ctx->globals[global].sym;
                node->is_function_pointer = 1;
                node->pointer_depth = 1;
                return node->data_type = ctx->globals[global].type;
            }
            if (global >= 0 && !ctx->globals[global].is_function) {
                node->sym = ctx->globals[global].sym;
                node->pointer_depth = ctx->globals[global].pointer_depth;
                node->array_length = ctx->globals[global].array_length;
                memcpy(node->array_dims, ctx->globals[global].array_dims,
                    sizeof(node->array_dims));
                node->array_dim_count = ctx->globals[global].array_dim_count;
                node->struct_name = ctx->globals[global].struct_name ? strdup(ctx->globals[global].struct_name) : NULL;
                return node->data_type = ctx->globals[global].type;
            }
            return node->data_type = TYPE_INVALID;
        case AST_FIELD_ACCESS: {
            int struct_index;
            int field_index;
            check_expression_type(ctx, &node->left);
            if (!node->left->struct_name || node->left->pointer_depth > 0) {
                semantic_error_at(ctx, node, "field access requires a struct value");
                return node->data_type = TYPE_INVALID;
            }
            struct_index = sema_find_struct(ctx, node->left->struct_name);
            field_index = sema_find_struct_field(ctx, struct_index, node->value);
            if (field_index < 0) {
                semantic_error_at(ctx, node, "struct '%s' has no field '%s'",
                    node->left->struct_name, node->value);
                return node->data_type = TYPE_INVALID;
            }
            node->data_type = ctx->structs[struct_index].fields[field_index].type;
            node->pointer_depth = ctx->structs[struct_index].fields[field_index].pointer_depth;
            node->array_length = 0;
            return node->data_type;
        }
        case AST_CALL: {
            int callee_local = sema_find_local(ctx, node->value);

            /*
             * A call through a local function pointer takes the pointer's
             * return type; there is no stored signature to check against.
             */
            if (callee_local >= 0 && ctx->locals[callee_local].is_function_pointer) {
                struct ast_node *arg;

                for (arg = node->left; arg; arg = arg->right) {
                    check_expression_type(ctx, &arg->left);
                }
                node->is_indirect_call = 1;
                node->sym = ctx->locals[callee_local].sym;
                return node->data_type = ctx->locals[callee_local].type;
            }
        }
        /* fall through to the ordinary, named call */
        global = sema_find_global(ctx, node->value);
            argument_index = 0;
            for (argument = node->left; argument; argument = argument->right) {
                check_expression_type(ctx, &argument->left);
                /*
                 * Passing a struct by value needs the System V classification
                 * rules -- small ones travel in registers, larger ones on the
                 * stack. That is not implemented, so it is refused rather than
                 * quietly passing the wrong thing; a pointer to it works.
                 */
                /*
                 * System V classifies a struct by size. With no floating-point
                 * types there is only the INTEGER class, so one of eight bytes
                 * or fewer travels in a single register -- which is exactly one
                 * argument slot, the shape the call sequence already has.
                 * Anything larger needs two registers or a stack copy, so it is
                 * refused rather than passed wrongly.
                 */
                if (argument->left && argument->left->ty &&
                    argument->left->ty->kind == TY_STRUCT &&
                    argument->left->ty->size > 16) {
                    semantic_error_at(ctx, argument->left,
                        "cannot pass a struct larger than 16 bytes by value yet; "
                        "pass a pointer to it");
                }
                if (global >= 0 && ctx->globals[global].is_function &&
                    argument_index < ctx->globals[global].parameter_count) {
                    if (semantic_effective_pointer_depth(argument->left) !=
                        ctx->globals[global].parameter_pointer_depths[argument_index]) {
                        semantic_format_type(ctx->globals[global].parameter_types[argument_index],
                            ctx->globals[global].parameter_pointer_depths[argument_index], 0,
                            left_name, sizeof(left_name));
                        semantic_format_type(argument->left->data_type,
                            semantic_effective_pointer_depth(argument->left), 0,
                            right_name, sizeof(right_name));
                        semantic_error_at(ctx, argument->left, "cannot pass %s as %s", right_name, left_name);
                    } else if (semantic_effective_pointer_depth(argument->left) == 0) {
                        insert_conversion(&argument->left,
                            ctx->globals[global].parameter_types[argument_index]);
                    }
                }
                argument_index++;
            }
            if (global >= 0 && ctx->globals[global].is_function) {
                node->pointer_depth = ctx->globals[global].pointer_depth;
                return node->data_type = ctx->globals[global].type;
            }
            return node->data_type = TYPE_INVALID;
        case AST_ADDRESS_OF:
            check_expression_type(ctx, &node->left);
            if (node->left->type != AST_IDENTIFIER &&
                node->left->type != AST_DEREFERENCE &&
                node->left->type != AST_FIELD_ACCESS &&
                node->left->type != AST_ARRAY_SUBSCRIPT) {
                semantic_error_at(ctx, node, "operand of '&' must be an lvalue");
                return node->data_type = TYPE_INVALID;
            }
            node->data_type = node->left->data_type;
            node->pointer_depth = node->left->pointer_depth + 1;
            node->array_length = 0;
            return node->data_type;
        case AST_DEREFERENCE:
            check_expression_type(ctx, &node->left);
            if (node->left->pointer_depth <= 0) {
                semantic_error_at(ctx, node, "cannot dereference non-pointer expression");
                return node->data_type = TYPE_INVALID;
            }
            node->data_type = node->left->data_type;
            node->pointer_depth = node->left->pointer_depth - 1;
            node->array_length = 0;
            /*
             * Carry the struct tag through, so *p and p->field reach the same
             * fields that p.field would on a struct value.
             */
            node->struct_name = node->left->struct_name ?
                strdup(node->left->struct_name) : NULL;
            return node->data_type;
        case AST_ARRAY_SUBSCRIPT:
            check_expression_type(ctx, &node->left);
            check_expression_type(ctx, &node->right);
            if (!semantic_is_integer(node->right->data_type, node->right->pointer_depth,
                    node->right->array_length)) {
                semantic_error_at(ctx, node->right, "array subscript must be an integer");
            }
            if (node->left->array_length <= 0 && node->left->pointer_depth <= 0) {
                semantic_error_at(ctx, node, "subscripted expression is not an array or pointer");
                return node->data_type = TYPE_INVALID;
            }
            node->data_type = node->left->data_type;
            node->pointer_depth = node->left->array_length > 0 ?
                node->left->pointer_depth : node->left->pointer_depth - 1;
            /*
             * Indexing peels off the outermost dimension: an element of
             * `int[2][3]` is an `int[3]`, which is itself still an array.
             */
            if (node->left->array_dim_count > 1) {
                int d;

                node->array_dim_count = node->left->array_dim_count - 1;
                for (d = 0; d < node->array_dim_count; d++) {
                    node->array_dims[d] = node->left->array_dims[d + 1];
                }
                node->array_length = node->array_dims[0];
                node->pointer_depth = node->left->pointer_depth;
            } else {
                node->array_dim_count = 0;
                node->array_length = 0;
            }
            /*
             * Carry the struct tag through the subscript, so an element of a
             * struct array is still a struct and its fields stay accessible.
             */
            node->struct_name = node->left->struct_name ?
                strdup(node->left->struct_name) : NULL;
            return node->data_type;
        case AST_CAST:
            check_expression_type(ctx, &node->left);
            if (node->data_type == TYPE_INVALID)
                node->data_type = semantic_type_from_name(node->value);
            return node->data_type;
        case AST_NEGATION:
        case AST_BITWISE_COMPLEMENT:
            left = integer_promotion(check_expression_type(ctx, &node->left));
            insert_conversion(&node->left, left);
            return node->data_type = left;
        case AST_LOGICAL_NEGATION:
            check_expression_type(ctx, &node->left);
            return node->data_type = TYPE_INT;
        case AST_PRE_INCREMENT:
        case AST_PRE_DECREMENT:
        case AST_POST_INCREMENT:
        case AST_POST_DECREMENT:
            node->data_type = check_expression_type(ctx, &node->left);
            node->pointer_depth = semantic_effective_pointer_depth(node->left);
            return node->data_type;
        case AST_ASSIGN:
            left = check_expression_type(ctx, &node->left);
            check_expression_type(ctx, &node->right);
            if (node->left->array_length > 0) {
                semantic_error_at(ctx, node->left, "cannot assign to array '%s'", node->left->value);
            } else if (!semantic_type_matches(left, semantic_effective_pointer_depth(node->left),
                    node->right->data_type, semantic_effective_pointer_depth(node->right))) {
                if (semantic_effective_pointer_depth(node->left) > 0 ||
                    semantic_effective_pointer_depth(node->right) > 0) {
                    semantic_format_type(left, node->left->pointer_depth, 0,
                        left_name, sizeof(left_name));
                    semantic_format_type(node->right->data_type,
                        semantic_effective_pointer_depth(node->right), 0,
                        right_name, sizeof(right_name));
                    semantic_error_at(ctx, node, "cannot assign %s to %s", right_name, left_name);
                } else {
                    insert_conversion(&node->right, left);
                }
            }
            node->pointer_depth = semantic_effective_pointer_depth(node->left);
            return node->data_type = left;
        case AST_CONDITIONAL:
            check_expression_type(ctx, &node->left);
            left = check_expression_type(ctx, &node->right->left);
            right = check_expression_type(ctx, &node->right->right);
            common = usual_arithmetic_type(left, right);
            insert_conversion(&node->right->left, common);
            insert_conversion(&node->right->right, common);
            return node->data_type = common;
        case AST_COMMA:
            check_expression_type(ctx, &node->left);
            return node->data_type = check_expression_type(ctx, &node->right);
        default:
            return check_binary_type(ctx, node);
    }
}

/*
 * Every expression node carries a resolved type, derived from the fields the
 * checker above computes. The code generator reads these for element sizes and
 * struct offsets instead of assuming 4 bytes.
 */
static CType check_expression_type(struct sema_ctx *ctx, struct ast_node **slot)
{
    CType result = check_expression_type_inner(ctx, slot);

    if (slot && *slot) {
        (*slot)->ty = sema_resolve_type(ctx, *slot);
    }
    return result;
}

static void check_initializer_list_types(struct sema_ctx *ctx, struct ast_node *declaration)
{
    int index = 0;
    struct ast_node *item;

    if (!declaration->left) {
        return;
    }
    if (declaration->left->type != AST_INITIALIZER_LIST) {
        semantic_error_at(ctx, declaration->left, "array initializer must be brace-enclosed");
        return;
    }

    /*
     * A struct is initialised member by member rather than by index, so the
     * limit is how many members it has and each value converts to the type of
     * the one it lands in.
     */
    if (declaration->array_length == 0 && declaration->struct_name) {
        int struct_index = sema_find_struct(ctx, declaration->struct_name);
        int field = 0;

        for (item = sema_initializer_items(declaration->left); item; item = item->right) {
            if (item->left && item->left->designator_field) {
                field = sema_find_struct_field(ctx, struct_index,
                    item->left->designator_field);
                if (field < 0) {
                    semantic_error_at(ctx, item->left, "struct '%s' has no field '%s'",
                        declaration->struct_name, item->left->designator_field);
                    return;
                }
            }
            if (struct_index < 0 || field >= ctx->structs[struct_index].field_count) {
                semantic_error_at(ctx, item->left ? item->left : item,
                    "too many initializers for '%s'", declaration->value);
                return;
            }
            check_expression_type(ctx, &item->left);
            if (item->left) {
                insert_conversion(&item->left,
                    ctx->structs[struct_index].fields[field].type);
            }
            field++;
        }
        return;
    }

    for (item = sema_initializer_items(declaration->left); item; item = item->right) {
        /* A designator places its element; the ones after it follow on. */
        if (item->left && item->left->designator_index >= 0) {
            index = item->left->designator_index;
        }
        if (index >= declaration->array_length) {
            semantic_error_at(ctx, item->left ? item->left : item,
                "initializer for '%s' is outside the array", declaration->value);
            return;
        }
        check_expression_type(ctx, &item->left);
        if (item->left) {
            insert_conversion(&item->left, declaration->data_type);
        }
        index++;
    }
}

static void check_statement_types(struct sema_ctx *ctx, struct ast_node *node)
{
    struct ast_node *parts;
    struct ast_node *condition_and_post;

    if (!node) return;
    switch (node->type) {
        case AST_BLOCK:
            sema_enter_scope(ctx);
            check_statement_types(ctx, node->left);
            sema_leave_scope(ctx);
            break;
        case AST_STATEMENT_LIST:
            check_statement_types(ctx, node->left);
            check_statement_types(ctx, node->right);
            break;
        case AST_DECL:
            sema_add_local(ctx, node, node->data_type);
            if (node->left) {
                if (node->array_length > 0) {
                    check_initializer_list_types(ctx, node);
                } else if (node->left->type == AST_INITIALIZER_LIST) {
                    /* A struct may be brace-initialised too, field by field. */
                    if (!node->struct_name) {
                        semantic_error_at(ctx, node->left,
                            "initializer list is only valid for arrays and structs");
                    } else {
                        check_initializer_list_types(ctx, node);
                    }
                } else {
                    check_expression_type(ctx, &node->left);
                    if (node->pointer_depth > 0 || semantic_effective_pointer_depth(node->left) > 0) {
                    if (!semantic_type_matches(node->data_type, node->pointer_depth,
                            node->left->data_type, semantic_effective_pointer_depth(node->left))) {
                        char left_name[64];
                        char right_name[64];
                        semantic_format_type(node->data_type, node->pointer_depth, 0,
                            left_name, sizeof(left_name));
                        semantic_format_type(node->left->data_type,
                            semantic_effective_pointer_depth(node->left), 0,
                            right_name, sizeof(right_name));
                        semantic_error_at(ctx, node, "cannot initialize %s with %s", left_name, right_name);
                    }
                    } else {
                        insert_conversion(&node->left, node->data_type);
                    }
                }
            }
            break;
        case AST_EXPR_STMT:
            check_expression_type(ctx, &node->left);
            break;
        case AST_RETURN:
            check_expression_type(ctx, &node->left);
            if (ctx->current_return_pointer_depth > 0 || semantic_effective_pointer_depth(node->left) > 0) {
                if (!semantic_type_matches(ctx->current_return_type, ctx->current_return_pointer_depth,
                        node->left->data_type, semantic_effective_pointer_depth(node->left))) {
                    char left_name[64];
                    char right_name[64];
                    semantic_format_type(ctx->current_return_type, ctx->current_return_pointer_depth, 0,
                        left_name, sizeof(left_name));
                    semantic_format_type(node->left->data_type,
                        semantic_effective_pointer_depth(node->left), 0,
                        right_name, sizeof(right_name));
                    semantic_error_at(ctx, node, "cannot return %s from function returning %s",
                        right_name, left_name);
                }
            } else {
                insert_conversion(&node->left, ctx->current_return_type);
            }
            break;
        case AST_IF:
            check_expression_type(ctx, &node->left);
            check_statement_types(ctx, node->right->left);
            check_statement_types(ctx, node->right->right);
            break;
        case AST_EMPTY:
        case AST_GOTO:
            break;
        case AST_LABEL:
        case AST_CASE:
        case AST_DEFAULT:
            check_statement_types(ctx, node->left);
            break;
        case AST_DO_WHILE:
            check_expression_type(ctx, &node->left);
            check_statement_types(ctx, node->right);
            break;
        case AST_SWITCH:
            check_expression_type(ctx, &node->left);
            check_statement_types(ctx, node->right);
            break;
        case AST_WHILE:
            check_expression_type(ctx, &node->left);
            check_statement_types(ctx, node->right);
            break;
        case AST_FOR:
            sema_enter_scope(ctx);
            parts = node->left;
            condition_and_post = parts->right;
            if (parts->left && parts->left->type == AST_DECL)
                check_statement_types(ctx, parts->left);
            else
                check_expression_type(ctx, &parts->left);
            check_expression_type(ctx, &condition_and_post->left);
            check_expression_type(ctx, &condition_and_post->right);
            check_statement_types(ctx, node->right);
            sema_leave_scope(ctx);
            break;
        default:
            break;
    }
}

void sema_check_top_level_types(struct sema_ctx *ctx, struct ast_node *node)
{
    struct ast_node *param;

    if (!node) return;
    if (node->type == AST_PROGRAM) {
        sema_check_top_level_types(ctx, node->left);
    } else if (node->type == AST_FUNCTION_LIST) {
        sema_check_top_level_types(ctx, node->left);
        sema_check_top_level_types(ctx, node->right);
    } else if (node->type == AST_GLOBAL_DECL) {
        if (node->left) {
            if (node->array_length > 0) {
                check_initializer_list_types(ctx, node);
            } else if (node->left->type == AST_INITIALIZER_LIST) {
                if (!node->struct_name) {
                    semantic_error_at(ctx, node->left,
                        "initializer list is only valid for arrays and structs");
                }
            } else {
                check_expression_type(ctx, &node->left);
                insert_conversion(&node->left, node->data_type);
            }
        }
    } else if (node->type == AST_STRUCT_DEF || node->type == AST_FUNCTION_DECL) {
        return;
    } else if (node->type == AST_FUNCTION) {
        ctx->current_return_type = node->data_type;
        ctx->current_return_pointer_depth = node->pointer_depth;
        ctx->local_count = 0;
        ctx->scope_depth = 1;
        ctx->frame_offset = 0;
        ctx->float_param_count = 0;
        ctx->integer_param_count = 0;
        {
            int param_index = 0;
            for (param = node->left; param; param = param->right)
                sema_add_parameter(ctx, param->left, param_index++);
        }
        if (node->right) check_statement_types(ctx, node->right->left);
    }
}
