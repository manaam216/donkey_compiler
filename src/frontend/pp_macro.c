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

struct pp_macro *pp_find_macro(const char *name)
{
    int i;

    if (!name) {
        return NULL;
    }
    for (i = 0; i < pp.macro_count; i++) {
        if (pp.macros[i].name && strcmp(pp.macros[i].name, name) == 0) {
            return &pp.macros[i];
        }
    }
    return NULL;
}

void pp_free_macro(struct pp_macro *macro)
{
    int i;

    free(macro->name);
    for (i = 0; i < macro->param_count; i++) {
        free(macro->params[i]);
    }
    free(macro->params);
    pp_list_free(&macro->body);
    memset(macro, 0, sizeof(*macro));
}

struct pp_macro *pp_add_macro(const char *name)
{
    struct pp_macro *existing = pp_find_macro(name);

    if (existing) {
        /* Redefinition simply replaces; C requires them to match, we allow it. */
        pp_free_macro(existing);
        existing->name = strdup(name);
        return existing;
    }

    if (pp.macro_count >= pp.macro_capacity) {
        pp.macro_capacity = pp.macro_capacity ? pp.macro_capacity * 2 : 64;
        pp.macros = xrealloc(pp.macros,
            (size_t)pp.macro_capacity * sizeof(*pp.macros), "preprocessor pp");
    }

    memset(&pp.macros[pp.macro_count], 0, sizeof(pp.macros[0]));
    pp.macros[pp.macro_count].name = strdup(name);
    return &pp.macros[pp.macro_count++];
}

void pp_remove_macro(const char *name)
{
    struct pp_macro *macro = pp_find_macro(name);

    if (macro) {
        pp_free_macro(macro);      /* leaves a hole with a NULL name; find skips it */
    }
}

/* ------------------------------------------------------------ expansion -- */

void pp_expand_range(const struct token *tokens, int count,
    struct token_list *out);

/* Render tokens back to source text, for the # operator. */
char *pp_stringize(const struct token *tokens, int count)
{
    size_t size = 1;
    char *text;
    int i;

    for (i = 0; i < count; i++) {
        size += (tokens[i].value ? strlen(tokens[i].value) : 0) + 1;
    }

    text = xmalloc(size, "preprocessor pp");
    text[0] = '\0';

    for (i = 0; i < count; i++) {
        if (i > 0) {
            strcat(text, " ");
        }
        if (tokens[i].value) {
            strcat(text, tokens[i].value);
        }
    }
    return text;
}

