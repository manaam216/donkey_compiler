/*
 * Parser: expressions. One function per precedence level, from assignment down
 * to primary expressions and their postfix operators.
 */
#include "defs.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decl.h"
#include "diag.h"
#include "parser_internal.h"

static struct ast_node *clone_expression(const struct ast_node *node)
{
    struct ast_node *copy;

    if (!node) {
        return NULL;
    }

    copy = create_ast_node_at(node->type, node->value,
        clone_expression(node->left), clone_expression(node->right),
        node->location);
    copy->data_type = node->data_type;
    copy->pointer_depth = node->pointer_depth;
    copy->array_length = node->array_length;
    memcpy(copy->array_dims, node->array_dims, sizeof(copy->array_dims));
    copy->array_dim_count = node->array_dim_count;
    copy->struct_name = node->struct_name ? strdup(node->struct_name) : NULL;
    copy->string_label = node->string_label;
    return copy;
}

static int is_repeatable(const struct ast_node *node)
{
    if (!node) {
        return 1;
    }
    if (node->type == AST_CALL || node->type == AST_ASSIGN ||
        node->type == AST_PRE_INCREMENT || node->type == AST_POST_INCREMENT ||
        node->type == AST_PRE_DECREMENT || node->type == AST_POST_DECREMENT) {
        return 0;
    }
    return is_repeatable(node->left) && is_repeatable(node->right);
}

static struct ast_node *parse_postfix(struct ast_node *id, struct token *tokens,
    int *token_index)
{
    while (tokens[*token_index].type == T_OPENBRACKET ||
           tokens[*token_index].type == T_DOT ||
           tokens[*token_index].type == T_ARROW) {
        if (tokens[*token_index].type == T_ARROW) {
        /* p->field means (*p).field, and is built as exactly that. */
        SourceLocation arrow_location = tokens[*token_index].location;

        (*token_index)++;
        if (tokens[*token_index].type != T_IDENTIFIER) {
            parse_error_at(&tokens[*token_index],
                "expected a field name after '->'");
            break;
        }
        id = create_ast_node_at(AST_FIELD_ACCESS, tokens[*token_index].value,
            create_ast_node_at(AST_DEREFERENCE, NULL, id, NULL, arrow_location),
            NULL, arrow_location);
        (*token_index)++;
        continue;
        }
        if (tokens[*token_index].type == T_DOT) {
        SourceLocation dot_location = tokens[*token_index].location;
        (*token_index)++;
        if (tokens[*token_index].type != T_IDENTIFIER) {
            parse_error_at(&tokens[*token_index], "expected field name, found '%s'",
                tokens[*token_index].value);
        }
        id = create_ast_node_at(AST_FIELD_ACCESS, tokens[*token_index].value, id, NULL,
            dot_location);
        (*token_index)++;
        continue;
        }
        SourceLocation bracket_location = tokens[*token_index].location;
        (*token_index)++;
        struct ast_node *index = parse_exp(tokens, token_index);
        if (tokens[*token_index].type != T_CLOSEBRACKET) {
        parse_error_at(&tokens[*token_index], "expected ']', found '%s'",
            tokens[*token_index].value);
        } else {
        (*token_index)++;
        }
        id = create_ast_node_at(AST_ARRAY_SUBSCRIPT, NULL, id, index, bracket_location);
    }

    return id;
}

struct ast_node* parse_factor(struct token *tokens, int *token_index)
{
    struct token *tok = &tokens[*token_index];

