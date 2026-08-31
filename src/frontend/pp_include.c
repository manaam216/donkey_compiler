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

    for (i = 0; i < pp.once_count; i++) {
        if (strcmp(pp.once_files[i], path) == 0) {
            return 1;
        }
    }
    return 0;
}

void pp_mark_once(const char *path)
{
    if (pp.once_count >= pp.once_capacity) {
        pp.once_capacity = pp.once_capacity ? pp.once_capacity * 2 : 16;
        pp.once_files = xrealloc(pp.once_files,
            (size_t)pp.once_capacity * sizeof(*pp.once_files), "preprocessor pp");
    }
    pp.once_files[pp.once_count++] = strdup(path);
}

/*
 * Find an included file. A quoted include looks beside the including file
 * first, then falls back to the search path; an angled one uses the search
 * path only.
 */
char *pp_resolve_include(const char *name, int angled, const char *from_path)
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
            text = read_whole_file(candidate, &unused);
            if (text) {
                free(text);
                return strdup(candidate);
            }
        }
    }

    for (i = 0; i < pp.include_path_count; i++) {
        size_t unused = 0;
        char *text;

        if (strlen(pp.include_paths[i]) + 1 + strlen(name) >= sizeof(candidate)) {
            continue;
        }
        snprintf(candidate, sizeof(candidate), "%s/%s",
            pp.include_paths[i], name);
        text = read_whole_file(candidate, &unused);
        if (text) {
            free(text);
            return strdup(candidate);
        }
    }

    return NULL;
}

void pp_include_file(const char *path, SourceLocation location,
    struct token_list *out)
{
    char *text;
    size_t length = 0;
    struct token *tokens = NULL;
    int count = 0;

    if (pp.depth >= MAX_INCLUDE_DEPTH) {
        diag_at(DIAG_ERROR, location,
            "#include nested too deeply (limit is %d)", MAX_INCLUDE_DEPTH);
        return;
    }
    if (marked_once(path)) {
        return;
    }

    text = read_whole_file(path, &length);
    if (!text) {
        diag_at(DIAG_ERROR, location, "cannot read '%s'", path);
        return;
    }

    lex_text(text, length, pp_intern_path(path), &tokens, &count);

    pp.depth++;
    pp_process_tokens(tokens, count, path, out);
    pp.depth--;

    free_tokens(tokens, count);
    free(text);
}
