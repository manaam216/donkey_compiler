#ifndef DONKEY_UNIT_H
#define DONKEY_UNIT_H

#include <stdio.h>
#include <string.h>

/*
 * A very small assertion helper shared by the unit tests.
 *
 * These exercise the compiler's stages in process, rather than by running the
 * binary and reading its output, which is what makes it practical to test
 * pieces that never reach the generated assembly -- option parsing, type
 * layout, token positions.
 */

static int unit_failures;

static void check_int(const char *label, long actual, long expected)
{
    if (actual != expected) {
        printf("FAIL %s: expected %ld, got %ld\n", label, expected, actual);
        unit_failures++;
    }
}

static void check_str(const char *label, const char *actual, const char *expected)
{
    if (!actual || strcmp(actual, expected) != 0) {
        printf("FAIL %s: expected '%s', got '%s'\n", label, expected,
            actual ? actual : "(null)");
        unit_failures++;
    }
}

static int unit_report(const char *suite)
{
    if (unit_failures) {
        printf("%d %s check(s) failed.\n", unit_failures, suite);
        return 1;
    }
    printf("All %s checks passed.\n", suite);
    return 0;
}

#endif