    if (tok->type == T_PLUS_PLUS) {
        SourceLocation operator_location = tok->location;
        (*token_index)++;
        struct ast_node *operand = parse_factor(tokens, token_index);
        if (operand->type != AST_IDENTIFIER) {
        }
        return create_ast_node_at(AST_PRE_INCREMENT, NULL, operand, NULL, operator_location);
    } else if (tok->type == T_MINUS_MINUS) {
        SourceLocation operator_location = tok->location;
        (*token_index)++;
        struct ast_node *operand = parse_factor(tokens, token_index);
        if (operand->type != AST_IDENTIFIER) {
        }
        return create_ast_node_at(AST_PRE_DECREMENT, NULL, operand, NULL, operator_location);
    } else if (tok->type == T_SIZEOF) {
        SourceLocation sizeof_location = tok->location;
        (*token_index)++;
        if (tokens[*token_index].type == T_OPENPAREN) {
            int type_index = *token_index + 1;
            const char *type_name;

            /*
             * `sizeof(struct X)` names a type, not an expression. The struct's
             * size is not known until its definition has been collected, so
             * the tag is recorded and semantic analysis resolves it.
             */
            if (tokens[type_index].type == T_STRUCT &&
                tokens[type_index + 1].type == T_IDENTIFIER &&
                tokens[type_index + 2].type == T_CLOSEPAREN) {
                struct ast_node *size = create_ast_node_at(AST_SIZEOF, NULL,
                    NULL, NULL, sizeof_location);

                size->struct_name = strdup(tokens[type_index + 1].value);
                size->data_type = TYPE_UINT;
                *token_index = type_index + 3;
                return size;
            }

            type_name = parse_type_name(tokens, &type_index);
            if (type_name && tokens[type_index].type == T_CLOSEPAREN) {
                *token_index = type_index + 1;
                struct ast_node *size = create_ast_node_at(AST_SIZEOF, (char *)type_name, NULL, NULL,
                    sizeof_location);
                size->data_type = TYPE_UINT;
                return size;
            }
        }

        /*
         * Keep the operand: its resolved type is what determines the size.
         * It is never evaluated, only measured.
         */
        struct ast_node *operand = parse_factor(tokens, token_index);
        struct ast_node *size = create_ast_node_at(AST_SIZEOF, NULL, operand, NULL,
            sizeof_location);
        size->data_type = TYPE_UINT;
        return size;
    } else if (tok->type == T_MINUS) {
        SourceLocation operator_location = tok->location;
        (*token_index)++;
        return create_ast_node_at(AST_NEGATION, NULL, parse_factor(tokens, token_index), NULL,
            operator_location);
    } else if (tok->type == T_LOGICAL_NEGATION) {
        SourceLocation operator_location = tok->location;
        (*token_index)++;
        return create_ast_node_at(AST_LOGICAL_NEGATION, NULL, parse_factor(tokens, token_index), NULL,
            operator_location);
    } else if (tok->type == T_BITWISE_COMPLEMENT) {
        SourceLocation operator_location = tok->location;
        (*token_index)++;
        return create_ast_node_at(AST_BITWISE_COMPLEMENT, NULL, parse_factor(tokens, token_index), NULL,
            operator_location);
    } else if (tok->type == T_AMPERSAND) {
        SourceLocation operator_location = tok->location;
        (*token_index)++;
        return create_ast_node_at(AST_ADDRESS_OF, NULL, parse_factor(tokens, token_index), NULL,
            operator_location);
    } else if (tok->type == T_STAR) {
        SourceLocation operator_location = tok->location;
        (*token_index)++;
        return create_ast_node_at(AST_DEREFERENCE, NULL, parse_factor(tokens, token_index), NULL,
            operator_location);
    }

    if (tok->type == T_FLOATLIT) {
        struct ast_node *literal = create_ast_node_at(AST_FLOATLIT, tok->value,
            NULL, NULL, tok->location);

        /* An f suffix makes it a float; otherwise a floating literal is double. */
        literal->data_type = strchr(tok->value, 'f') ? TYPE_FLOAT : TYPE_DOUBLE;
        (*token_index)++;
        return literal;
    }

    if (tok->type == T_INTLIT || tok->type == T_CHARLIT) {
        struct ast_node *lit_node = create_ast_node_at(AST_INTLIT, tok->value, NULL, NULL,
            tok->location);
        (*token_index)++;
        return lit_node;
    }

    if (tok->type == T_STRINGLIT) {
        struct ast_node *str_node = create_ast_node_at(AST_STRINGLIT, tok->value, NULL, NULL,
            tok->location);
        str_node->data_type = TYPE_CHAR;
        str_node->pointer_depth = 1;
        (*token_index)++;
        return str_node;
    }

    /*
     * An enum constant is a compile-time integer, so it becomes a literal here
     * and nothing downstream needs to know enums exist.
     */
    if (tok->type == T_IDENTIFIER && find_enum_constant(tok->value)) {
        const struct enum_constant *constant = find_enum_constant(tok->value);
        char text[32];
        struct ast_node *literal;

        snprintf(text, sizeof(text), "%ld", constant->value);
        literal = create_ast_node_at(AST_INTLIT, text, NULL, NULL, tok->location);
        literal->data_type = TYPE_INT;
        (*token_index)++;
        return literal;
    }

