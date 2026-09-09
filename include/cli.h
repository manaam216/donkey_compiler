#ifndef DONKEY_CLI_H
#define DONKEY_CLI_H

#include <stdio.h>

/*
 * Command-line parsing.
 *
 * Only options with something behind them are accepted. Flags on the roadmap
 * but not yet implemented -- -O without an optimiser, -g without debug info --
 * are rejected with an explanation rather than silently ignored, so a build
 * never quietly does something other than what was asked.
 */

#define MAX_INCLUDE_PATHS 32
#define MAX_DEFINES 32

struct options {
    const char *input;
    const char *output;

    int preprocess_only;

    const char *include_paths[MAX_INCLUDE_PATHS];
    int include_path_count;

    const char *defines[MAX_DEFINES];
    int define_count;

    int dump_tokens;
    int dump_ast;

    /*
     * --dump-ir shows the IR as lowering left it, with locals still in memory;
     * --dump-ssa shows it after promotion, with phis. Having both is what makes
     * the SSA pass reviewable: the difference between them is what it did.
     */
    int dump_ir;
    int dump_ssa;

    /*
     * -O0 through -O3. Zero is not "the passes with their flags off" but no
     * passes at all, so the unoptimised IR stays available to compare against.
     */
    int optimise;
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
