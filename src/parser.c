#include "defs.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decl.h"

static const char *parser_source_path;

static void parse_error_at(struct token *token, const char *format, ...)
{
    va_list args;

    fprintf(stderr, "Parse error at %s:%d:%d: ",
        parser_source_path, token->location.line, token->location.column);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

static const char* parse_type_name(struct token *tokens, int *token_index)
{
    int is_unsigned = 0;
    int has_sign = 0;
    TokenType base_type;

    if (tokens[*token_index].type == T_SIGNED || tokens[*token_index].type == T_UNSIGNED) {
        is_unsigned = tokens[*token_index].type == T_UNSIGNED;
        has_sign = 1;
        (*token_index)++;
    }

    base_type = tokens[*token_index].type;
    if (base_type == T_CHAR || base_type == T_SHORT || base_type == T_INT || base_type == T_LONG) {
        (*token_index)++;
    } else if (has_sign) {
        base_type = T_INT;
    } else {
        return NULL;
    }

    if ((base_type == T_SHORT || base_type == T_LONG) && tokens[*token_index].type == T_INT) {
        (*token_index)++;
    }

    if (base_type == T_CHAR) {
        return is_unsigned ? "uchar" : "char";
    }
    if (base_type == T_SHORT) {
        return is_unsigned ? "ushort" : "short";
    }
    if (base_type == T_LONG) {
        return is_unsigned ? "ulong" : "long";
    }

    return is_unsigned ? "uint" : "int";
}

static CType type_from_name(const char *name)
{
    if (strcmp(name, "char") == 0) return TYPE_CHAR;
    if (strcmp(name, "uchar") == 0) return TYPE_UCHAR;
    if (strcmp(name, "short") == 0) return TYPE_SHORT;
    if (strcmp(name, "ushort") == 0) return TYPE_USHORT;
    if (strcmp(name, "uint") == 0) return TYPE_UINT;
    if (strcmp(name, "long") == 0) return TYPE_LONG;
    if (strcmp(name, "ulong") == 0) return TYPE_ULONG;
    return TYPE_INT;
}

static int is_type_start(TokenType type)
{
    return type == T_CHAR || type == T_SHORT || type == T_INT ||
        type == T_LONG || type == T_SIGNED || type == T_UNSIGNED;
}

struct ast_node* parse_program(struct token *tokens, int *token_index, const char *source_path)
{
    parser_source_path = source_path;
    return create_ast_node_at(AST_PROGRAM, NULL, parse_function_list(tokens, token_index), NULL,
        tokens[*token_index].location);
}

struct ast_node* parse_function_list(struct token *tokens, int *token_index)
{
    if (tokens[*token_index].type == T_EOF) {
        return NULL;
    }

    struct ast_node *function = parse_external_declaration(tokens, token_index);
    struct ast_node *rest = parse_function_list(tokens, token_index);

    return create_ast_node(AST_FUNCTION_LIST, NULL, function, rest);
}

struct ast_node* parse_external_declaration(struct token *tokens, int *token_index)
{
    int name_index = *token_index;

    if (!parse_type_name(tokens, &name_index)) {
        parse_error_at(&tokens[*token_index], "expected top-level declaration, found '%s'",
            tokens[*token_index].value);
    }

    if (tokens[name_index].type != T_IDENTIFIER) {
        parse_error_at(&tokens[name_index], "expected identifier in top-level declaration, found '%s'",
            tokens[name_index].value);
    }

    if (tokens[name_index + 1].type == T_OPENPAREN) {
        return parse_function(tokens, token_index);
    }

    return parse_global_declaration(tokens, token_index);
}

struct ast_node* parse_function(struct token *tokens, int *token_index)
{
    const char *type_name = parse_type_name(tokens, token_index);
    struct token *tok;

    if (!type_name) {
        parse_error_at(&tokens[*token_index], "expected function return type, found '%s'",
            tokens[*token_index].value);
    }

    tok = &tokens[*token_index];
    if (tok->type != T_IDENTIFIER) {
        parse_error_at(tok, "expected identifier, found '%s'", tok->value);
    }

    char *func_name = tok->value;
    SourceLocation function_location = tok->location;
    (*token_index)++;

    tok = &tokens[*token_index];
    if (tok->type != T_OPENPAREN) {
        parse_error_at(tok, "expected '(', found '%s'", tok->value);
    }
    (*token_index)++;

    struct ast_node *params = parse_param_list(tokens, token_index);

    tok = &tokens[*token_index];
    if (tok->type != T_CLOSEPAREN) {
        parse_error_at(tok, "expected ')', found '%s'", tok->value);
    }
    (*token_index)++;

    struct ast_node *body = parse_block(tokens, token_index);

    struct ast_node *function = create_ast_node_at(AST_FUNCTION, func_name, params, body, function_location);
    function->data_type = type_from_name(type_name);
    return function;
}

struct ast_node* parse_global_declaration(struct token *tokens, int *token_index)
{
    const char *type_name = parse_type_name(tokens, token_index);

    struct token *tok = &tokens[*token_index];
    if (tok->type != T_IDENTIFIER) {
        parse_error_at(tok, "expected global variable name, found '%s'", tok->value);
    }

    char *name = tok->value;
    SourceLocation declaration_location = tok->location;
    (*token_index)++;

    struct ast_node *initializer = NULL;
    if (tokens[*token_index].type == T_ASSIGN) {
        (*token_index)++;
        initializer = parse_exp(tokens, token_index);
    }

    if (tokens[*token_index].type != T_SEMICOLON) {
        parse_error_at(&tokens[*token_index], "expected ';' after global declaration, found '%s'",
            tokens[*token_index].value);
    }
    (*token_index)++;

    struct ast_node *declaration = create_ast_node_at(AST_GLOBAL_DECL, name, initializer, NULL,
        declaration_location);
    declaration->data_type = type_from_name(type_name);
    return declaration;
}

struct ast_node* parse_param_list(struct token *tokens, int *token_index)
{
    if (tokens[*token_index].type == T_CLOSEPAREN) {
        return NULL;
    }

    const char *type_name = parse_type_name(tokens, token_index);
    if (!type_name) {
        parse_error_at(&tokens[*token_index], "expected parameter type, found '%s'",
            tokens[*token_index].value);
    }

    struct token *tok = &tokens[*token_index];
    if (tok->type != T_IDENTIFIER) {
        parse_error_at(tok, "expected parameter name, found '%s'", tok->value);
    }

    struct ast_node *param = create_ast_node_at(AST_IDENTIFIER, tok->value, NULL, NULL, tok->location);
    param->data_type = type_from_name(type_name);
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
        parse_error_at(tok, "expected '{', found '%s'", tok->value);
    }
    SourceLocation block_location = tok->location;
    (*token_index)++;

    struct ast_node *statements = parse_statement_list(tokens, token_index);

    tok = &tokens[*token_index];
    if (tok->type != T_CLOSEBRACE) {
        parse_error_at(tok, "expected '}', found '%s'", tok->value);
    }
    (*token_index)++;

    return create_ast_node_at(AST_BLOCK, NULL, statements, NULL, block_location);
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

    if (tok->type == T_OPENBRACE) {
        return parse_block(tokens, token_index);
    }

    if (is_type_start(tok->type)) {
        return parse_declaration(tokens, token_index);
    }

    if (tok->type == T_IF) {
        return parse_if_statement(tokens, token_index);
    }

    if (tok->type == T_WHILE) {
        return parse_while_statement(tokens, token_index);
    }

    if (tok->type == T_FOR) {
        return parse_for_statement(tokens, token_index);
    }

    if (tok->type == T_BREAK) {
        SourceLocation break_location = tok->location;
        (*token_index)++;
        tok = &tokens[*token_index];
        if (tok->type != T_SEMICOLON) {
            parse_error_at(tok, "expected ';', found '%s'", tok->value);
        }
        (*token_index)++;
        return create_ast_node_at(AST_BREAK, NULL, NULL, NULL, break_location);
    }

    if (tok->type == T_CONTINUE) {
        SourceLocation continue_location = tok->location;
        (*token_index)++;
        tok = &tokens[*token_index];
        if (tok->type != T_SEMICOLON) {
            parse_error_at(tok, "expected ';', found '%s'", tok->value);
        }
        (*token_index)++;
        return create_ast_node_at(AST_CONTINUE, NULL, NULL, NULL, continue_location);
    }

    if (tok->type == T_RETURN) {
        SourceLocation return_location = tok->location;
        (*token_index)++;

        struct ast_node *exp = parse_exp(tokens, token_index);

        tok = &tokens[*token_index];
        if (tok->type != T_SEMICOLON) {
            parse_error_at(tok, "expected ';', found '%s'", tok->value);
        }
        (*token_index)++;

        return create_ast_node_at(AST_RETURN, NULL, exp, NULL, return_location);
    }

    struct ast_node *exp = parse_exp(tokens, token_index);

    tok = &tokens[*token_index];
    if (tok->type != T_SEMICOLON) {
        parse_error_at(tok, "expected ';', found '%s'", tok->value);
    }
    (*token_index)++;

    return create_ast_node_at(AST_EXPR_STMT, NULL, exp, NULL, exp->location);
}