static int macro_param_index(const struct pp_macro *macro, const char *name)
{
    int i;

    if (!name) {
        return -1;
    }
    for (i = 0; i < macro->param_count; i++) {
        if (strcmp(macro->params[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

/*
 * Join two tokens with ##. The result has to be re-lexed: pasting `a` and `1`
 * makes the single identifier a1, not two tokens sitting next to each other.
 */
static void paste_tokens(struct token_list *out, const struct token *right)
{
    struct token *left;
    char *joined;
    struct token *relexed = NULL;
    int relexed_count = 0;
    size_t length;

    if (out->count == 0) {
        pp_list_push(out, right);
        return;
    }

    left = &out->items[out->count - 1];
    length = (left->value ? strlen(left->value) : 0) +
             (right->value ? strlen(right->value) : 0) + 1;
    joined = xmalloc(length, "preprocessor pp");
    snprintf(joined, length, "%s%s", left->value ? left->value : "",
        right->value ? right->value : "");

    lex_text(joined, strlen(joined), "<paste>", &relexed, &relexed_count);

    if (relexed_count >= 1) {
        SourceLocation location = left->location;
        int i;

        free(left->value);
        out->count--;                    /* drop the left operand */
        for (i = 0; i < relexed_count; i++) {
            if (relexed[i].type == T_EOF) {
                continue;
            }
            relexed[i].location = location;
            pp_list_push(out, &relexed[i]);
        }
    }

    free_tokens(relexed, relexed_count);
    free(joined);
}

/*
 * Collect the arguments of a function-like macro invocation. *index points at
 * the open parenthesis on entry and just past the matching one on return.
 */
static void collect_arguments(const struct token *tokens, int count, int *index,
    struct token_list **arguments, int *argument_count, int expected)
{
    struct token_list *args;
    int capacity = expected > 0 ? expected : 1;
    int depth = 0;
    int n;

    args = xcalloc((size_t)capacity, sizeof(*args), "preprocessor pp");

    (*index)++;                          /* step over the open parenthesis */
    n = 1;

    while (*index < count) {
        const struct token *token = &tokens[*index];

        if (token->type == T_OPENPAREN) {
            depth++;
        } else if (token->type == T_CLOSEPAREN) {
            if (depth == 0) {
                (*index)++;
                break;
            }
            depth--;
        } else if (token->type == T_COMMA && depth == 0) {
            /* A comma at the top level separates arguments. */
            if (n >= capacity) {
                int old = capacity;

                capacity *= 2;
                args = xrealloc(args, (size_t)capacity * sizeof(*args), "preprocessor pp");
                memset(&args[old], 0, (size_t)(capacity - old) * sizeof(*args));
            }
            n++;
            (*index)++;
            continue;
        }

        pp_list_push(&args[n - 1], token);
        (*index)++;
    }

    /* An empty argument list is how a macro with no parameters is called. */
    if (expected == 0 && n == 1 && args[0].count == 0) {
        n = 0;
    }

    *arguments = args;
    *argument_count = n;
}

static void free_arguments(struct token_list *arguments, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        pp_list_free(&arguments[i]);
    }
    free(arguments);
}

/* Substitute arguments into a macro body, applying # and ## as they appear. */
static void substitute(struct pp_macro *macro, struct token_list *arguments,
    int argument_count, SourceLocation location, struct token_list *out)
{
    int i;

    for (i = 0; i < macro->body.count; i++) {
        struct token *token = &macro->body.items[i];
        int param;

        /* # parameter: the argument's source text, as a string literal. */
        if (token->type == T_HASH && macro->is_function_like &&
            i + 1 < macro->body.count) {
            param = macro_param_index(macro, macro->body.items[i + 1].value);
            if (param >= 0) {
                char *text = param < argument_count
                    ? pp_stringize(arguments[param].items, arguments[param].count)
                    : strdup("");
                struct token literal = pp_make_token(T_STRINGLIT, text, location);

                pp_list_push(out, &literal);
                free(text);
                i++;
                continue;
            }
        }

        /* ## joins what is on either side of it. */
        if (token->type == T_HASH_HASH && i + 1 < macro->body.count) {
            struct token *next = &macro->body.items[i + 1];

            param = macro_param_index(macro, next->value);
            if (param >= 0 && param < argument_count) {
                int j;

                for (j = 0; j < arguments[param].count; j++) {
                    if (j == 0) {
                        paste_tokens(out, &arguments[param].items[j]);
                    } else {
                        pp_list_push(out, &arguments[param].items[j]);
                    }
                }
            } else {
                paste_tokens(out, next);
            }
            i++;
            continue;
        }

        param = macro_param_index(macro, token->value);
        if (param >= 0 && token->type == T_IDENTIFIER) {
            /*
             * Arguments are macro-expanded before being substituted, except
             * next to # or ##. Those cases are handled above, so a plain
             * expansion is right by the time control reaches here.
             */
            if (param < argument_count) {
                struct token_list expanded = { NULL, 0, 0 };
                int j;

                pp_expand_range(arguments[param].items, arguments[param].count,
                    &expanded);
                for (j = 0; j < expanded.count; j++) {
                    expanded.items[j].location = location;
                    pp_list_push(out, &expanded.items[j]);
                }
                pp_list_free(&expanded);
            }
            continue;
        }

        {
            struct token copy = *token;

            copy.location = location;
            pp_list_push(out, &copy);
        }
    }
}

/*
 * Expand a run of tokens, replacing macro invocations, and rescanning the
 * result so a macro that produces another macro's name expands too.
 */
void pp_expand_range(const struct token *tokens, int count,
    struct token_list *out)
{
    int i = 0;

    while (i < count) {
        const struct token *token = &tokens[i];
        struct pp_macro *macro;

        if (token->type != T_IDENTIFIER) {
            pp_list_push(out, token);
            i++;
            continue;
        }

        macro = pp_find_macro(token->value);
        if (!macro || macro->expanding) {
            pp_list_push(out, token);
            i++;
            continue;
        }

        if (macro->is_function_like) {
            struct token_list *arguments = NULL;
            int argument_count = 0;
            struct token_list replaced = { NULL, 0, 0 };
            struct token_list rescanned = { NULL, 0, 0 };
            int j = i + 1;
            int k;

            /* Without a following parenthesis the name is not an invocation. */
            if (j >= count || tokens[j].type != T_OPENPAREN) {
                pp_list_push(out, token);
                i++;
                continue;
            }

            collect_arguments(tokens, count, &j, &arguments, &argument_count,
                macro->param_count);

            if (argument_count != macro->param_count) {
                diag_at(DIAG_ERROR, token->location,
                    "macro '%s' expects %d argument(s), but %d given",
                    macro->name, macro->param_count, argument_count);
            }

            macro->expanding = 1;
            substitute(macro, arguments, argument_count, token->location, &replaced);
            pp_expand_range(replaced.items, replaced.count, &rescanned);
            macro->expanding = 0;

            for (k = 0; k < rescanned.count; k++) {
                pp_list_push(out, &rescanned.items[k]);
            }

            pp_list_free(&replaced);
            pp_list_free(&rescanned);
            free_arguments(arguments, argument_count);
            i = j;
            continue;
        }

        {
            struct token_list replaced = { NULL, 0, 0 };
            struct token_list rescanned = { NULL, 0, 0 };
            int k;

            macro->expanding = 1;
            substitute(macro, NULL, 0, token->location, &replaced);
            pp_expand_range(replaced.items, replaced.count, &rescanned);
            macro->expanding = 0;

            for (k = 0; k < rescanned.count; k++) {
                pp_list_push(out, &rescanned.items[k]);
            }
            pp_list_free(&replaced);
            pp_list_free(&rescanned);
            i++;
        }
    }
}
