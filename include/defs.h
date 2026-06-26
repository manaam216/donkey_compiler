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
    T_QUESTION,
    T_COLON,
    T_CHAR,
    T_SHORT,
    T_INT,
    T_LONG,
    T_SIGNED,
    T_UNSIGNED,
    T_RETURN,
    T_IF,
    T_ELSE,
    T_WHILE,
    T_FOR,
    T_BREAK,
    T_CONTINUE,
    T_SIZEOF,
    T_IDENTIFIER,
    T_INTLIT,
    T_BITWISE_COMPLEMENT,
    T_LOGICAL_NEGATION,
    T_PLUS,
    T_PLUS_PLUS,
    T_PLUS_ASSIGN,
    T_STAR,
    T_STAR_ASSIGN,
    T_SLASH,
    T_SLASH_ASSIGN,
    T_MINUS,
    T_MINUS_MINUS,
    T_MINUS_ASSIGN,
    T_PERCENT,
    T_PERCENT_ASSIGN,
    T_AMPERSAND,
    T_AMPERSAND_ASSIGN,
    T_PIPE,
    T_PIPE_ASSIGN,
    T_CARET,
    T_CARET_ASSIGN,
    T_SHIFT_LEFT,
    T_SHIFT_RIGHT,
    T_SHIFT_LEFT_ASSIGN,
    T_SHIFT_RIGHT_ASSIGN,
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
    AST_GLOBAL_DECL,
    AST_BLOCK,
    AST_STATEMENT_LIST,
    AST_RETURN,
    AST_DECL,
    AST_EXPR_STMT,
    AST_PARAM_LIST,
    AST_ARG_LIST,
    AST_CONDITIONAL,
    AST_CONDITIONAL_BRANCHES,
    AST_IF,
    AST_IF_BRANCHES,
    AST_WHILE,
    AST_FOR,
    AST_FOR_PARTS,
    AST_BREAK,
    AST_CONTINUE,
    AST_CALL,
    AST_CAST,
    AST_SIZEOF,
    AST_INTLIT,
    AST_IDENTIFIER,
    AST_NEGATION,
    AST_BITWISE_COMPLEMENT,
    AST_LOGICAL_NEGATION,
    AST_PRE_INCREMENT,
    AST_PRE_DECREMENT,
    AST_POST_INCREMENT,
    AST_POST_DECREMENT,
    AST_ADD,
    AST_SUB,
    AST_MUL,
    AST_DIV,
    AST_MOD,
    AST_SHIFT_LEFT,
    AST_SHIFT_RIGHT,
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
    AST_ASSIGN,
    AST_COMMA
} ASTNodeType;

typedef enum {
    TYPE_INVALID,
    TYPE_CHAR,
    TYPE_UCHAR,
    TYPE_SHORT,
    TYPE_USHORT,
    TYPE_INT,
    TYPE_UINT,
    TYPE_LONG,
    TYPE_ULONG
} CType;

typedef struct {
    int line;
    int column;
} SourceLocation;

struct ast_node {
    ASTNodeType type;
    CType data_type;
    SourceLocation location;
    struct ast_node *left;
    struct ast_node *right;
    char *value;
};

struct token {
    TokenType type;
    SourceLocation location;
    char *value;
};

#endif