struct ast_node* parse_declaration(struct token *tokens, int *token_index)
{
    const char *type_name = parse_type_name(tokens, token_index);

    struct token *tok = &tokens[*token_index];
    if (tok->type != T_IDENTIFIER) {
        parse_error_at(tok, "expected identifier in declaration, found '%s'", tok->value);
    }

    char *name = tok->value;
    SourceLocation declaration_location = tok->location;
    (*token_index)++;

    struct ast_node *initializer = NULL;
    if (tokens[*token_index].type == T_ASSIGN) {
        (*token_index)++;
        initializer = parse_exp(tokens, token_index);
    }

    tok = &tokens[*token_index];
    if (tok->type != T_SEMICOLON) {
        parse_error_at(tok, "expected ';', found '%s'", tok->value);
    }
    (*token_index)++;

    struct ast_node *declaration = create_ast_node_at(AST_DECL, name, initializer, NULL,
        declaration_location);
    declaration->data_type = type_from_name(type_name);
    return declaration;
}

struct ast_node* parse_if_statement(struct token *tokens, int *token_index)
{
    SourceLocation if_location = tokens[*token_index].location;
    (*token_index)++;

    if (tokens[*token_index].type != T_OPENPAREN) {
        parse_error_at(&tokens[*token_index], "expected '(' after if, found '%s'",
            tokens[*token_index].value);
    }
    (*token_index)++;