    if (tok->type == T_IDENTIFIER) {
        char *name = tok->value;
        SourceLocation identifier_location = tok->location;
        (*token_index)++;

        if (tokens[*token_index].type == T_OPENPAREN) {
            (*token_index)++;
            struct ast_node *args = parse_arg_list(tokens, token_index);
            if (tokens[*token_index].type != T_CLOSEPAREN) {
                parse_error_at(&tokens[*token_index], "expected ')' after function call, found '%s'",
                    tokens[*token_index].value);
            } else {
                (*token_index)++;
            }
            struct ast_node *call = create_ast_node_at(AST_CALL, name, args, NULL,
                identifier_location);
            if (tokens[*token_index].type == T_PLUS_PLUS || tokens[*token_index].type == T_MINUS_MINUS) {
                parse_error_at(&tokens[*token_index],
                    "postfix increment/decrement requires an identifier");
            }
            return call;
        }

        struct ast_node *id = create_ast_node_at(AST_IDENTIFIER, name, NULL, NULL,
            identifier_location);
        
        id = parse_postfix(id, tokens, token_index);
        if (tokens[*token_index].type == T_PLUS_PLUS) {
            SourceLocation operator_location = tokens[*token_index].location;
            (*token_index)++;
            return create_ast_node_at(AST_POST_INCREMENT, NULL, id, NULL, operator_location);
        } else if (tokens[*token_index].type == T_MINUS_MINUS) {
            SourceLocation operator_location = tokens[*token_index].location;
            (*token_index)++;
            return create_ast_node_at(AST_POST_DECREMENT, NULL, id, NULL, operator_location);
        }

        return id;
    }

    if (tok->type == T_OPENPAREN) {
        SourceLocation paren_location = tok->location;
        (*token_index)++;
        int type_index = *token_index;
        const char *struct_tag = NULL;
        const char *type_name;

        /*
         * A compound literal, `(struct Point){1, 2}` or `(int[3]){1, 2, 3}`,
         * looks like a cast until the brace. It names a type and then an
         * initializer, and yields an unnamed object of that type.
         */
        if (tokens[type_index].type == T_STRUCT &&
            tokens[type_index + 1].type == T_IDENTIFIER) {
            struct_tag = tokens[type_index + 1].value;
            type_index += 2;
            type_name = "int";
        } else {
            type_name = parse_type_name(tokens, &type_index);
        }

        if ((type_name || struct_tag) && tokens[type_index].type == T_CLOSEPAREN &&
            tokens[type_index + 1].type == T_OPENBRACE) {
            struct ast_node *literal;

            *token_index = type_index + 1;
            literal = create_ast_node_at(AST_COMPOUND_LITERAL, NULL,
                parse_initializer(tokens, token_index), NULL, paren_location);
            literal->data_type = type_from_name(type_name);
            literal->struct_name = struct_tag ? strdup(struct_tag) : NULL;
            return parse_postfix(literal, tokens, token_index);
        }

        /* Not a compound literal after all: an ordinary cast, or a group. */
        type_index = *token_index;
        type_name = parse_type_name(tokens, &type_index);
        if (type_name && tokens[type_index].type == T_CLOSEPAREN) {
            *token_index = type_index + 1;
            struct ast_node *cast = create_ast_node_at(AST_CAST, (char *)type_name,
                parse_factor(tokens, token_index), NULL, paren_location);
            cast->data_type = type_from_name(type_name);
            return cast;
        }

        struct ast_node *inner_exp = parse_exp(tokens, token_index);
        tok = &tokens[*token_index];
        if (tok->type != T_CLOSEPAREN) {
            parse_error_at(tok, "expected closing parenthesis, found '%s'", tok->value);
        } else {
            (*token_index)++;
        }
        /* A parenthesised expression is a primary, so `(*p)[1]` works. */
        return parse_postfix(inner_exp, tokens, token_index);
    }

    parse_error_at(tok, "unexpected token '%s' in expression parsing", tok->value);
    return NULL;
}

struct ast_node* parse_arg_list(struct token *tokens, int *token_index)
{
    if (tokens[*token_index].type == T_CLOSEPAREN) {
        return NULL;
    }

    struct ast_node *arg = parse_assignment(tokens, token_index);
    struct ast_node *rest = NULL;

