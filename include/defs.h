#ifndef DONKEY_DEFS_H
#define DONKEY_DEFS_H

#include <stdio.h>

typedef enum {
    T_OPENBRACE,
    T_CLOSEBRACE,
    T_OPENPAREN,
    T_CLOSEPAREN,
    T_SEMICOLON,
    T_COMMA,
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
    T_PERCENT,
    T_AMPERSAND,
    T_PIPE,
    T_CARET,
    T_LOGICAL_AND,
    T_LOGICAL_OR,
    T_EQUAL,
    T_NOT_EQUAL,
    T_ASSIGN,
    T_LESS,
    T_LESS_EQUAL,
    T_GREATER,
    T_GREATER_EQUAL,
    T_EOF,
    T_INVALID
} TokenType;

typedef enum {
    AST_PROGRAM,
    AST_FUNCTION_LIST,
    AST_FUNCTION,
    AST_BLOCK,
    AST_STATEMENT_LIST,
    AST_RETURN,
    AST_DECL,
    AST_EXPR_STMT,
    AST_PARAM_LIST,
    AST_ARG_LIST,
    AST_CALL,
    AST_INTLIT,
    AST_IDENTIFIER,
    AST_NEGATION,
    AST_BITWISE_COMPLEMENT,
    AST_LOGICAL_NEGATION,
    AST_ADD,
    AST_SUB,
    AST_MUL,
    AST_DIV,
    AST_MOD,
    AST_BITWISE_AND,
    AST_BITWISE_OR,
    AST_BITWISE_XOR,
    AST_LOGICAL_AND,
    AST_LOGICAL_OR,
    AST_EQUAL,
    AST_NOT_EQUAL,
    AST_LESS,
    AST_LESS_EQUAL,
    AST_GREATER,
    AST_GREATER_EQUAL,
    AST_ASSIGN
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
