#ifndef DONKEY_DEFS_H
#define DONKEY_DEFS_H

#include <stdio.h>

typedef enum {
    T_OPENBRACE, T_CLOSEBRACE, T_OPENPAREN, T_CLOSEPAREN,
    T_SEMICOLON, T_INT, T_RETURN, T_IDENTIFIER, T_INTLIT, 
    T_NEG,             // Negation -
    T_BITWISE_COMPLEMENT, // Bitwise complement ~
    T_LOGICAL_NEGATION,   // Logical negation !
    T_PLUS, T_STAR, T_SLASH, T_MINUS,
    T_INVALID
} TokenType;


typedef enum {
    AST_PROGRAM,
    AST_FUNCTION,
    AST_RETURN,
    AST_INTLIT,
    AST_IDENTIFIER,
    AST_NEGATION,           // For the negation (-) operator
    AST_BITWISE_COMPLEMENT, // For the bitwise complement (~) operator
    AST_LOGICAL_NEGATION,   // For the logical negation (!) operator
    AST_ADD,                // For the addition (+) operator
    AST_SUB,                // For the subtraction (-) operator
    AST_MUL,                // For the multiplication (*) operator
    AST_DIV,                // For the division (/) operator
    // Add other AST node types as necessary
} ASTNodeType;




// Abstract Syntax Tree (AST) nodes
struct ast_node {
    ASTNodeType type;
    struct ast_node *left;    // For children (if any)
    struct ast_node *right;   // For children (if any)
    char *value;              // For identifiers or literals
};

// Token structure
struct token {
    TokenType type;
    char *value;
};

#endif // DONKEY_DEFS_H
