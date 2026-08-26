#ifndef DONKEY_DIAG_H
#define DONKEY_DIAG_H

#include "defs.h"

/*
 * Diagnostics for every stage.
 *
 * Reporting a problem does not end compilation. Each stage records what it
 * found and carries on, so one run can surface several genuine problems
 * instead of the first one and nothing else. Whether to continue is the
 * caller's decision, taken by asking diag_has_errors() at a point where
 * stopping is safe.
 *
 * Messages follow the conventional compiler format, which editors and CI can
 * parse:
 *
 *     path.c:3:12: error: use of undeclared variable 'missing'
 *         return missing + 1;
 *                ^
 */

typedef enum {
    DIAG_ERROR,
    DIAG_WARNING,
    DIAG_NOTE
} DiagLevel;

/*
 * Load the source so diagnostics can quote the offending line. Safe to call
 * with a path that cannot be read: messages simply lose the quoted line.
 */
void diag_init(const char *source_path);
void diag_cleanup(void);

void diag_at(DiagLevel level, SourceLocation location, const char *format, ...);

/*
 * Emit "path: In function 'name':" before the next diagnostic, the way a C
 * compiler groups messages. Repeated calls with the same name are ignored, and
 * NULL clears it.
 */
void diag_set_function(const char *name);

int diag_error_count(void);
int diag_warning_count(void);
int diag_has_errors(void);

/*
 * Treat warnings as errors. Wired to a command-line flag later; until then it
 * is off, and warnings never fail a build.
 */
void diag_set_warnings_are_errors(int enabled);

/* Drop warnings entirely, for -w. Errors are unaffected. */
void diag_set_warnings_suppressed(int enabled);

/*
 * True once so many errors have been reported that further ones are more
 * likely to be noise from a confused parser than real problems. Stages use it
 * to stop early.
 */
int diag_too_many_errors(void);

/*
 * An invariant the compiler itself was supposed to guarantee -- an AST shape
 * semantic analysis should already have rejected, say. Unlike an ordinary
 * error this does not accumulate: once an invariant is broken, continuing
 * would emit wrong code rather than report a problem, so this reports a
 * compiler bug and stops.
 */
void diag_internal(SourceLocation location, const char *format, ...);

#endif
