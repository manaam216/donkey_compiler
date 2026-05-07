#ifndef DONKEY_DEFS_H
#define DONKEY_DEFS_H

#include <stdio.h>

typedef enum {
    T_OPENBRACE,
    T_CLOSEBRACE,
    T_OPENPAREN,
    T_CLOSEPAREN,
    T_SEMICOLON,
    T_INT,
    T_RETURN,
    T_IDENTIFIER,
    T_INTLIT,
    T_BITWISE_COMPLEMENT,
    T_LOGICAL_NEGATION,
    T_PLUS,
    T_STAR,
    T_SLASH,
    T_MINUS,
    T_EOF,
    T_INVALID
} TokenType;

typedef enum {
    AST_PROGRAM,
    AST_FUNCTION,
    AST_RETURN,
    AST_INTLIT,
    AST_IDENTIFIER,
    AST_NEGATION,
    AST_BITWISE_COMPLEMENT,
    AST_LOGICAL_NEGATION,
    AST_ADD,
    AST_SUB,
    AST_MUL,
    AST_DIV
} ASTNodeType;

struct ast_node {
    ASTNodeType type;
    struct ast_node *left;
    struct ast_node *right;
    char *value;
};

struct token {
    TokenType type;
    char *value;
};

#endif
