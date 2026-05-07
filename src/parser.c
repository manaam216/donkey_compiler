#include "defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decl.h"

struct ast_node* parse_program(struct token *tokens, int *token_index)
{
    struct ast_node *function = parse_function(tokens, token_index);

    if (tokens[*token_index].type != T_EOF) {
        fprintf(stderr, "Unexpected token after function: %s\n", tokens[*token_index].value);
        exit(1);
    }

    return function;
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

    tok = &tokens[*token_index];
    if (tok->type != T_CLOSEPAREN) {
        fprintf(stderr, "Expected ')', found %s\n", tok->value);
        exit(1);
    }
    (*token_index)++;

    tok = &tokens[*token_index];
    if (tok->type != T_OPENBRACE) {
        fprintf(stderr, "Expected '{', found %s\n", tok->value);
        exit(1);
    }
    (*token_index)++;

    struct ast_node *stmt = parse_statement(tokens, token_index);

    tok = &tokens[*token_index];
    if (tok->type != T_CLOSEBRACE) {
        fprintf(stderr, "Expected '}', found %s\n", tok->value);
        exit(1);
    }
    (*token_index)++;

    return create_ast_node(AST_FUNCTION, func_name, stmt, NULL);
}

struct ast_node* parse_statement(struct token *tokens, int *token_index)
{
    struct token *tok = &tokens[*token_index];

    if (tok->type != T_RETURN) {
        fprintf(stderr, "Expected 'return', found %s\n", tok->value);
        exit(1);
    }
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
        struct ast_node *id_node = create_ast_node(AST_IDENTIFIER, tok->value, NULL, NULL);
        (*token_index)++;
        return id_node;
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
        } else {
            break;
        }
    }

    return left;
}

struct ast_node* parse_exp(struct token *tokens, int *token_index)
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
