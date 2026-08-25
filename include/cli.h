#ifndef DONKEY_CLI_H
#define DONKEY_CLI_H

#include <stdio.h>

/*
 * Command-line parsing.
 *
 * Only options with something behind them are accepted. Flags on the roadmap
 * but not yet implemented -- -E, -I and -D without a preprocessor, -O without
 * an optimiser, -g without debug info -- are rejected with an explanation
 * rather than silently ignored, so a build never quietly does something other
 * than what was asked.
 */

struct options {
    const char *input;
    const char *output;

    int dump_tokens;
    int dump_ast;
    int verbose;

    int warnings_are_errors;
    int suppress_warnings;
};

/*
 * Fill in *options from the command line.
 *
 * Returns 0 to continue, or an exit status if the program should stop now --
 * which covers both a usage error and a request that is already satisfied,
 * such as --help.
 */
int cli_parse(int argc, char *argv[], struct options *options, int *should_exit);

void cli_usage(const char *program, FILE *stream);

#endif
