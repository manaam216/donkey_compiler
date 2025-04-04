#include "defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decl.h"

/// parse_program now accepts a copy of the tokens array
struct ast_node* parse_program(struct token *tokens, int *token_index) {
    return parse_function(tokens, token_index);  // A program is just one function
}

// Parse a function
struct ast_node* parse_function(struct token *tokens, int *token_index) {
    struct token *tok = &tokens[*token_index];

    // Match "int"
    if (tok->type != T_INT) {
        fprintf(stderr, "Expected 'int', found %s\n", tok->value);
        exit(1);
    }
    (*token_index)++;

    // Match identifier (function name)
    tok = &tokens[*token_index];
    if (tok->type != T_IDENTIFIER) {
        fprintf(stderr, "Expected identifier, found %d\n", tok->type);
        exit(1);
    }

    char *func_name = tok->value;
    (*token_index)++;

    // Match opening parenthesis
    tok = &tokens[*token_index];
    if (tok->type != T_OPENPAREN) {
        fprintf(stderr, "Expected '(', found %s\n", tok->value);
        exit(1);
    }
    (*token_index)++;

    // Match closing parenthesis
    tok = &tokens[*token_index];
    if (tok->type != T_CLOSEPAREN) {
        fprintf(stderr, "Expected ')', found %s\n", tok->value);
        exit(1);
    }
    (*token_index)++;

    // Match opening brace
    tok = &tokens[*token_index];
    if (tok->type != T_OPENBRACE) {
        fprintf(stderr, "Expected '{', found %s\n", tok->value);
        exit(1);
    }
    (*token_index)++;

    // Parse the statement inside the function
    struct ast_node *stmt = parse_statement(tokens, token_index);

    // Match closing brace
    tok = &tokens[*token_index];
    if (tok->type != T_CLOSEBRACE) {
        fprintf(stderr, "Expected '}', found %s\n", tok->value);
        exit(1);
    }
    (*token_index)++;

    // Return the function node
    return create_ast_node(AST_FUNCTION, func_name, stmt, NULL);
}

// Parse a statement (only return statement in this simple grammar)
struct ast_node* parse_statement(struct token *tokens, int *token_index) {
    struct token *tok = &tokens[*token_index];

    // Match "return"
    if (tok->type != T_RETURN) {
        fprintf(stderr, "Expected 'return', found %s\n", tok->value);
        exit(1);
    }
    (*token_index)++;

    // Parse the expression (in this case, just an integer literal)
    struct ast_node *exp = parse_exp(tokens, token_index);

    // Match semicolon
    tok = &tokens[*token_index];
    if (tok->type != T_SEMICOLON) {
        fprintf(stderr, "Expected ';', found %s\n", tok->value);
        exit(1);
    }
    (*token_index)++;

    // Return the return statement AST node
    return create_ast_node(AST_RETURN, NULL, exp, NULL);
}

struct ast_node* parse_factor(struct token *tokens, int *token_index) {
    struct token *tok = &tokens[*token_index];

    // Handle unary negation (e.g. -3)
    if (tok->type == T_LOGICAL_NEGATION) {
        (*token_index)++;
        struct ast_node *operand = parse_factor(tokens, token_index);  // Recursively parse the operand
        return create_ast_node(AST_LOGICAL_NEGATION, NULL, operand, NULL);
    }

    // Handle bitwise complement (e.g. ~x)
    else if (tok->type == T_BITWISE_COMPLEMENT) {
        (*token_index)++;
        struct ast_node *operand = parse_factor(tokens, token_index);  // Recursively parse the operand
        return create_ast_node(AST_BITWISE_COMPLEMENT, NULL, operand, NULL);
    }

    // Handle integer literal (e.g. 3)
    if (tok->type == T_INTLIT) {
        struct ast_node *lit_node = create_ast_node(AST_INTLIT, tok->value, NULL, NULL);
        (*token_index)++;
        return lit_node;
    }

    // Handle identifier (e.g. variable names)
    if (tok->type == T_IDENTIFIER) {
        struct ast_node *id_node = create_ast_node(AST_IDENTIFIER, tok->value, NULL, NULL);
        (*token_index)++;
        return id_node;
    }

    // Handle opening parenthesis (e.g. (exp))
    if (tok->type == T_OPENPAREN) {
        (*token_index)++;  // Move past the opening parenthesis
        struct ast_node *inner_exp = parse_exp(tokens, token_index);  // Parse the inner expression
        tok = &tokens[*token_index];
        if (tok->type != T_CLOSEPAREN) {
            fprintf(stderr, "Expected closing parenthesis, found %s\n", tok->value);
            exit(1);
        }
        (*token_index)++;  // Move past the closing parenthesis
        return inner_exp;
    }

    // Error if no valid expression found
    fprintf(stderr, "Unexpected token %s in expression parsing\n", tok->value);
    exit(1);
}


struct ast_node* parse_term(struct token *tokens, int *token_index) {
    struct ast_node *left = parse_factor(tokens, token_index);  // Parse the first factor

    // Handle multiplication and division
    while (1) {
        struct token *tok = &tokens[*token_index];

        if (tok->type == T_STAR) {
            (*token_index)++;
            struct ast_node *right = parse_factor(tokens, token_index);  // Parse the right factor
            left = create_ast_node(AST_MUL, NULL, left, right);           // Create AST node for multiplication
        } else if (tok->type == T_SLASH) {
            (*token_index)++;
            struct ast_node *right = parse_factor(tokens, token_index);  // Parse the right factor
            left = create_ast_node(AST_DIV, NULL, left, right);           // Create AST node for division
        } else {
            break;  // No more multiplication or division operators
        }
    }

    return left;
}

struct ast_node* parse_exp(struct token *tokens, int *token_index) {
    struct ast_node *left = parse_term(tokens, token_index);  // Parse the first term

    // Handle binary operations (+, -, *, /)
    while (1) {
        struct token *tok = &tokens[*token_index];

        if (tok->type == T_PLUS) {
            (*token_index)++;
            struct ast_node *right = parse_term(tokens, token_index);  // Parse the right term
            left = create_ast_node(AST_ADD, NULL, left, right);         // Create AST node for addition
        } else if (tok->type == T_MINUS) {
            (*token_index)++;
            struct ast_node *right = parse_term(tokens, token_index);  // Parse the right term
            left = create_ast_node(AST_SUB, NULL, left, right);         // Create AST node for subtraction
        } else {
            break;  // No more addition or subtraction operators
        }
    }

    return left;
}

// Function to create new AST nodes
struct ast_node* create_ast_node(ASTNodeType type, char *value, struct ast_node *left, struct ast_node *right) {
    struct ast_node *node = (struct ast_node*)malloc(sizeof(struct ast_node));
    node->type = type;
    node->value = value ? strdup(value) : NULL;
    node->left = left;
    node->right = right;
    return node;
}

void free_ast_node(struct ast_node *node) {
    if (node) {
        if (node->value) free(node->value);
        free(node);
    }
}