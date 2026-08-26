#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "decl.h"
#include "diag.h"
#include "preprocess.h"

#define MAX_INCLUDE_DEPTH 64

/*
 * A growable token list. The preprocessor builds several of these -- macro
 * bodies, argument lists, the output stream -- so they share one type.
 */
struct token_list {
    struct token *items;
    int count;
    int capacity;
};

struct pp_macro {
    char *name;
    int is_function_like;

    char **params;
    int param_count;

    struct token_list body;

    /*
     * Set while this macro is being expanded, so a macro that mentions itself
     * stops rather than recursing forever. This is the "blue paint" rule: a
     * name is not replaced again while its own expansion is in progress.
     */
    int expanding;
};

struct pp_state {
    struct pp_macro *macros;
    int macro_count;
    int macro_capacity;

    const char *const *include_paths;
    int include_path_count;

    /* Files that asked not to be included twice, via #pragma once. */
    char **once_files;
    int once_count;
    int once_capacity;

    /*
     * Every path a token can name. Token locations point into these, so they
     * must outlive the token stream -- freeing a path after including it left
     * every diagnostic from that file naming freed memory.
     */
    char **paths;
    int path_count;
    int path_capacity;

    int depth;
};

static struct pp_state state;

/* ---------------------------------------------------------------- lists -- */

static void list_reserve(struct token_list *list, int needed)
{
    if (list->count + needed <= list->capacity) {
        return;
    }
    while (list->capacity < list->count + needed) {
        list->capacity = list->capacity ? list->capacity * 2 : 64;
    }
    list->items = realloc(list->items, (size_t)list->capacity * sizeof(*list->items));
    if (!list->items) {
        fprintf(stderr, "Out of memory in the preprocessor\n");
        exit(EXIT_FAILURE);
    }
}

/* Tokens own their text, so copies duplicate it. */
static void list_push(struct token_list *list, const struct token *token)
{
    list_reserve(list, 1);
    list->items[list->count] = *token;
    list->items[list->count].value = token->value ? strdup(token->value) : NULL;
    list->count++;
}

static void list_free(struct token_list *list)
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

static struct token make_token(TokenType type, const char *value,
    SourceLocation location)
{
    struct token token;

    token.type = type;
    token.value = (char *)value;      /* copied by list_push */
    token.location = location;
    token.at_line_start = 0;
    return token;
}

/* --------------------------------------------------------------- macros -- */

/* Keep a path alive for as long as any token can refer to it. */
static const char *intern_path(const char *path)
{
    int i;

    for (i = 0; i < state.path_count; i++) {
        if (strcmp(state.paths[i], path) == 0) {
            return state.paths[i];
        }
    }

    if (state.path_count >= state.path_capacity) {
        state.path_capacity = state.path_capacity ? state.path_capacity * 2 : 16;
        state.paths = realloc(state.paths,
            (size_t)state.path_capacity * sizeof(*state.paths));
        if (!state.paths) {
            fprintf(stderr, "Out of memory in the preprocessor\n");
            exit(EXIT_FAILURE);
        }
    }
    state.paths[state.path_count] = strdup(path);
    return state.paths[state.path_count++];
}

static struct pp_macro *find_macro(const char *name)
{
    int i;

    if (!name) {
        return NULL;
    }
    for (i = 0; i < state.macro_count; i++) {
        if (state.macros[i].name && strcmp(state.macros[i].name, name) == 0) {
            return &state.macros[i];
        }
    }
    return NULL;
}

static void free_macro(struct pp_macro *macro)
{
    int i;

    free(macro->name);
    for (i = 0; i < macro->param_count; i++) {
        free(macro->params[i]);
    }
    free(macro->params);
    list_free(&macro->body);
    memset(macro, 0, sizeof(*macro));
}

static struct pp_macro *add_macro(const char *name)
{
    struct pp_macro *existing = find_macro(name);

    if (existing) {
        /* Redefinition simply replaces; C requires them to match, we allow it. */
        free_macro(existing);
        existing->name = strdup(name);
        return existing;
    }

    if (state.macro_count >= state.macro_capacity) {
        state.macro_capacity = state.macro_capacity ? state.macro_capacity * 2 : 64;
        state.macros = realloc(state.macros,
            (size_t)state.macro_capacity * sizeof(*state.macros));
        if (!state.macros) {
            fprintf(stderr, "Out of memory in the preprocessor\n");
            exit(EXIT_FAILURE);
        }
    }

