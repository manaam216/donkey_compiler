#include "defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decl.h"

struct ast_node* parse_program(struct token *tokens, int *token_index)
{
    return create_ast_node(AST_PROGRAM, NULL, parse_function_list(tokens, token_index), NULL);
}

struct ast_node* parse_function_list(struct token *tokens, int *token_index)
{
    if (tokens[*token_index].type == T_EOF) {
        return NULL;
    }

    struct ast_node *function = parse_function(tokens, token_index);
    struct ast_node *rest = parse_function_list(tokens, token_index);

    return create_ast_node(AST_FUNCTION_LIST, NULL, function, rest);
}

struct ast_node* parse_function(struct token *tokens, int *token_index)
{
    struct token *tok = &tokens[*token_index];

    if (tok->type != T_INT) {
        fprintf(stderr, "Expected 'int', found %s\n", tok->value);
        exit(1);
    }
    (*token_index)++;

    tok = &tokens[*token_index];
    if (tok->type != T_IDENTIFIER) {
        fprintf(stderr, "Expected identifier, found %d\n", tok->type);
        exit(1);
    }

    char *func_name = tok->value;
    (*token_index)++;

    tok = &tokens[*token_index];
    if (tok->type != T_OPENPAREN) {
        fprintf(stderr, "Expected '(', found %s\n", tok->value);
        exit(1);
    }
    (*token_index)++;

    struct ast_node *params = parse_param_list(tokens, token_index);

    tok = &tokens[*token_index];
    if (tok->type != T_CLOSEPAREN) {
        fprintf(stderr, "Expected ')', found %s\n", tok->value);
        exit(1);
    }
    (*token_index)++;

    struct ast_node *body = parse_block(tokens, token_index);

    return create_ast_node(AST_FUNCTION, func_name, params, body);
}

struct ast_node* parse_param_list(struct token *tokens, int *token_index)
{
    if (tokens[*token_index].type == T_CLOSEPAREN) {
        return NULL;
    }

    if (tokens[*token_index].type != T_INT) {
        fprintf(stderr, "Expected parameter type 'int', found %s\n", tokens[*token_index].value);
        exit(1);
    }
    (*token_index)++;

    struct token *tok = &tokens[*token_index];
    if (tok->type != T_IDENTIFIER) {
        fprintf(stderr, "Expected parameter name, found %s\n", tok->value);
        exit(1);
    }

    struct ast_node *param = create_ast_node(AST_IDENTIFIER, tok->value, NULL, NULL);
    (*token_index)++;

    struct ast_node *rest = NULL;
    if (tokens[*token_index].type == T_COMMA) {
        (*token_index)++;
        rest = parse_param_list(tokens, token_index);
    }

    return create_ast_node(AST_PARAM_LIST, NULL, param, rest);
}

struct ast_node* parse_block(struct token *tokens, int *token_index)
{
    struct token *tok = &tokens[*token_index];

    if (tok->type != T_OPENBRACE) {
        fprintf(stderr, "Expected '{', found %s\n", tok->value);
        exit(1);
    }
    (*token_index)++;

    struct ast_node *statements = parse_statement_list(tokens, token_index);

    tok = &tokens[*token_index];
    if (tok->type != T_CLOSEBRACE) {
        fprintf(stderr, "Expected '}', found %s\n", tok->value);
        exit(1);
    }
    (*token_index)++;

    return create_ast_node(AST_BLOCK, NULL, statements, NULL);
}

struct ast_node* parse_statement_list(struct token *tokens, int *token_index)
{
    if (tokens[*token_index].type == T_CLOSEBRACE) {
        return NULL;
    }

    struct ast_node *stmt = parse_statement(tokens, token_index);
    struct ast_node *rest = parse_statement_list(tokens, token_index);

    return create_ast_node(AST_STATEMENT_LIST, NULL, stmt, rest);
}

struct ast_node* parse_statement(struct token *tokens, int *token_index)
{
    struct token *tok = &tokens[*token_index];

    if (tok->type == T_INT) {
        return parse_declaration(tokens, token_index);
    }

    if (tok->type == T_RETURN) {
        (*token_index)++;

        struct ast_node *exp = parse_exp(tokens, token_index);

        tok = &tokens[*token_index];
        if (tok->type != T_SEMICOLON) {
            fprintf(stderr, "Expected ';', found %s\n", tok->value);
            exit(1);
        }
        (*token_index)++;

        return create_ast_node(AST_RETURN, NULL, exp, NULL);
    }

    struct ast_node *exp = parse_exp(tokens, token_index);

    tok = &tokens[*token_index];
    if (tok->type != T_SEMICOLON) {
        fprintf(stderr, "Expected ';', found %s\n", tok->value);
        exit(1);
    }
    (*token_index)++;

    return create_ast_node(AST_EXPR_STMT, NULL, exp, NULL);
}

struct ast_node* parse_declaration(struct token *tokens, int *token_index)
{
    (*token_index)++;