    if (tokens[*token_index].type == T_COMMA) {
        (*token_index)++;
        rest = parse_arg_list(tokens, token_index);
    }

    return create_ast_node(AST_ARG_LIST, NULL, arg, rest);
}

struct ast_node* parse_term(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_factor(tokens, token_index);

    while (1) {
        struct token *tok = &tokens[*token_index];

        if (tok->type == T_STAR) {
            (*token_index)++;
            left = create_ast_node(AST_MUL, NULL, left, parse_factor(tokens, token_index));
        } else if (tok->type == T_SLASH) {
            (*token_index)++;
            left = create_ast_node(AST_DIV, NULL, left, parse_factor(tokens, token_index));
        } else if (tok->type == T_PERCENT) {
            (*token_index)++;
            left = create_ast_node(AST_MOD, NULL, left, parse_factor(tokens, token_index));
        } else {
            break;
        }
    }

    return left;
}

struct ast_node* parse_exp(struct token *tokens, int *token_index)
{
    return parse_comma(tokens, token_index);
}

struct ast_node* parse_comma(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_assignment(tokens, token_index);

    while (tokens[*token_index].type == T_COMMA) {
        (*token_index)++;
        left = create_ast_node(AST_COMMA, NULL, left, parse_assignment(tokens, token_index));
    }

    return left;
}

struct ast_node* parse_assignment(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_conditional(tokens, token_index);
    TokenType op = tokens[*token_index].type;

    if (op == T_ASSIGN || op == T_PLUS_ASSIGN || op == T_MINUS_ASSIGN ||
        op == T_STAR_ASSIGN || op == T_SLASH_ASSIGN || op == T_PERCENT_ASSIGN ||
        op == T_AMPERSAND_ASSIGN || op == T_PIPE_ASSIGN || op == T_CARET_ASSIGN ||
        op == T_SHIFT_LEFT_ASSIGN || op == T_SHIFT_RIGHT_ASSIGN) {
        if (left->type != AST_IDENTIFIER &&
            left->type != AST_DEREFERENCE &&
            left->type != AST_FIELD_ACCESS &&
            left->type != AST_ARRAY_SUBSCRIPT) {
            parse_error_at(&tokens[*token_index], "left side of assignment must be an identifier");
        }

        SourceLocation operator_location = tokens[*token_index].location;
        (*token_index)++;
        struct ast_node *right = parse_assignment(tokens, token_index);

        if (op == T_ASSIGN) {
            return create_ast_node_at(AST_ASSIGN, NULL, left, right, operator_location);
        }
        if (!is_repeatable(left)) {
            parse_error_at(&tokens[*token_index],
                "compound assignment target must not have side effects");
        }

        ASTNodeType binop = AST_ADD;
        if (op == T_MINUS_ASSIGN) binop = AST_SUB;
        else if (op == T_STAR_ASSIGN) binop = AST_MUL;
        else if (op == T_SLASH_ASSIGN) binop = AST_DIV;
        else if (op == T_PERCENT_ASSIGN) binop = AST_MOD;
        else if (op == T_AMPERSAND_ASSIGN) binop = AST_BITWISE_AND;
        else if (op == T_PIPE_ASSIGN) binop = AST_BITWISE_OR;
        else if (op == T_CARET_ASSIGN) binop = AST_BITWISE_XOR;
        else if (op == T_SHIFT_LEFT_ASSIGN) binop = AST_SHIFT_LEFT;
        else if (op == T_SHIFT_RIGHT_ASSIGN) binop = AST_SHIFT_RIGHT;

        return create_ast_node(
            AST_ASSIGN,
            NULL,
            left,
            create_ast_node_at(binop, NULL,
                clone_expression(left),
                right,
                operator_location)
        );
    }

    return left;
}

struct ast_node* parse_conditional(struct token *tokens, int *token_index)
{
    struct ast_node *cond = parse_logical_or(tokens, token_index);

    if (tokens[*token_index].type == T_QUESTION) {
        (*token_index)++;
        struct ast_node *then_exp = parse_exp(tokens, token_index);

        if (tokens[*token_index].type != T_COLON) {
            parse_error_at(&tokens[*token_index],
                "expected ':' in conditional expression, found '%s'",
                tokens[*token_index].value);
        } else {
            (*token_index)++;
        }

        struct ast_node *else_exp = parse_conditional(tokens, token_index);
        return create_ast_node(
            AST_CONDITIONAL,
            NULL,
            cond,
            create_ast_node(AST_CONDITIONAL_BRANCHES, NULL, then_exp, else_exp)
        );
    }

