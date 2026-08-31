#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "decl.h"
#include "diag.h"
#include "preprocess.h"
#include "support/mem.h"
#include "support/file.h"
#include "pp_internal.h"

/*
 * A #if expression is evaluated over already-expanded tokens, so by the time
 * this runs every macro has been replaced. Identifiers that survive are zero,
 * as C requires. Precedence climbing keeps the whole grammar in one function
 * pair rather than one function per level.
 */
struct cond_parser {
    const struct token *tokens;
    int count;
    int index;
};

static long cond_expression(struct cond_parser *parser, int min_precedence);

static const struct token *cond_peek(struct cond_parser *parser)
{
    return parser->index < parser->count ? &parser->tokens[parser->index] : NULL;
}

static long cond_primary(struct cond_parser *parser)
{
    const struct token *token = cond_peek(parser);

    if (!token) {
        return 0;
    }

    switch (token->type) {
        case T_INTLIT:
        case T_CHARLIT:
            parser->index++;
            return strtol(token->value, NULL, 0);
        case T_IDENTIFIER:
            /* Any name still standing here was never defined; C says it is 0. */
            parser->index++;
            return 0;
        case T_OPENPAREN: {
            long value;

            parser->index++;
            value = cond_expression(parser, 0);
            if (cond_peek(parser) && cond_peek(parser)->type == T_CLOSEPAREN) {
                parser->index++;
            } else {
                diag_at(DIAG_ERROR, token->location,
                    "missing ')' in preprocessor expression");
            }
            return value;
        }
        case T_MINUS:
            parser->index++;
            return -cond_primary(parser);
        case T_PLUS:
            parser->index++;
            return cond_primary(parser);
        case T_LOGICAL_NEGATION:
            parser->index++;
            return !cond_primary(parser);
        case T_BITWISE_COMPLEMENT:
            parser->index++;
            return ~cond_primary(parser);
        default:
            diag_at(DIAG_ERROR, token->location,
                "unexpected '%s' in preprocessor expression",
                token->value ? token->value : "?");
            parser->index++;
            return 0;
    }
}

static int binary_precedence(TokenType type)
{
    switch (type) {
        case T_LOGICAL_OR:    return 1;
        case T_LOGICAL_AND:   return 2;
        case T_PIPE:          return 3;
        case T_CARET:         return 4;
        case T_AMPERSAND:     return 5;
        case T_EQUAL:
        case T_NOT_EQUAL:     return 6;
        case T_LESS:
        case T_LESS_EQUAL:
        case T_GREATER:
        case T_GREATER_EQUAL: return 7;
        case T_SHIFT_LEFT:
        case T_SHIFT_RIGHT:   return 8;
        case T_PLUS:
        case T_MINUS:         return 9;
        case T_STAR:
        case T_SLASH:
        case T_PERCENT:       return 10;
        default:              return 0;
    }
}

static long apply_binary(TokenType type, long left, long right,
    SourceLocation location)
{
    switch (type) {
        case T_LOGICAL_OR:    return left || right;
        case T_LOGICAL_AND:   return left && right;
        case T_PIPE:          return left | right;
        case T_CARET:         return left ^ right;
        case T_AMPERSAND:     return left & right;
        case T_EQUAL:         return left == right;
        case T_NOT_EQUAL:     return left != right;
        case T_LESS:          return left < right;
        case T_LESS_EQUAL:    return left <= right;
        case T_GREATER:       return left > right;
        case T_GREATER_EQUAL: return left >= right;
        case T_SHIFT_LEFT:    return left << right;
        case T_SHIFT_RIGHT:   return left >> right;
        case T_PLUS:          return left + right;
        case T_MINUS:         return left - right;
        case T_STAR:          return left * right;
        case T_SLASH:
        case T_PERCENT:
            if (right == 0) {
                diag_at(DIAG_ERROR, location,
                    "division by zero in preprocessor expression");
                return 0;
            }
            return type == T_SLASH ? left / right : left % right;
        default:              return 0;
    }
}

static long cond_expression(struct cond_parser *parser, int min_precedence)
{
    long left = cond_primary(parser);

    for (;;) {
        const struct token *token = cond_peek(parser);
        int precedence;

        if (!token) {
            break;
        }

        /* The conditional operator is right-associative and lowest of all. */
        if (token->type == T_QUESTION && min_precedence == 0) {
            long then_value;
            long else_value;

            parser->index++;
            then_value = cond_expression(parser, 0);
            if (cond_peek(parser) && cond_peek(parser)->type == T_COLON) {
                parser->index++;
            } else {
                diag_at(DIAG_ERROR, token->location,
                    "missing ':' in preprocessor expression");
            }
            else_value = cond_expression(parser, 0);
            left = left ? then_value : else_value;
            continue;
        }

        precedence = binary_precedence(token->type);
        if (precedence == 0 || precedence < min_precedence) {
            break;
        }
        parser->index++;
        left = apply_binary(token->type, left,
            cond_expression(parser, precedence + 1), token->location);
    }

    return left;
}

/*
 * Replace `defined X` and `defined(X)` with 1 or 0 before expansion, because
 * the operand must not itself be expanded.
 */
static void resolve_defined(const struct token *tokens, int count,
    struct token_list *out)
{
    int i = 0;

    while (i < count) {
        if (tokens[i].type == T_IDENTIFIER && tokens[i].value &&
            strcmp(tokens[i].value, "defined") == 0) {
            int j = i + 1;
            int parenthesised = 0;
            const char *name = NULL;

            if (j < count && tokens[j].type == T_OPENPAREN) {
                parenthesised = 1;
                j++;
            }
            if (j < count && tokens[j].type == T_IDENTIFIER) {
                name = tokens[j].value;
                j++;
            } else {
                diag_at(DIAG_ERROR, tokens[i].location,
                    "'defined' needs a macro name");
            }
            if (parenthesised) {
                if (j < count && tokens[j].type == T_CLOSEPAREN) {
                    j++;
                } else {
                    diag_at(DIAG_ERROR, tokens[i].location,
                        "missing ')' after 'defined'");
                }
            }

            {
                struct token value = pp_make_token(T_INTLIT,
                    (name && pp_find_macro(name)) ? "1" : "0", tokens[i].location);

                pp_list_push(out, &value);
            }
            i = j;
            continue;
        }

        pp_list_push(out, &tokens[i]);
        i++;
    }
}

long pp_evaluate_condition(const struct token *tokens, int count)
{
    struct token_list resolved = { NULL, 0, 0 };
    struct token_list expanded = { NULL, 0, 0 };
    struct cond_parser parser;
    long value;

    resolve_defined(tokens, count, &resolved);
    pp_expand_range(resolved.items, resolved.count, &expanded);

    parser.tokens = expanded.items;
    parser.count = expanded.count;
    parser.index = 0;
    value = cond_expression(&parser, 0);

    pp_list_free(&resolved);
    pp_list_free(&expanded);
    return value;
}
