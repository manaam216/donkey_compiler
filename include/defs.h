#ifndef DONKEY_DEFS_H
#define DONKEY_DEFS_H

#include <stdio.h>

typedef enum {
    T_OPENBRACE,
    T_CLOSEBRACE,
    T_OPENPAREN,
    T_CLOSEPAREN,
    T_OPENBRACKET,
    T_CLOSEBRACKET,
    T_SEMICOLON,
    T_COMMA,
    T_QUESTION,
    T_COLON,
    T_DOT,
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
    T_STRUCT,
    T_SIZEOF,
    T_IDENTIFIER,
    T_INTLIT,
    T_CHARLIT,
    T_STRINGLIT,
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
    T_HASH,
    T_HASH_HASH,
    T_EOF,
    T_INVALID
} TokenType;

typedef enum {
    AST_PROGRAM,
    AST_FUNCTION_LIST,
    AST_FUNCTION,
    AST_GLOBAL_DECL,
    AST_STRUCT_DEF,
    AST_FIELD_LIST,
    AST_BLOCK,
    AST_STATEMENT_LIST,
    AST_RETURN,
    AST_DECL,
    AST_EXPR_STMT,
    AST_PARAM_LIST,
    AST_ARG_LIST,
    AST_INITIALIZER_LIST,
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
    AST_ADDRESS_OF,
    AST_DEREFERENCE,
    AST_ARRAY_SUBSCRIPT,
    AST_FIELD_ACCESS,
    AST_INTLIT,
    AST_STRINGLIT,
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

    /*
     * The file this position is in. Once #include exists a token may come from
     * somewhere other than the file named on the command line, and a
     * diagnostic that quoted the wrong file would be worse than one that
     * quoted nothing. NULL means the translation unit's own file.
     */
    const char *file;
} SourceLocation;

struct ast_node {
    ASTNodeType type;

    /*
     * Syntactic type as written in the source. The parser fills these in; it
     * cannot build a full type because a struct's layout is not known until
     * its definition has been collected.
     */
    CType data_type;
    int pointer_depth;
    int array_length;
    char *struct_name;

    /*
     * Resolved type, filled in by semantic analysis. This is what the code
     * generator reads for sizes, offsets, and pointer scaling -- the fields
     * above are the input to resolution, not the type model itself.
     */
    struct Type *ty;

    /*
     * Storage identity, filled in by semantic analysis. On declarations and
     * identifier references this is the variable's symbol; on functions and
     * calls it is the function's. The code generator reads offsets from here
     * rather than resolving names a second time.
     */
    struct Symbol *sym;

    int string_label;
    SourceLocation location;
    struct ast_node *left;
    struct ast_node *right;
    char *value;
};

struct token {
    TokenType type;
    SourceLocation location;
    char *value;

    /*
     * First token on its line. A preprocessing directive is recognised by a
     * '#' in that position, and a directive runs until the next token that
     * starts a line -- which is how the token stream stands in for the
     * newlines the lexer discards.
     */
    int at_line_start;
};

#endif