    return cond;
}

struct ast_node* parse_logical_or(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_logical_and(tokens, token_index);

    while (tokens[*token_index].type == T_LOGICAL_OR) {
        (*token_index)++;
        left = create_ast_node(AST_LOGICAL_OR, NULL, left, parse_logical_and(tokens, token_index));
    }

    return left;
}

struct ast_node* parse_logical_and(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_bitwise_or(tokens, token_index);

    while (tokens[*token_index].type == T_LOGICAL_AND) {
        (*token_index)++;
        left = create_ast_node(AST_LOGICAL_AND, NULL, left, parse_bitwise_or(tokens, token_index));
    }

    return left;
}

struct ast_node* parse_bitwise_or(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_bitwise_xor(tokens, token_index);

    while (tokens[*token_index].type == T_PIPE) {
        (*token_index)++;
        left = create_ast_node(AST_BITWISE_OR, NULL, left, parse_bitwise_xor(tokens, token_index));
    }

    return left;
}

struct ast_node* parse_bitwise_xor(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_bitwise_and(tokens, token_index);

    while (tokens[*token_index].type == T_CARET) {
        (*token_index)++;
        left = create_ast_node(AST_BITWISE_XOR, NULL, left, parse_bitwise_and(tokens, token_index));
    }

    return left;
}

struct ast_node* parse_bitwise_and(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_equality(tokens, token_index);

    while (tokens[*token_index].type == T_AMPERSAND) {
        (*token_index)++;
        left = create_ast_node(AST_BITWISE_AND, NULL, left, parse_equality(tokens, token_index));
    }

    return left;
}

struct ast_node* parse_equality(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_relational(tokens, token_index);

    while (1) {
        struct token *tok = &tokens[*token_index];

        if (tok->type == T_EQUAL) {
            (*token_index)++;
            left = create_ast_node(AST_EQUAL, NULL, left, parse_relational(tokens, token_index));
        } else if (tok->type == T_NOT_EQUAL) {
            (*token_index)++;
            left = create_ast_node(AST_NOT_EQUAL, NULL, left, parse_relational(tokens, token_index));
        } else {
            break;
        }
    }

    return left;
}

struct ast_node* parse_relational(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_shift(tokens, token_index);

    while (1) {
        struct token *tok = &tokens[*token_index];

        if (tok->type == T_LESS) {
            (*token_index)++;
            left = create_ast_node(AST_LESS, NULL, left, parse_shift(tokens, token_index));
        } else if (tok->type == T_LESS_EQUAL) {
            (*token_index)++;
            left = create_ast_node(AST_LESS_EQUAL, NULL, left, parse_shift(tokens, token_index));
        } else if (tok->type == T_GREATER) {
            (*token_index)++;
            left = create_ast_node(AST_GREATER, NULL, left, parse_shift(tokens, token_index));
        } else if (tok->type == T_GREATER_EQUAL) {
            (*token_index)++;
            left = create_ast_node(AST_GREATER_EQUAL, NULL, left, parse_shift(tokens, token_index));
        } else {
            break;
        }
    }

    return left;
}

struct ast_node* parse_shift(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_additive(tokens, token_index);

    while (1) {
        struct token *tok = &tokens[*token_index];

        if (tok->type == T_SHIFT_LEFT) {
            (*token_index)++;
            left = create_ast_node(AST_SHIFT_LEFT, NULL, left, parse_additive(tokens, token_index));
        } else if (tok->type == T_SHIFT_RIGHT) {
            (*token_index)++;
            left = create_ast_node(AST_SHIFT_RIGHT, NULL, left, parse_additive(tokens, token_index));
        } else {
            break;
        }
    }

    return left;
}

struct ast_node* parse_additive(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_term(tokens, token_index);

    while (1) {
        struct token *tok = &tokens[*token_index];

        if (tok->type == T_PLUS) {
            (*token_index)++;
            left = create_ast_node(AST_ADD, NULL, left, parse_term(tokens, token_index));
        } else if (tok->type == T_MINUS) {
            (*token_index)++;
            left = create_ast_node(AST_SUB, NULL, left, parse_term(tokens, token_index));
        } else {
            break;
        }
    }

    return left;
}
