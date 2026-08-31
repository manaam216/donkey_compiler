/*
 * Parser: statements. Blocks, conditionals, loops, switch, and jumps.
 */
#include "defs.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decl.h"
#include "diag.h"
#include "parser_internal.h"

struct ast_node* parse_do_while_statement(struct token *tokens, int *token_index)
{
    SourceLocation location = tokens[*token_index].location;
    struct ast_node *body;
    struct ast_node *condition;

    (*token_index)++;
    body = parse_statement(tokens, token_index);

    if (tokens[*token_index].type != T_WHILE) {
        parse_error_at(&tokens[*token_index], "expected 'while' after the body of a do loop");
        return create_ast_node_at(AST_DO_WHILE, NULL, NULL, body, location);
    }
    (*token_index)++;

    if (tokens[*token_index].type != T_OPENPAREN) {
        parse_error_at(&tokens[*token_index], "expected '(' after while");
    } else {
        (*token_index)++;
    }
    condition = parse_exp(tokens, token_index);
    if (tokens[*token_index].type != T_CLOSEPAREN) {
        parse_error_at(&tokens[*token_index], "expected ')' after the condition");
    } else {
        (*token_index)++;
    }
    if (tokens[*token_index].type != T_SEMICOLON) {
        parse_error_at(&tokens[*token_index], "expected ';' after do-while");
    } else {
        (*token_index)++;
    }

    return create_ast_node_at(AST_DO_WHILE, NULL, condition, body, location);
}

struct ast_node* parse_case_label(struct token *tokens, int *token_index)
{
    SourceLocation location = tokens[*token_index].location;
    int is_default = tokens[*token_index].type == T_DEFAULT;
    struct ast_node *node;
    char value[32];

    (*token_index)++;

    if (is_default) {
        node = create_ast_node_at(AST_DEFAULT, NULL, NULL, NULL, location);
    } else {
        long constant = 0;

        if (tokens[*token_index].type == T_INTLIT ||
            tokens[*token_index].type == T_CHARLIT) {
            constant = strtol(tokens[*token_index].value, NULL, 0);
            (*token_index)++;
        } else {
            parse_error_at(&tokens[*token_index],
                "a case label must be an integer constant");
        }
        snprintf(value, sizeof(value), "%ld", constant);
        node = create_ast_node_at(AST_CASE, value, NULL, NULL, location);
    }

    if (tokens[*token_index].type != T_COLON) {
        parse_error_at(&tokens[*token_index], "expected ':' after the case label");
    } else {
        (*token_index)++;
    }

    /*
     * A label at the very end of a switch body labels nothing; treat that as
     * an empty statement rather than running off into the closing brace.
     */
    if (tokens[*token_index].type == T_CLOSEBRACE) {
        node->left = create_ast_node_at(AST_EMPTY, NULL, NULL, NULL, location);
    } else {
        node->left = parse_statement(tokens, token_index);
    }
    return node;
}

struct ast_node* parse_switch_statement(struct token *tokens, int *token_index)
{
    SourceLocation location = tokens[*token_index].location;
    struct ast_node *control;
    struct ast_node *body;

    (*token_index)++;
    if (tokens[*token_index].type != T_OPENPAREN) {
        parse_error_at(&tokens[*token_index], "expected '(' after switch");
    } else {
        (*token_index)++;
    }
    control = parse_exp(tokens, token_index);
    if (tokens[*token_index].type != T_CLOSEPAREN) {
        parse_error_at(&tokens[*token_index], "expected ')' after the switch value");
    } else {
        (*token_index)++;
    }

    body = parse_statement(tokens, token_index);
    return create_ast_node_at(AST_SWITCH, NULL, control, body, location);
}

struct ast_node* parse_statement(struct token *tokens, int *token_index)
{
    struct token *tok = &tokens[*token_index];

    if (tok->type == T_OPENBRACE) {
        return parse_block(tokens, token_index);
    }

