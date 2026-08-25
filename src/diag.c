#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "diag.h"

#define MAX_REPORTED_ERRORS 20

static const char *diag_path;
static char *diag_source;          /* whole file, for quoting lines */
static long diag_source_length;
static const char *diag_function;
static int diag_errors;
static int diag_warnings;
static int diag_warnings_are_errors;
static int diag_warnings_suppressed;
static int diag_suppressed;

void diag_init(const char *source_path)
{
    FILE *file;
    long size;

    diag_path = source_path;
    diag_function = NULL;
    diag_errors = 0;
    diag_warnings = 0;
    diag_suppressed = 0;

    free(diag_source);
    diag_source = NULL;
    diag_source_length = 0;

    file = fopen(source_path, "rb");
    if (!file) {
        return;                    /* diagnostics still work, just unquoted */
    }

    if (fseek(file, 0, SEEK_END) == 0 && (size = ftell(file)) >= 0) {
        rewind(file);
        diag_source = malloc((size_t)size + 1);
        if (diag_source) {
            diag_source_length = (long)fread(diag_source, 1, (size_t)size, file);
            diag_source[diag_source_length] = '\0';
        }
    }
    fclose(file);
}

void diag_cleanup(void)
{
    free(diag_source);
    diag_source = NULL;
    diag_source_length = 0;
    diag_function = NULL;
}

void diag_set_function(const char *name)
{
    diag_function = name;
}

void diag_set_warnings_are_errors(int enabled)
{
    diag_warnings_are_errors = enabled;
}

void diag_set_warnings_suppressed(int enabled)
{
    diag_warnings_suppressed = enabled;
}

int diag_error_count(void)
{
    return diag_errors;
}

int diag_warning_count(void)
{
    return diag_warnings;
}

int diag_has_errors(void)
{
    return diag_errors > 0 || (diag_warnings_are_errors && diag_warnings > 0);
}

int diag_too_many_errors(void)
{
    return diag_errors >= MAX_REPORTED_ERRORS;
}

/* Start of the given 1-based line, or NULL if the source has no such line. */
static const char *line_start(int line)
{
    const char *p = diag_source;
    int current = 1;

    if (!p || line < 1) {
        return NULL;
    }
    while (current < line && *p) {
        if (*p == '\n') {
            current++;
        }
        p++;
    }
    return current == line ? p : NULL;
}

/*
 * Quote the offending line and mark the column. Tabs are copied into the
 * marker line so the caret stays under the right character whatever the
 * reader's tab width.
 */
static void print_source_line(SourceLocation location)
{
    const char *start = line_start(location.line);
    const char *p;

    if (!start || location.column < 1) {
        return;
    }

    fprintf(stderr, "    ");
    for (p = start; *p && *p != '\n'; p++) {
        fputc(*p == '\r' ? ' ' : *p, stderr);
    }
    fputc('\n', stderr);

    fprintf(stderr, "    ");
    for (p = start; *p && *p != '\n' && (p - start) < location.column - 1; p++) {
        fputc(*p == '\t' ? '\t' : ' ', stderr);
    }
    fputs("^\n", stderr);
}

static const char *level_name(DiagLevel level)
{
    switch (level) {
        case DIAG_WARNING: return "warning";
        case DIAG_NOTE:    return "note";
        default:           return "error";
    }
}

void diag_at(DiagLevel level, SourceLocation location, const char *format, ...)
{
    static const char *reported_function;
    va_list args;

    if (level == DIAG_ERROR) {
        diag_errors++;
        /*
         * Past the cap, keep counting but stop printing: further messages from
         * a parser that has lost its place are rarely about real problems.
         */
        if (diag_errors > MAX_REPORTED_ERRORS) {
            if (!diag_suppressed) {
                diag_suppressed = 1;
                fprintf(stderr, "%s: note: too many errors; further ones suppressed\n",
                    diag_path ? diag_path : "<input>");
            }
            return;
        }
    } else if (level == DIAG_WARNING) {
        if (diag_warnings_suppressed) {
            return;
        }
        diag_warnings++;
    }

    if (diag_function && diag_function != reported_function) {
        fprintf(stderr, "%s: In function '%s':\n",
            diag_path ? diag_path : "<input>", diag_function);
        reported_function = diag_function;
    }

    if (location.line > 0) {
        fprintf(stderr, "%s:%d:%d: %s: ", diag_path ? diag_path : "<input>",
            location.line, location.column, level_name(level));
    } else {
        fprintf(stderr, "%s: %s: ", diag_path ? diag_path : "<input>",
            level_name(level));
    }

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);

    if (location.line > 0) {
        print_source_line(location);
    }
}

void diag_internal(SourceLocation location, const char *format, ...)
{
    va_list args;

    if (location.line > 0) {
        fprintf(stderr, "%s:%d:%d: internal error: ",
            diag_path ? diag_path : "<input>", location.line, location.column);
    } else {
        fprintf(stderr, "%s: internal error: ", diag_path ? diag_path : "<input>");
    }

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);
    fputs("this is a bug in the compiler, not in the input\n", stderr);

    if (location.line > 0) {
        print_source_line(location);
    }
    exit(EXIT_FAILURE);
}
