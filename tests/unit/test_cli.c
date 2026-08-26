/*
 * Option parsing, tested in process. Getting these wrong is quiet and
 * expensive -- a misread -o writes somewhere unexpected, a mishandled -Werror
 * turns a warning into a failed build or fails to.
 *
 * Some cases deliberately provoke usage errors, so this test prints to stderr
 * even when it passes.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "cli.h"
#include "unit.h"

/* cli_parse takes argv; build one that looks the way the C runtime supplies it. */
static int parse(struct options *options, int *should_exit, int count, ...)
{
    char *argv[16];
    va_list args;
    int i;

    argv[0] = (char *)"donkey";
    va_start(args, count);
    for (i = 0; i < count; i++) {
        argv[i + 1] = va_arg(args, char *);
    }
    va_end(args);

    return cli_parse(count + 1, argv, options, should_exit);
}

static void test_input_and_default_output(void)
{
    struct options options;
    int should_exit = 0;

    parse(&options, &should_exit, 1, (char *)"main.c");
    check_int("continues", should_exit, 0);
    check_str("input", options.input, "main.c");
    check_str("default output", options.output, "output.asm");
}

static void test_output_option(void)
{
    struct options options;
    int should_exit = 0;

    parse(&options, &should_exit, 3, (char *)"-o", (char *)"out.s", (char *)"main.c");
    check_str("-o input", options.input, "main.c");
    check_str("-o output", options.output, "out.s");

    parse(&options, &should_exit, 2, (char *)"main.c", (char *)"--output=out2.s");
    check_str("--output= form", options.output, "out2.s");
}

static void test_legacy_positional_output(void)
{
    struct options options;
    int should_exit = 0;

    /* The original interface, still accepted: donkey input.c output.asm */
    parse(&options, &should_exit, 2, (char *)"main.c", (char *)"main.asm");
    check_int("continues", should_exit, 0);
    check_str("legacy input", options.input, "main.c");
    check_str("legacy output", options.output, "main.asm");
}

static void test_warning_flags(void)
{
    struct options options;
    int should_exit = 0;

    parse(&options, &should_exit, 2, (char *)"-Werror", (char *)"main.c");
    check_int("-Werror", options.warnings_are_errors, 1);

    parse(&options, &should_exit, 2, (char *)"-w", (char *)"main.c");
    check_int("-w", options.suppress_warnings, 1);

    parse(&options, &should_exit, 2, (char *)"-Wall", (char *)"main.c");
    check_int("-Wall is accepted", should_exit, 0);
    check_int("-Wall is not -Werror", options.warnings_are_errors, 0);
}

static void test_dump_flags(void)
{
    struct options options;
    int should_exit = 0;

    parse(&options, &should_exit, 2, (char *)"--dump-tokens", (char *)"main.c");
    check_int("--dump-tokens", options.dump_tokens, 1);

    parse(&options, &should_exit, 2, (char *)"--dump-ast", (char *)"main.c");
    check_int("--dump-ast", options.dump_ast, 1);
}

static void test_missing_input_is_an_error(void)
{
    struct options options;
    int should_exit = 0;
    int status = parse(&options, &should_exit, 0);

    check_int("stops", should_exit, 1);
    check_int("fails", status, EXIT_FAILURE);
}

static void test_roadmap_flags_are_rejected(void)
{
    struct options options;
    int should_exit = 0;
    int status;

    /* Rejected with an explanation rather than accepted and ignored. */
    status = parse(&options, &should_exit, 2, (char *)"-O2", (char *)"main.c");
    check_int("-O2 stops", should_exit, 1);
    check_int("-O2 fails", status, EXIT_FAILURE);

    /* -E, -I and -D became real once the preprocessor landed. */
    should_exit = 0;
    parse(&options, &should_exit, 2, (char *)"-E", (char *)"main.c");
    check_int("-E is accepted", options.preprocess_only, 1);

    should_exit = 0;
    parse(&options, &should_exit, 3, (char *)"-I", (char *)"inc", (char *)"main.c");
    check_int("-I recorded", options.include_path_count, 1);
    check_str("-I path", options.include_paths[0], "inc");

    should_exit = 0;
    parse(&options, &should_exit, 2, (char *)"-DNAME=1", (char *)"main.c");
    check_int("-D recorded", options.define_count, 1);
    check_str("-D text", options.defines[0], "NAME=1");

    /* -o must not be mistaken for the -O it shares a letter with. */
    should_exit = 0;
    status = parse(&options, &should_exit, 3, (char *)"-o", (char *)"out.s",
        (char *)"main.c");
    check_int("-o still works", status, EXIT_SUCCESS);
    check_str("-o output", options.output, "out.s");
}

static void test_help_stops_successfully(void)
{
    struct options options;
    int should_exit = 0;
    int status = parse(&options, &should_exit, 1, (char *)"--help");

    check_int("--help stops", should_exit, 1);
    check_int("--help succeeds", status, EXIT_SUCCESS);
}

int main(void)
{
    test_input_and_default_output();
    test_output_option();
    test_legacy_positional_output();
    test_warning_flags();
    test_dump_flags();
    test_missing_input_is_an_error();
    test_roadmap_flags_are_rejected();
    test_help_stops_successfully();

    return unit_report("cli");
}