    struct token *tok = &tokens[*token_index];
    if (tok->type != T_IDENTIFIER) {
        fprintf(stderr, "Expected identifier in declaration, found %s\n", tok->value);
        exit(1);
    }

    char *name = tok->value;
    (*token_index)++;

    struct ast_node *initializer = NULL;
    if (tokens[*token_index].type == T_ASSIGN) {
        (*token_index)++;
        initializer = parse_exp(tokens, token_index);
    }

    tok = &tokens[*token_index];
    if (tok->type != T_SEMICOLON) {
        fprintf(stderr, "Expected ';', found %s\n", tok->value);
        exit(1);
    }
    (*token_index)++;

    return create_ast_node(AST_DECL, name, initializer, NULL);
}

struct ast_node* parse_factor(struct token *tokens, int *token_index)
{
    struct token *tok = &tokens[*token_index];

    if (tok->type == T_MINUS) {
        (*token_index)++;
        return create_ast_node(AST_NEGATION, NULL, parse_factor(tokens, token_index), NULL);
    } else if (tok->type == T_LOGICAL_NEGATION) {
        (*token_index)++;
        return create_ast_node(AST_LOGICAL_NEGATION, NULL, parse_factor(tokens, token_index), NULL);
    } else if (tok->type == T_BITWISE_COMPLEMENT) {
        (*token_index)++;
        return create_ast_node(AST_BITWISE_COMPLEMENT, NULL, parse_factor(tokens, token_index), NULL);
    }

    if (tok->type == T_INTLIT) {
        struct ast_node *lit_node = create_ast_node(AST_INTLIT, tok->value, NULL, NULL);
        (*token_index)++;
        return lit_node;
    }

    if (tok->type == T_IDENTIFIER) {
        char *name = tok->value;
        (*token_index)++;

        if (tokens[*token_index].type == T_OPENPAREN) {
            (*token_index)++;
            struct ast_node *args = parse_arg_list(tokens, token_index);
            if (tokens[*token_index].type != T_CLOSEPAREN) {
                fprintf(stderr, "Expected ')' after function call, found %s\n", tokens[*token_index].value);
                exit(1);
            }
            (*token_index)++;
            return create_ast_node(AST_CALL, name, args, NULL);
        }

        return create_ast_node(AST_IDENTIFIER, name, NULL, NULL);
    }

    if (tok->type == T_OPENPAREN) {
        (*token_index)++;
        struct ast_node *inner_exp = parse_exp(tokens, token_index);
        tok = &tokens[*token_index];
        if (tok->type != T_CLOSEPAREN) {
            fprintf(stderr, "Expected closing parenthesis, found %s\n", tok->value);
            exit(1);
        }
        (*token_index)++;
        return inner_exp;
    }

    fprintf(stderr, "Unexpected token %s in expression parsing\n", tok->value);
    exit(1);
}

struct ast_node* parse_arg_list(struct token *tokens, int *token_index)
{
    if (tokens[*token_index].type == T_CLOSEPAREN) {
        return NULL;
    }

    struct ast_node *arg = parse_exp(tokens, token_index);
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
    return parse_assignment(tokens, token_index);
}

struct ast_node* parse_assignment(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_logical_or(tokens, token_index);

    if (tokens[*token_index].type == T_ASSIGN) {
        if (left->type != AST_IDENTIFIER) {
            fprintf(stderr, "Left side of assignment must be an identifier\n");
            exit(1);
        }

        (*token_index)++;
        return create_ast_node(AST_ASSIGN, NULL, left, parse_assignment(tokens, token_index));
    }

    return left;
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
    struct ast_node *left = parse_additive(tokens, token_index);

    while (1) {
        struct token *tok = &tokens[*token_index];

        if (tok->type == T_LESS) {
            (*token_index)++;
            left = create_ast_node(AST_LESS, NULL, left, parse_additive(tokens, token_index));
        } else if (tok->type == T_LESS_EQUAL) {
            (*token_index)++;
            left = create_ast_node(AST_LESS_EQUAL, NULL, left, parse_additive(tokens, token_index));
        } else if (tok->type == T_GREATER) {
            (*token_index)++;
            left = create_ast_node(AST_GREATER, NULL, left, parse_additive(tokens, token_index));
        } else if (tok->type == T_GREATER_EQUAL) {
            (*token_index)++;
            left = create_ast_node(AST_GREATER_EQUAL, NULL, left, parse_additive(tokens, token_index));
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

struct ast_node* create_ast_node(ASTNodeType type, char *value, struct ast_node *left, struct ast_node *right)
{
    struct ast_node *node = malloc(sizeof(struct ast_node));
    if (!node) {
        perror("Error allocating AST node");
        exit(EXIT_FAILURE);
    }

    node->type = type;
    node->value = value ? strdup(value) : NULL;
    node->left = left;
    node->right = right;
    return node;
}

void free_ast_node(struct ast_node *node)
{
    if (node) {
        free_ast_node(node->left);
        free_ast_node(node->right);
        free(node->value);
        free(node);
    }
}
