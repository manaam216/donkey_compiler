#include <stdio.h>
#include <string.h>
#include "defs.h"
#include "decl.h"
#include "type.h"
#include "symbol.h"
#include "dump.h"

/*
 * Debug dumps of the token stream and the syntax tree.
 *
 * These exist to make the compiler's own intermediate state inspectable. Every
 * later stage -- the preprocessor, the wider declarator grammar, the IR -- is
 * easier to debug against a printed tree than against generated assembly.
 */

static const char *token_kind_name(int kind)
{
    switch (kind) {
        case T_OPENBRACE: return "openbrace";
        case T_CLOSEBRACE: return "closebrace";
        case T_OPENPAREN: return "openparen";
        case T_CLOSEPAREN: return "closeparen";
        case T_OPENBRACKET: return "openbracket";
        case T_CLOSEBRACKET: return "closebracket";
        case T_SEMICOLON: return "semicolon";
        case T_COMMA: return "comma";
        case T_QUESTION: return "question";
        case T_COLON: return "colon";
        case T_DOT: return "dot";
        case T_CHAR: return "char";
        case T_SHORT: return "short";
        case T_INT: return "int";
        case T_LONG: return "long";
        case T_SIGNED: return "signed";
        case T_UNSIGNED: return "unsigned";
        case T_RETURN: return "return";
        case T_IF: return "if";
        case T_ELSE: return "else";
        case T_WHILE: return "while";
        case T_DO: return "do";
        case T_FOR: return "for";
        case T_SWITCH: return "switch";
        case T_CASE: return "case";
        case T_DEFAULT: return "default";
        case T_GOTO: return "goto";
        case T_BREAK: return "break";
        case T_CONTINUE: return "continue";
        case T_STRUCT: return "struct";
        case T_UNION: return "union";
        case T_ENUM: return "enum";
        case T_VOID: return "void";
        case T_FLOAT: return "float";
        case T_DOUBLE: return "double";
        case T_TYPEDEF: return "typedef";
        case T_EXTERN: return "extern";
        case T_STATIC: return "static";
        case T_CONST: return "const";
        case T_VOLATILE: return "volatile";
        case T_ELLIPSIS: return "ellipsis";
        case T_SIZEOF: return "sizeof";
        case T_IDENTIFIER: return "identifier";
        case T_INTLIT: return "intlit";
        case T_FLOATLIT: return "floatlit";
        case T_CHARLIT: return "charlit";
        case T_STRINGLIT: return "stringlit";
        case T_BITWISE_COMPLEMENT: return "bitwise_complement";
        case T_LOGICAL_NEGATION: return "logical_negation";
        case T_PLUS: return "plus";
        case T_PLUS_PLUS: return "plus_plus";
        case T_PLUS_ASSIGN: return "plus_assign";
        case T_STAR: return "star";
        case T_STAR_ASSIGN: return "star_assign";
        case T_SLASH: return "slash";
        case T_SLASH_ASSIGN: return "slash_assign";
        case T_MINUS: return "minus";
        case T_MINUS_MINUS: return "minus_minus";
        case T_ARROW: return "arrow";
        case T_MINUS_ASSIGN: return "minus_assign";
        case T_PERCENT: return "percent";
        case T_PERCENT_ASSIGN: return "percent_assign";
        case T_AMPERSAND: return "ampersand";
        case T_AMPERSAND_ASSIGN: return "ampersand_assign";
        case T_PIPE: return "pipe";
        case T_PIPE_ASSIGN: return "pipe_assign";
        case T_CARET: return "caret";
        case T_CARET_ASSIGN: return "caret_assign";
        case T_SHIFT_LEFT: return "shift_left";
        case T_SHIFT_RIGHT: return "shift_right";
        case T_SHIFT_LEFT_ASSIGN: return "shift_left_assign";
        case T_SHIFT_RIGHT_ASSIGN: return "shift_right_assign";
        case T_LOGICAL_AND: return "logical_and";
        case T_LOGICAL_OR: return "logical_or";
        case T_EQUAL: return "equal";
        case T_NOT_EQUAL: return "not_equal";
        case T_ASSIGN: return "assign";
        case T_LESS: return "less";
        case T_LESS_EQUAL: return "less_equal";
        case T_GREATER: return "greater";
        case T_GREATER_EQUAL: return "greater_equal";
        case T_HASH: return "hash";
        case T_HASH_HASH: return "hash_hash";
        case T_EOF: return "eof";
        case T_INVALID: return "invalid";
        default: return "?";
    }
}