    memset(&state.macros[state.macro_count], 0, sizeof(state.macros[0]));
    state.macros[state.macro_count].name = strdup(name);
    return &state.macros[state.macro_count++];
}

static void remove_macro(const char *name)
{
    struct pp_macro *macro = find_macro(name);

    if (macro) {
        free_macro(macro);      /* leaves a hole with a NULL name; find skips it */
    }
}

/* ------------------------------------------------------------ expansion -- */

static void expand_range(const struct token *tokens, int count,
    struct token_list *out);

/* Render tokens back to source text, for the # operator. */
static char *stringize(const struct token *tokens, int count)
{
    size_t size = 1;
    char *text;
    int i;

    for (i = 0; i < count; i++) {
        size += (tokens[i].value ? strlen(tokens[i].value) : 0) + 1;
    }

    text = malloc(size);
    if (!text) {
        fprintf(stderr, "Out of memory in the preprocessor\n");
        exit(EXIT_FAILURE);
    }
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
        list_push(out, right);
        return;
    }

    left = &out->items[out->count - 1];
    length = (left->value ? strlen(left->value) : 0) +
             (right->value ? strlen(right->value) : 0) + 1;
    joined = malloc(length);
    if (!joined) {
        fprintf(stderr, "Out of memory in the preprocessor\n");
        exit(EXIT_FAILURE);
    }
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
            list_push(out, &relexed[i]);
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

    args = calloc((size_t)capacity, sizeof(*args));
    if (!args) {
        fprintf(stderr, "Out of memory in the preprocessor\n");
        exit(EXIT_FAILURE);
    }

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
                args = realloc(args, (size_t)capacity * sizeof(*args));
                if (!args) {
                    fprintf(stderr, "Out of memory in the preprocessor\n");
                    exit(EXIT_FAILURE);
                }
                memset(&args[old], 0, (size_t)(capacity - old) * sizeof(*args));
            }
            n++;
            (*index)++;
            continue;
        }

        list_push(&args[n - 1], token);
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
        list_free(&arguments[i]);
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
                    ? stringize(arguments[param].items, arguments[param].count)
                    : strdup("");
                struct token literal = make_token(T_STRINGLIT, text, location);

                list_push(out, &literal);
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
                        list_push(out, &arguments[param].items[j]);
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

                expand_range(arguments[param].items, arguments[param].count,
                    &expanded);
                for (j = 0; j < expanded.count; j++) {
                    expanded.items[j].location = location;
                    list_push(out, &expanded.items[j]);
                }
                list_free(&expanded);
            }
            continue;
        }

        {
            struct token copy = *token;

            copy.location = location;
            list_push(out, &copy);
        }
    }
}

/*
 * Expand a run of tokens, replacing macro invocations, and rescanning the
 * result so a macro that produces another macro's name expands too.
 */
static void expand_range(const struct token *tokens, int count,
    struct token_list *out)
{
    int i = 0;

    while (i < count) {
        const struct token *token = &tokens[i];
        struct pp_macro *macro;

        if (token->type != T_IDENTIFIER) {
            list_push(out, token);
            i++;
            continue;
        }

        macro = find_macro(token->value);
        if (!macro || macro->expanding) {
            list_push(out, token);
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
                list_push(out, token);
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
            expand_range(replaced.items, replaced.count, &rescanned);
            macro->expanding = 0;

            for (k = 0; k < rescanned.count; k++) {
                list_push(out, &rescanned.items[k]);
            }

            list_free(&replaced);
            list_free(&rescanned);
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
            expand_range(replaced.items, replaced.count, &rescanned);
            macro->expanding = 0;

            for (k = 0; k < rescanned.count; k++) {
                list_push(out, &rescanned.items[k]);
            }
            list_free(&replaced);
            list_free(&rescanned);
            i++;
        }
    }
}

/* ----------------------------------------------------------- conditions -- */

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
                struct token value = make_token(T_INTLIT,
                    (name && find_macro(name)) ? "1" : "0", tokens[i].location);

                list_push(out, &value);
            }
            i = j;
            continue;
        }

        list_push(out, &tokens[i]);
        i++;
    }
}

static long evaluate_condition(const struct token *tokens, int count)
{
    struct token_list resolved = { NULL, 0, 0 };
    struct token_list expanded = { NULL, 0, 0 };
    struct cond_parser parser;
    long value;

    resolve_defined(tokens, count, &resolved);
    expand_range(resolved.items, resolved.count, &expanded);

    parser.tokens = expanded.items;
    parser.count = expanded.count;
    parser.index = 0;
    value = cond_expression(&parser, 0);

    list_free(&resolved);
    list_free(&expanded);
    return value;
}