    struct ast_node *cond = parse_exp(tokens, token_index);

    if (tokens[*token_index].type != T_CLOSEPAREN) {
        parse_error_at(&tokens[*token_index], "expected ')' after if condition, found '%s'",
            tokens[*token_index].value);
    }
    (*token_index)++;

    struct ast_node *then_stmt = parse_statement(tokens, token_index);
    struct ast_node *else_stmt = NULL;

    if (tokens[*token_index].type == T_ELSE) {
        (*token_index)++;
        else_stmt = parse_statement(tokens, token_index);
    }

    return create_ast_node_at(AST_IF, NULL, cond,
        create_ast_node_at(AST_IF_BRANCHES, NULL, then_stmt, else_stmt, if_location),
        if_location);
}

struct ast_node* parse_while_statement(struct token *tokens, int *token_index)
{
    SourceLocation while_location = tokens[*token_index].location;
    (*token_index)++;

    if (tokens[*token_index].type != T_OPENPAREN) {
        parse_error_at(&tokens[*token_index], "expected '(' after while, found '%s'",
            tokens[*token_index].value);
    }
    (*token_index)++;

    struct ast_node *cond = parse_exp(tokens, token_index);

    if (tokens[*token_index].type != T_CLOSEPAREN) {
        parse_error_at(&tokens[*token_index], "expected ')' after while condition, found '%s'",
            tokens[*token_index].value);
    }
    (*token_index)++;

    return create_ast_node_at(AST_WHILE, NULL, cond, parse_statement(tokens, token_index),
        while_location);
}

struct ast_node* parse_for_statement(struct token *tokens, int *token_index)
{
    SourceLocation for_location = tokens[*token_index].location;
    (*token_index)++;

    if (tokens[*token_index].type != T_OPENPAREN) {
        parse_error_at(&tokens[*token_index], "expected '(' after for, found '%s'",
            tokens[*token_index].value);
    }
    (*token_index)++;

    struct ast_node *init = parse_for_init(tokens, token_index);

    struct ast_node *cond = parse_optional_exp(tokens, token_index);
    if (tokens[*token_index].type != T_SEMICOLON) {
        parse_error_at(&tokens[*token_index], "expected ';' after for condition, found '%s'",
            tokens[*token_index].value);
    }
    (*token_index)++;

    struct ast_node *post = parse_optional_exp(tokens, token_index);
    if (tokens[*token_index].type != T_CLOSEPAREN) {
        parse_error_at(&tokens[*token_index], "expected ')' after for clauses, found '%s'",
            tokens[*token_index].value);
    }
    (*token_index)++;

    struct ast_node *cond_post = create_ast_node_at(AST_FOR_PARTS, NULL, cond, post, for_location);
    struct ast_node *parts = create_ast_node_at(AST_FOR_PARTS, NULL, init, cond_post, for_location);

