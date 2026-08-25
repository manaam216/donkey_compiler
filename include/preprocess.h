#ifndef DONKEY_PREPROCESS_H
#define DONKEY_PREPROCESS_H

#include "defs.h"

/*
 * The preprocessor: translation phases 3 and 4.
 *
 * It works on tokens rather than raw text. Text substitution cannot get `#`
 * and `##` right, and it has no reliable way to tell a macro name from the
 * same letters inside a string literal. Operating on the token stream the
 * lexer already produces avoids both problems.
 *
 * Directives are recognised by a '#' that begins a line, and a directive ends
 * at the next token that begins a line -- the token stream carries no
 * newlines, so the at_line_start flag stands in for them.
 */

struct pp_options {
    const char *const *include_paths;
    int include_path_count;

    /* -D arguments, each "NAME" or "NAME=value". */
    const char *const *defines;
    int define_count;
};

/*
 * Read a translation unit, run every directive, expand every macro, and return
 * the resulting token stream. Errors are reported through diag; the caller
 * should check diag_has_errors() rather than a return value, and the token
 * stream is still returned so later stages can be reached in a test.
 */
void preprocess_file(const char *path, const struct pp_options *options,
    struct token **tokens, int *token_count);

/* Expose the token stream for -E, once there is a flag for it. */
void preprocess_free(void);

#endif