/* ----------------------------------------------------------- directives -- */

static void process_tokens(const struct token *tokens, int count,
    const char *path, struct token_list *out);

static char *read_file(const char *path, size_t *length)
{
    FILE *file = fopen(path, "rb");
    char *text;
    long size;
    size_t read;

    if (!file) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);

    text = malloc((size_t)size + 1);
    if (!text) {
        fclose(file);
        fprintf(stderr, "Out of memory in the preprocessor\n");
        exit(EXIT_FAILURE);
    }
    read = fread(text, 1, (size_t)size, file);
    text[read] = '\0';
    *length = read;
    fclose(file);
    return text;
}

/* The directory part of a path, so a quoted include can be looked up beside it. */
static void directory_of(const char *path, char *buffer, size_t size)
{
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *last = slash > backslash ? slash : backslash;

    if (!last) {
        snprintf(buffer, size, ".");
        return;
    }
    if ((size_t)(last - path) >= size) {
        snprintf(buffer, size, ".");
        return;
    }
    memcpy(buffer, path, (size_t)(last - path));
    buffer[last - path] = '\0';
}

static int marked_once(const char *path)
{
    int i;

    for (i = 0; i < state.once_count; i++) {
        if (strcmp(state.once_files[i], path) == 0) {
            return 1;
        }
    }
    return 0;
}

static void mark_once(const char *path)
{
    if (state.once_count >= state.once_capacity) {
        state.once_capacity = state.once_capacity ? state.once_capacity * 2 : 16;
        state.once_files = realloc(state.once_files,
            (size_t)state.once_capacity * sizeof(*state.once_files));
        if (!state.once_files) {
            fprintf(stderr, "Out of memory in the preprocessor\n");
            exit(EXIT_FAILURE);
        }
    }
    state.once_files[state.once_count++] = strdup(path);
}

/*
 * Find an included file. A quoted include looks beside the including file
 * first, then falls back to the search path; an angled one uses the search
 * path only.
 */
static char *resolve_include(const char *name, int angled, const char *from_path)
{
    /* Room for a directory, a separator, a name, and the terminator. */
    char candidate[2048];
    int i;

    if (!angled) {
        char directory[1024];

        directory_of(from_path, directory, sizeof(directory));
        /*
         * Check the join fits rather than letting snprintf truncate: a
         * silently shortened path would be looked up, fail, and report a
         * missing file rather than a name that was too long.
         */
        if (strlen(directory) + 1 + strlen(name) < sizeof(candidate)) {
            size_t unused = 0;
            char *text;

            snprintf(candidate, sizeof(candidate), "%s/%s", directory, name);
            text = read_file(candidate, &unused);
            if (text) {
                free(text);
                return strdup(candidate);
            }
        }
    }

    for (i = 0; i < state.include_path_count; i++) {
        size_t unused = 0;
        char *text;

        if (strlen(state.include_paths[i]) + 1 + strlen(name) >= sizeof(candidate)) {
            continue;
        }
        snprintf(candidate, sizeof(candidate), "%s/%s",
            state.include_paths[i], name);
        text = read_file(candidate, &unused);
        if (text) {
            free(text);
            return strdup(candidate);
        }
    }

    return NULL;
}

static void include_file(const char *path, SourceLocation location,
    struct token_list *out)
{
    char *text;
    size_t length = 0;
    struct token *tokens = NULL;
    int count = 0;

    if (state.depth >= MAX_INCLUDE_DEPTH) {
        diag_at(DIAG_ERROR, location,
            "#include nested too deeply (limit is %d)", MAX_INCLUDE_DEPTH);
        return;
    }
    if (marked_once(path)) {
        return;
    }

    text = read_file(path, &length);
    if (!text) {
        diag_at(DIAG_ERROR, location, "cannot read '%s'", path);
        return;
    }

    lex_text(text, length, intern_path(path), &tokens, &count);

    state.depth++;
    process_tokens(tokens, count, path, out);
    state.depth--;

    free_tokens(tokens, count);
    free(text);
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
    macro = add_macro(name);
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
                macro->params = realloc(macro->params,
                    (size_t)(macro->param_count + 1) * sizeof(*macro->params));
                if (!macro->params) {
                    fprintf(stderr, "Out of memory in the preprocessor\n");
                    exit(EXIT_FAILURE);
                }
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
        list_push(&macro->body, &tokens[i]);
    }
}