static const char *ast_kind_name(int kind)
{
    switch (kind) {
        case AST_PROGRAM: return "program";
        case AST_FUNCTION_LIST: return "function_list";
        case AST_FUNCTION: return "function";
        case AST_FUNCTION_DECL: return "function_decl";
        case AST_GLOBAL_DECL: return "global_decl";
        case AST_STRUCT_DEF: return "struct_def";
        case AST_FIELD_LIST: return "field_list";
        case AST_BLOCK: return "block";
        case AST_STATEMENT_LIST: return "statement_list";
        case AST_RETURN: return "return";
        case AST_DECL: return "decl";
        case AST_EXPR_STMT: return "expr_stmt";
        case AST_PARAM_LIST: return "param_list";
        case AST_ARG_LIST: return "arg_list";
        case AST_INITIALIZER_LIST: return "initializer_list";
        case AST_COMPOUND_LITERAL: return "compound_literal";
        case AST_CONDITIONAL: return "conditional";
        case AST_CONDITIONAL_BRANCHES: return "conditional_branches";
        case AST_IF: return "if";
        case AST_IF_BRANCHES: return "if_branches";
        case AST_WHILE: return "while";
        case AST_DO_WHILE: return "do_while";
        case AST_FOR: return "for";
        case AST_SWITCH: return "switch";
        case AST_SWITCH_BODY: return "switch_body";
        case AST_CASE: return "case";
        case AST_DEFAULT: return "default";
        case AST_GOTO: return "goto";
        case AST_LABEL: return "label";
        case AST_EMPTY: return "empty";
        case AST_FOR_PARTS: return "for_parts";
        case AST_BREAK: return "break";
        case AST_CONTINUE: return "continue";
        case AST_CALL: return "call";
        case AST_CAST: return "cast";
        case AST_SIZEOF: return "sizeof";
        case AST_ADDRESS_OF: return "address_of";
        case AST_DEREFERENCE: return "dereference";
        case AST_ARRAY_SUBSCRIPT: return "array_subscript";
        case AST_FIELD_ACCESS: return "field_access";
        case AST_INTLIT: return "intlit";
        case AST_FLOATLIT: return "floatlit";
        case AST_STRINGLIT: return "stringlit";
        case AST_IDENTIFIER: return "identifier";
        case AST_NEGATION: return "negation";
        case AST_BITWISE_COMPLEMENT: return "bitwise_complement";
        case AST_LOGICAL_NEGATION: return "logical_negation";
        case AST_PRE_INCREMENT: return "pre_increment";
        case AST_PRE_DECREMENT: return "pre_decrement";
        case AST_POST_INCREMENT: return "post_increment";
        case AST_POST_DECREMENT: return "post_decrement";
        case AST_ADD: return "add";
        case AST_SUB: return "sub";
        case AST_MUL: return "mul";
        case AST_DIV: return "div";
        case AST_MOD: return "mod";
        case AST_SHIFT_LEFT: return "shift_left";
        case AST_SHIFT_RIGHT: return "shift_right";
        case AST_BITWISE_AND: return "bitwise_and";
        case AST_BITWISE_OR: return "bitwise_or";
        case AST_BITWISE_XOR: return "bitwise_xor";
        case AST_LOGICAL_AND: return "logical_and";
        case AST_LOGICAL_OR: return "logical_or";
        case AST_EQUAL: return "equal";
        case AST_NOT_EQUAL: return "not_equal";
        case AST_LESS: return "less";
        case AST_LESS_EQUAL: return "less_equal";
        case AST_GREATER: return "greater";
        case AST_GREATER_EQUAL: return "greater_equal";
        case AST_ASSIGN: return "assign";
        case AST_COMMA: return "comma";
        default: return "?";
    }
}

void dump_tokens(const struct token *tokens, int token_count)
{
    int i;

    printf("%-5s %-8s %-18s %s\n", "#", "LINE:COL", "KIND", "TEXT");
    for (i = 0; i < token_count; i++) {
        char position[16];

        snprintf(position, sizeof(position), "%d:%d",
            tokens[i].location.line, tokens[i].location.column);
        printf("%-5d %-8s %-18s %s\n", i, position,
            token_kind_name(tokens[i].type),
            tokens[i].value ? tokens[i].value : "");
    }
    printf("%d tokens\n", token_count);
}

/* Describe whatever semantic analysis managed to attach to a node. */
static void print_annotations(const struct ast_node *node)
{
    if (node->ty) {
        char rendered[128];

        ty_format(node->ty, rendered, sizeof(rendered));
        printf(" : %s", rendered);
    }
    if (node->sym) {
        const char *storage = "?";

        switch (node->sym->kind) {
            case SYM_LOCAL:    storage = "local"; break;
            case SYM_PARAM:    storage = "param"; break;
            case SYM_GLOBAL:   storage = "global"; break;
            case SYM_FUNCTION: storage = "function"; break;
        }
        printf(" [%s", storage);
        if (node->sym->kind == SYM_LOCAL || node->sym->kind == SYM_PARAM) {
            printf(" %+d(%%rbp)", node->sym->offset);
        }
        if (node->sym->kind == SYM_FUNCTION && node->sym->frame_size) {
            printf(" frame=%d", node->sym->frame_size);
        }
        printf("]");
    }
}

static void dump_node(const struct ast_node *node, int depth)
{
    int i;

    if (!node) {
        return;
    }

    for (i = 0; i < depth; i++) {
        printf("  ");
    }

    printf("%s", ast_kind_name(node->type));
    if (node->value) {
        printf(" '%s'", node->value);
    }
    print_annotations(node);
    if (node->location.line > 0) {
        printf("   <%d:%d>", node->location.line, node->location.column);
    }
    printf("\n");

    dump_node(node->left, depth + 1);
    dump_node(node->right, depth + 1);
}

void dump_ast(const struct ast_node *root)
{
    dump_node(root, 0);
}