    return create_ast_node_at(AST_FOR, NULL, parts, parse_statement(tokens, token_index),
        for_location);
}

struct ast_node* parse_for_init(struct token *tokens, int *token_index)
{
    if (tokens[*token_index].type == T_SEMICOLON) {
        (*token_index)++;
        return NULL;
    }

    if (is_type_start(tokens[*token_index].type)) {
        return parse_declaration(tokens, token_index);
    }

    struct ast_node *init = parse_exp(tokens, token_index);
    if (tokens[*token_index].type != T_SEMICOLON) {
        parse_error_at(&tokens[*token_index], "expected ';' after for initializer, found '%s'",
            tokens[*token_index].value);
    }
    (*token_index)++;

    return init;
}

struct ast_node* parse_optional_exp(struct token *tokens, int *token_index)
{
    if (tokens[*token_index].type == T_SEMICOLON || tokens[*token_index].type == T_CLOSEPAREN) {
        return NULL;
    }

    return parse_exp(tokens, token_index);
}

struct ast_node* parse_factor(struct token *tokens, int *token_index)
{
    struct token *tok = &tokens[*token_index];

    if (tok->type == T_PLUS_PLUS) {
        SourceLocation operator_location = tok->location;
        (*token_index)++;
        struct ast_node *operand = parse_factor(tokens, token_index);
        if (operand->type != AST_IDENTIFIER) {
            parse_error_at(tok, "operand of prefix ++ must be an identifier");
        }
        return create_ast_node_at(AST_PRE_INCREMENT, NULL, operand, NULL, operator_location);
    } else if (tok->type == T_MINUS_MINUS) {
        SourceLocation operator_location = tok->location;
        (*token_index)++;
        struct ast_node *operand = parse_factor(tokens, token_index);
        if (operand->type != AST_IDENTIFIER) {
            parse_error_at(tok, "operand of prefix -- must be an identifier");
        }
        return create_ast_node_at(AST_PRE_DECREMENT, NULL, operand, NULL, operator_location);
    } else if (tok->type == T_SIZEOF) {
        SourceLocation sizeof_location = tok->location;
        (*token_index)++;
        if (tokens[*token_index].type == T_OPENPAREN) {
            int type_index = *token_index + 1;
            const char *type_name = parse_type_name(tokens, &type_index);
            if (type_name && tokens[type_index].type == T_CLOSEPAREN) {
                *token_index = type_index + 1;
                struct ast_node *size = create_ast_node_at(AST_SIZEOF, (char *)type_name, NULL, NULL,
                    sizeof_location);
                size->data_type = TYPE_UINT;
                return size;
            }
        }

        parse_factor(tokens, token_index);
        struct ast_node *size = create_ast_node_at(AST_SIZEOF, "int", NULL, NULL, sizeof_location);
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
    }

    if (tok->type == T_INTLIT) {
        struct ast_node *lit_node = create_ast_node_at(AST_INTLIT, tok->value, NULL, NULL,
            tok->location);
        (*token_index)++;
        return lit_node;
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
            }
            (*token_index)++;
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
        const char *type_name = parse_type_name(tokens, &type_index);
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
        }
        (*token_index)++;
        return inner_exp;
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
        if (left->type != AST_IDENTIFIER) {
            parse_error_at(&tokens[*token_index], "left side of assignment must be an identifier");
        }

        SourceLocation operator_location = tokens[*token_index].location;
        (*token_index)++;
        struct ast_node *right = parse_assignment(tokens, token_index);

        if (op == T_ASSIGN) {
            return create_ast_node_at(AST_ASSIGN, NULL, left, right, operator_location);
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
                create_ast_node_at(AST_IDENTIFIER, left->value, NULL, NULL, left->location),
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
        }
        (*token_index)++;

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

struct ast_node* create_ast_node(ASTNodeType type, char *value, struct ast_node *left, struct ast_node *right)
{
    SourceLocation location = {0, 0};

    if (left) {
        location = left->location;
    } else if (right) {
        location = right->location;
    }
    return create_ast_node_at(type, value, left, right, location);
}

struct ast_node* create_ast_node_at(ASTNodeType type, char *value, struct ast_node *left, struct ast_node *right, SourceLocation location)
{
    struct ast_node *node = malloc(sizeof(struct ast_node));
    if (!node) {
        perror("Error allocating AST node");
        exit(EXIT_FAILURE);
    }

    node->type = type;
    node->data_type = TYPE_INVALID;
    node->location = location;
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