    if (is_type_start(tok->type) || is_typedef_name(tok)) {
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
        struct ast_node *exp = NULL;

        (*token_index)++;

        /* `return;` with no value, which a void function needs. */
        if (tokens[*token_index].type != T_SEMICOLON) {
            exp = parse_exp(tokens, token_index);
        }

        tok = &tokens[*token_index];
        if (tok->type != T_SEMICOLON) {
            parse_error_at(tok, "expected ';', found '%s'", tok->value);
        } else {
            (*token_index)++;
        }

        return create_ast_node_at(AST_RETURN, NULL, exp, NULL, return_location);
    }

    if (tok->type == T_DO) {
        return parse_do_while_statement(tokens, token_index);
    }

    if (tok->type == T_SWITCH) {
        return parse_switch_statement(tokens, token_index);
    }

    if (tok->type == T_CASE || tok->type == T_DEFAULT) {
        return parse_case_label(tokens, token_index);
    }

    if (tok->type == T_GOTO) {
        SourceLocation goto_location = tok->location;
        char *label;

        (*token_index)++;
        if (tokens[*token_index].type != T_IDENTIFIER) {
            parse_error_at(&tokens[*token_index], "expected a label name after goto");
            return create_ast_node_at(AST_EMPTY, NULL, NULL, NULL, goto_location);
        }
        label = tokens[*token_index].value;
        (*token_index)++;
        if (tokens[*token_index].type != T_SEMICOLON) {
            parse_error_at(&tokens[*token_index], "expected ';' after goto");
        } else {
            (*token_index)++;
        }
        return create_ast_node_at(AST_GOTO, label, NULL, NULL, goto_location);
    }

    /*
     * `name:` is a label. It takes two tokens of lookahead to tell apart from
     * an expression statement that merely starts with an identifier.
     */
    if (tok->type == T_IDENTIFIER && tokens[*token_index + 1].type == T_COLON) {
        SourceLocation label_location = tok->location;
        char *label = tok->value;

        *token_index += 2;
        return create_ast_node_at(AST_LABEL, label,
            parse_statement(tokens, token_index), NULL, label_location);
    }

    /* A lone semicolon is a statement that does nothing. */
    if (tok->type == T_SEMICOLON) {
        SourceLocation empty_location = tok->location;

        (*token_index)++;
        return create_ast_node_at(AST_EMPTY, NULL, NULL, NULL, empty_location);
    }

    struct ast_node *exp = parse_exp(tokens, token_index);

    tok = &tokens[*token_index];
    if (tok->type != T_SEMICOLON) {
        parse_error_at(tok, "expected ';', found '%s'", tok->value);
    }
    (*token_index)++;

    return create_ast_node_at(AST_EXPR_STMT, NULL, exp, NULL, exp->location);
}

struct ast_node* parse_if_statement(struct token *tokens, int *token_index)
{
    SourceLocation if_location = tokens[*token_index].location;
    (*token_index)++;

    if (tokens[*token_index].type != T_OPENPAREN) {
        parse_error_at(&tokens[*token_index], "expected '(' after if, found '%s'",
            tokens[*token_index].value);
    } else {
        (*token_index)++;
    }

    struct ast_node *cond = parse_exp(tokens, token_index);

    if (tokens[*token_index].type != T_CLOSEPAREN) {
        parse_error_at(&tokens[*token_index], "expected ')' after if condition, found '%s'",
            tokens[*token_index].value);
    } else {
        (*token_index)++;
    }

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
    } else {
        (*token_index)++;
    }

    struct ast_node *cond = parse_exp(tokens, token_index);

    if (tokens[*token_index].type != T_CLOSEPAREN) {
        parse_error_at(&tokens[*token_index], "expected ')' after while condition, found '%s'",
            tokens[*token_index].value);
    } else {
        (*token_index)++;
    }

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
    } else {
        (*token_index)++;
    }

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
    } else {
        (*token_index)++;
    }

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

    if (is_type_start(tokens[*token_index].type) ||
        is_typedef_name(&tokens[*token_index])) {
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
