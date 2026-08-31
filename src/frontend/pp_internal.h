#ifndef DONKEY_PP_INTERNAL_H
#define DONKEY_PP_INTERNAL_H

#include "defs.h"

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

/*
 * Shared between the preprocessor's four files.
 *
 * preprocess.c drives translation phase 4: it walks the token stream one
 * directive at a time and owns the state every other file reads. pp_macro.c
 * holds the macro table and the expansion algorithm, pp_cond.c evaluates #if,
 * and pp_include.c finds and pulls in headers.
 *
 * These declarations are the seam between them, and are internal to the
 * preprocessor: nothing outside src/frontend includes this. The pp_ prefix
 * keeps names like make_token and process_tokens from colliding with the lexer
 * and the parser, which are linked into the same program.
 */

/* The preprocessor is a singleton: one translation unit at a time. */
extern struct pp_state pp;

/* preprocess.c -- token lists, path interning, and the directive loop. */
void pp_list_push(struct token_list *list, const struct token *token);
void pp_list_free(struct token_list *list);
struct token pp_make_token(TokenType type, const char *value,
    SourceLocation location);
const char *pp_intern_path(const char *path);
void pp_process_tokens(const struct token *tokens, int count,
    const char *path, struct token_list *out);

/* pp_macro.c -- the macro table and macro expansion. */
struct pp_macro *pp_find_macro(const char *name);
void pp_free_macro(struct pp_macro *macro);
struct pp_macro *pp_add_macro(const char *name);
void pp_remove_macro(const char *name);
void pp_expand_range(const struct token *tokens, int count,
    struct token_list *out);

/* Render tokens back to source text, for the # operator. */
char *pp_stringize(const struct token *tokens, int count);
char *pp_stringize(const struct token *tokens, int count);
void pp_expand_range(const struct token *tokens, int count,
    struct token_list *out);

/* Render tokens back to source text, for the # operator. */
char *pp_stringize(const struct token *tokens, int count);

/* pp_cond.c -- #if expression evaluation. */
long pp_evaluate_condition(const struct token *tokens, int count);

/* pp_include.c -- header lookup, #pragma once, and nested includes. */
void pp_mark_once(const char *path);
char *pp_resolve_include(const char *name, int angled, const char *from_path);
void pp_include_file(const char *path, SourceLocation location,
    struct token_list *out);

#endif