/*
 * Walk one file's tokens, acting on directives and expanding everything else.
 * Conditional groups are tracked with a stack so they can nest.
 */
static void process_tokens(const struct token *tokens, int count,
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
                expand_range(&tokens[i], line_end - i, out);
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
                    condition = evaluate_condition(&tokens[i + 2], end - i - 2) != 0;
                } else {
                    int defined = i + 2 < end && tokens[i + 2].type == T_IDENTIFIER &&
                        find_macro(tokens[i + 2].value) != NULL;

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
                    condition = evaluate_condition(&tokens[i + 2], end - i - 2) != 0;
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
                remove_macro(tokens[i + 2].value);
            } else {
                diag_at(DIAG_ERROR, token->location, "#undef needs a macro name");
            }
        } else if (strcmp(name, "include") == 0) {
            const struct token *argument = i + 2 < end ? &tokens[i + 2] : NULL;
            char *resolved = NULL;

            if (argument && argument->type == T_STRINGLIT) {
                resolved = resolve_include(argument->value, 0, path);
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
                    resolved = resolve_include(name_buffer, 1, path);
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
                include_file(resolved, token->location, out);
                free(resolved);
            }
        } else if (strcmp(name, "pragma") == 0) {
            if (i + 2 < end && tokens[i + 2].value &&
                strcmp(tokens[i + 2].value, "once") == 0) {
                mark_once(path);
            }
            /* Any other pragma is ignored, which is what the standard allows. */
        } else if (strcmp(name, "error") == 0) {
            char *message = stringize(&tokens[i + 2], end - i - 2);

            diag_at(DIAG_ERROR, token->location, "#error %s", message);
            free(message);
        } else if (strcmp(name, "warning") == 0) {
            char *message = stringize(&tokens[i + 2], end - i - 2);

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
    struct pp_macro *macro = add_macro(name);
    SourceLocation location;
    struct token token;

    location.line = 0;
    location.column = 0;
    location.file = NULL;
    token = make_token(type, value, location);
    list_push(&macro->body, &token);
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

    macro = add_macro(name);

    if (equals) {
        struct token *tokens = NULL;
        int count = 0;
        int i;

        lex_text(equals + 1, strlen(equals + 1), "<command line>", &tokens, &count);
        for (i = 0; i < count; i++) {
            if (tokens[i].type != T_EOF) {
                list_push(&macro->body, &tokens[i]);
            }
        }
        free_tokens(tokens, count);
    } else {
        SourceLocation location;
        struct token one;

        location.line = 0;
        location.column = 0;
        location.file = NULL;
        one = make_token(T_INTLIT, "1", location);
        list_push(&macro->body, &one);
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
        state.include_paths = options->include_paths;
        state.include_path_count = options->include_path_count;
    }

    define_builtin("__STDC__", "1", T_INTLIT);
    define_builtin("__DONKEY__", "1", T_INTLIT);

    if (options) {
        int i;

        for (i = 0; i < options->define_count; i++) {
            define_from_command_line(options->defines[i]);
        }
    }

    text = read_file(path, &length);
    if (!text) {
        SourceLocation location;

        location.line = 0;
        location.column = 0;
        diag_at(DIAG_ERROR, location, "cannot read '%s'", path);
        *tokens = NULL;
        *token_count = 0;
        return;
    }

    lex_text(text, length, intern_path(path), &file_tokens, &file_token_count);
    process_tokens(file_tokens, file_token_count, path, &out);
    free_tokens(file_tokens, file_token_count);
    free(text);

    /* The parser expects a terminator. */
    eof_location.line = 0;
    eof_location.column = 0;
    if (out.count > 0) {
        eof_location = out.items[out.count - 1].location;
    }
    {
        struct token eof = make_token(T_EOF, "EOF", eof_location);

        list_push(&out, &eof);
    }

    *tokens = out.items;
    *token_count = out.count;
}

void preprocess_free(void)
{
    int i;

    for (i = 0; i < state.macro_count; i++) {
        free_macro(&state.macros[i]);
    }
    free(state.macros);
    for (i = 0; i < state.once_count; i++) {
        free(state.once_files[i]);
    }
    free(state.once_files);
    for (i = 0; i < state.path_count; i++) {
        free(state.paths[i]);
    }
    free(state.paths);

    memset(&state, 0, sizeof(state));
}
