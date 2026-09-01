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

/* The preprocessor is a singleton: one translation unit at a time. */
struct pp_state pp;

/* ---------------------------------------------------------------- lists -- */

static void list_reserve(struct token_list *list, int needed)
{
    if (list->count + needed <= list->capacity) {
        return;
    }
    while (list->capacity < list->count + needed) {
        list->capacity = list->capacity ? list->capacity * 2 : 64;
    }
    list->items = xrealloc(list->items,
        (size_t)list->capacity * sizeof(*list->items), "a token list");
}

/* Tokens own their text, so copies duplicate it. */
void pp_list_push(struct token_list *list, const struct token *token)
{
    list_reserve(list, 1);
    list->items[list->count] = *token;
    list->items[list->count].value = token->value ? strdup(token->value) : NULL;
    list->count++;
}

void pp_list_free(struct token_list *list)
{
    int i;

    for (i = 0; i < list->count; i++) {
        free(list->items[i].value);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

struct token pp_make_token(TokenType type, const char *value,
    SourceLocation location)
{
    struct token token;

    token.type = type;
    token.value = (char *)value;      /* copied by pp_list_push */
    token.location = location;
    token.at_line_start = 0;
    return token;
}

/* Keep a path alive for as long as any token can refer to it. */
const char *pp_intern_path(const char *path)
{
    int i;

    for (i = 0; i < pp.path_count; i++) {
        if (strcmp(pp.paths[i], path) == 0) {
            return pp.paths[i];
        }
    }

    if (pp.path_count >= pp.path_capacity) {
        pp.path_capacity = pp.path_capacity ? pp.path_capacity * 2 : 16;
        pp.paths = xrealloc(pp.paths,
            (size_t)pp.path_capacity * sizeof(*pp.paths), "preprocessor pp");
    }
    pp.paths[pp.path_count] = strdup(path);
    return pp.paths[pp.path_count++];
}

/* The tokens of one directive: from just after '#' to the end of the line. */
static int directive_end(const struct token *tokens, int count, int start)
{
    int i = start;

    while (i < count && tokens[i].type != T_EOF) {
        if (i > start && tokens[i].at_line_start) {
            break;
        }
        i++;
    }
    return i;
}

static void define_macro(const struct token *tokens, int start, int end)
{
    struct pp_macro *macro;
    const char *name;
    int i = start;

    if (i >= end || tokens[i].type != T_IDENTIFIER) {
        diag_at(DIAG_ERROR, tokens[start > 0 ? start - 1 : 0].location,
            "#define needs a macro name");
        return;
    }

    name = tokens[i].value;
    macro = pp_add_macro(name);
    i++;

    /*
     * A '(' immediately after the name -- no space -- makes it function-like.
     * The token stream has no spacing, so adjacency is judged by column.
     */
    if (i < end && tokens[i].type == T_OPENPAREN &&
        tokens[i].location.line == tokens[i - 1].location.line &&
        tokens[i].location.column ==
            tokens[i - 1].location.column + (int)strlen(name)) {
        macro->is_function_like = 1;
        i++;

        while (i < end && tokens[i].type != T_CLOSEPAREN) {
            if (tokens[i].type == T_IDENTIFIER) {
                macro->params = xrealloc(macro->params,
                    (size_t)(macro->param_count + 1) * sizeof(*macro->params), "preprocessor pp");
                macro->params[macro->param_count++] = strdup(tokens[i].value);
            } else if (tokens[i].type != T_COMMA) {
                diag_at(DIAG_ERROR, tokens[i].location,
                    "unexpected '%s' in macro parameter list",
                    tokens[i].value ? tokens[i].value : "?");
            }
            i++;
        }
        if (i < end) {
            i++;                    /* step over ')' */
        }
    }

    for (; i < end; i++) {
        pp_list_push(&macro->body, &tokens[i]);
    }
}

/*
 * Walk one file's tokens, acting on directives and expanding everything else.
 * Conditional groups are tracked with a stack so they can nest.
 */
void pp_process_tokens(const struct token *tokens, int count,
    const char *path, struct token_list *out)
{
    struct {
        int active;         /* emitting inside this group */
        int taken;          /* some branch already matched */
        int seen_else;
    } stack[64];
    int depth = 0;
    int active = 1;
    int i = 0;

    while (i < count) {
        const struct token *token = &tokens[i];
        int end;
        const char *name;

        if (token->type == T_EOF) {
            break;
        }

        if (!(token->type == T_HASH && token->at_line_start)) {
            if (active) {
                /* Expand from here to the end of the line, then continue after. */
                int line_end = i + 1;

                while (line_end < count && tokens[line_end].type != T_EOF &&
                       !tokens[line_end].at_line_start) {
                    line_end++;
                }
                pp_expand_range(&tokens[i], line_end - i, out);
                i = line_end;
            } else {
                i++;
            }
            continue;
        }

        end = directive_end(tokens, count, i + 1);
        if (i + 1 >= end) {
            i = end;                /* a bare '#' is allowed and does nothing */
            continue;
        }

        name = tokens[i + 1].value ? tokens[i + 1].value : "";

        if (strcmp(name, "ifdef") == 0 || strcmp(name, "ifndef") == 0 ||
            strcmp(name, "if") == 0) {
            int condition = 0;

            if (depth >= (int)(sizeof(stack) / sizeof(stack[0]))) {
                diag_at(DIAG_ERROR, token->location,
                    "conditional directives nested too deeply");
                i = end;
                continue;
            }

            if (active) {
                if (strcmp(name, "if") == 0) {
                    condition = pp_evaluate_condition(&tokens[i + 2], end - i - 2) != 0;
                } else {
                    int defined = i + 2 < end && tokens[i + 2].type == T_IDENTIFIER &&
                        pp_find_macro(tokens[i + 2].value) != NULL;

                    if (i + 2 >= end) {
                        diag_at(DIAG_ERROR, token->location,
                            "#%s needs a macro name", name);
                    }
                    condition = strcmp(name, "ifdef") == 0 ? defined : !defined;
                }
            }

            stack[depth].active = active;
            stack[depth].taken = condition;
            stack[depth].seen_else = 0;
            depth++;
            active = active && condition;
            i = end;
            continue;
        }

        if (strcmp(name, "elif") == 0 || strcmp(name, "else") == 0) {
            if (depth == 0) {
                diag_at(DIAG_ERROR, token->location,
                    "#%s without a matching #if", name);
                i = end;
                continue;
            }
            if (stack[depth - 1].seen_else) {
                diag_at(DIAG_ERROR, token->location, "#%s after #else", name);
                i = end;
                continue;
            }

            if (strcmp(name, "else") == 0) {
                stack[depth - 1].seen_else = 1;
                active = stack[depth - 1].active && !stack[depth - 1].taken;
                stack[depth - 1].taken = 1;
            } else {
                int condition = 0;

                if (stack[depth - 1].active && !stack[depth - 1].taken) {
                    condition = pp_evaluate_condition(&tokens[i + 2], end - i - 2) != 0;
                }
                active = stack[depth - 1].active && condition;
                if (condition) {
                    stack[depth - 1].taken = 1;
                }
            }
            i = end;
            continue;
        }

        if (strcmp(name, "endif") == 0) {
            if (depth == 0) {
                diag_at(DIAG_ERROR, token->location, "#endif without a matching #if");
            } else {
                depth--;
                active = stack[depth].active;
            }
            i = end;
            continue;
        }

        if (!active) {
            i = end;                /* everything else is skipped when inactive */
            continue;
        }

        if (strcmp(name, "define") == 0) {
            define_macro(tokens, i + 2, end);
        } else if (strcmp(name, "undef") == 0) {
            if (i + 2 < end && tokens[i + 2].type == T_IDENTIFIER) {
                pp_remove_macro(tokens[i + 2].value);
            } else {
                diag_at(DIAG_ERROR, token->location, "#undef needs a macro name");
            }
        } else if (strcmp(name, "include") == 0) {
            const struct token *argument = i + 2 < end ? &tokens[i + 2] : NULL;
            char *resolved = NULL;

            if (argument && argument->type == T_STRINGLIT) {
                resolved = pp_resolve_include(argument->value, 0, path);
                if (!resolved) {
                    diag_at(DIAG_ERROR, token->location,
                        "cannot find include file '%s'", argument->value);
                }
            } else if (argument && argument->type == T_LESS) {
                /*
                 * An angled include is not a single token: the lexer sees
                 * '<', the path in pieces, then '>'. Reassemble it.
                 */
                char name_buffer[512];
                int j = i + 3;

                name_buffer[0] = '\0';
                while (j < end && tokens[j].type != T_GREATER) {
                    if (tokens[j].value) {
                        strncat(name_buffer, tokens[j].value,
                            sizeof(name_buffer) - strlen(name_buffer) - 1);
                    }
                    j++;
                }
                if (j >= end) {
                    diag_at(DIAG_ERROR, token->location,
                        "missing '>' in #include");
                } else {
                    resolved = pp_resolve_include(name_buffer, 1, path);
                    if (!resolved) {
                        diag_at(DIAG_ERROR, token->location,
                            "cannot find include file <%s>", name_buffer);
                    }
                }
            } else {
                diag_at(DIAG_ERROR, token->location,
                    "#include needs \"file\" or <file>");
            }

            if (resolved) {
                pp_include_file(resolved, token->location, out);
                free(resolved);
            }
        } else if (strcmp(name, "pragma") == 0) {
            if (i + 2 < end && tokens[i + 2].value &&
                strcmp(tokens[i + 2].value, "once") == 0) {
                pp_mark_once(path);
            }
            /* Any other pragma is ignored, which is what the standard allows. */
        } else if (strcmp(name, "error") == 0) {
            char *message = pp_stringize(&tokens[i + 2], end - i - 2);

            diag_at(DIAG_ERROR, token->location, "#error %s", message);
            free(message);
        } else if (strcmp(name, "warning") == 0) {
            char *message = pp_stringize(&tokens[i + 2], end - i - 2);

            diag_at(DIAG_WARNING, token->location, "#warning %s", message);
            free(message);
        } else if (strcmp(name, "line") == 0) {
            /* Accepted and ignored: positions come from the token stream. */
        } else {
            diag_at(DIAG_ERROR, tokens[i + 1].location,
                "unknown preprocessing directive '#%s'", name);
        }

        i = end;
    }

    if (depth != 0) {
        SourceLocation location;

        location.line = 0;
        location.column = 0;
        diag_at(DIAG_ERROR, location, "unterminated #if in '%s'", path);
    }
}

/* ---------------------------------------------------------------- entry -- */

static void define_builtin(const char *name, const char *value, TokenType type)
{
    struct pp_macro *macro = pp_add_macro(name);
    SourceLocation location;
    struct token token;

    location.line = 0;
    location.column = 0;
    location.file = NULL;
    token = pp_make_token(type, value, location);
    pp_list_push(&macro->body, &token);
}

/*
 * Apply a -D argument. "NAME" defines the macro as 1, matching every other C
 * compiler; "NAME=value" lexes the value so it can be any token sequence.
 */
static void define_from_command_line(const char *text)
{
    const char *equals = strchr(text, '=');
    char name[256];
    struct pp_macro *macro;

    if (equals) {
        size_t length = (size_t)(equals - text);

        if (length >= sizeof(name)) {
            length = sizeof(name) - 1;
        }
        memcpy(name, text, length);
        name[length] = 0;
    } else {
        snprintf(name, sizeof(name), "%s", text);
    }

    macro = pp_add_macro(name);

    if (equals) {
        struct token *tokens = NULL;
        int count = 0;
        int i;

        lex_text(equals + 1, strlen(equals + 1), "<command line>", &tokens, &count);
        for (i = 0; i < count; i++) {
            if (tokens[i].type != T_EOF) {
                pp_list_push(&macro->body, &tokens[i]);
            }
        }
        free_tokens(tokens, count);
    } else {
        SourceLocation location;
        struct token one;

        location.line = 0;
        location.column = 0;
        location.file = NULL;
        one = pp_make_token(T_INTLIT, "1", location);
        pp_list_push(&macro->body, &one);
    }
}

void preprocess_file(const char *path, const struct pp_options *options,
    struct token **tokens, int *token_count)
{
    struct token_list out = { NULL, 0, 0 };
    struct token *file_tokens = NULL;
    int file_token_count = 0;
    char *text;
    size_t length = 0;
    SourceLocation eof_location;

    preprocess_free();

    if (options) {
        pp.include_paths = options->include_paths;
        pp.include_path_count = options->include_path_count;
    }

    define_builtin("__STDC__", "1", T_INTLIT);
    define_builtin("__DONKEY__", "1", T_INTLIT);

    if (options) {
        int i;

        for (i = 0; i < options->define_count; i++) {
            define_from_command_line(options->defines[i]);
        }
    }

    text = read_whole_file(path, &length);
    if (!text) {
        SourceLocation location;

        location.line = 0;
        location.column = 0;
        diag_at(DIAG_ERROR, location, "cannot read '%s'", path);
        *tokens = NULL;
        *token_count = 0;
        return;
    }

    lex_text(text, length, pp_intern_path(path), &file_tokens, &file_token_count);
    pp_process_tokens(file_tokens, file_token_count, path, &out);
    free_tokens(file_tokens, file_token_count);
    free(text);

    /* The parser expects a terminator. */
    eof_location.line = 0;
    eof_location.column = 0;
    if (out.count > 0) {
        eof_location = out.items[out.count - 1].location;
    }
    {
        struct token eof = pp_make_token(T_EOF, "EOF", eof_location);

        pp_list_push(&out, &eof);
    }

    *tokens = out.items;
    *token_count = out.count;
}

void preprocess_free(void)
{
    int i;

    for (i = 0; i < pp.macro_count; i++) {
        pp_free_macro(&pp.macros[i]);
    }
    free(pp.macros);
    for (i = 0; i < pp.once_count; i++) {
        free(pp.once_files[i]);
    }
    free(pp.once_files);
    for (i = 0; i < pp.path_count; i++) {
        free(pp.paths[i]);
    }
    free(pp.paths);

    memset(&pp, 0, sizeof(pp));
}
