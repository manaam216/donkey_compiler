#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cli.h"

#define DONKEY_VERSION "0.9.0"

enum {
    OPT_DUMP_TOKENS = 1000,
    OPT_DUMP_AST,
    OPT_DUMP_IR,
    OPT_DUMP_SSA,
    OPT_VERSION
};

static const struct option long_options[] = {
    { "dump-tokens", no_argument,       NULL, OPT_DUMP_TOKENS },
    { "dump-ast",    no_argument,       NULL, OPT_DUMP_AST },
    { "dump-ir",     no_argument,       NULL, OPT_DUMP_IR },
    { "dump-ssa",    no_argument,       NULL, OPT_DUMP_SSA },
    { "verbose",     no_argument,       NULL, 'v' },
    { "help",        no_argument,       NULL, 'h' },
    { "version",     no_argument,       NULL, OPT_VERSION },
    { "output",      required_argument, NULL, 'o' },
    { "include",     required_argument, NULL, 'I' },
    { NULL,          0,                 NULL, 0 }
};

void cli_usage(const char *program, FILE *stream)
{
    fprintf(stream,
        "Usage: %s [options] <input.c>\n"
        "\n"
        "Options:\n"
        "  -o, --output <file>   Write assembly to <file> (default: output.asm)\n"
        "  -S                    Emit assembly; the only mode currently supported\n"
        "  -Wall                 Enable all warnings (already the default)\n"
        "  -Werror               Treat warnings as errors\n"
        "  -w                    Suppress warnings\n"
        "      --dump-tokens     Print the token stream and stop\n"
        "      --dump-ast        Print the syntax tree, annotated, and stop\n"
        "  -v, --verbose         Report each stage as it runs\n"
        "  -h, --help            Show this help\n"
        "      --version         Show the version\n"
        "\n"
        "For compatibility, a second positional argument is taken as the output\n"
        "path, as in: %s input.c output.asm\n",
        program, program);
}

/*
 * Options that belong to stages Donkey does not have yet. Saying so is more
 * useful than "unrecognized option", and much more useful than accepting the
 * flag and ignoring it.
 */
static int reject_unimplemented(const char *argument)
{
    static const struct {
        const char *prefix;
        const char *reason;
    } pending[] = {
        { "-O", "there is no optimiser yet" },
        { "-g", "debug information is not generated yet" },
        { "-c", "Donkey emits assembly; it does not assemble or link" }
    };
    size_t i;

    for (i = 0; i < sizeof(pending) / sizeof(pending[0]); i++) {
        size_t length = strlen(pending[i].prefix);

        if (strncmp(argument, pending[i].prefix, length) == 0) {
            /* -O alone must not swallow -o, which is a different option. */
            if (strcmp(pending[i].prefix, "-O") == 0 && argument[1] != 'O') {
                continue;
            }
            fprintf(stderr, "%s: not supported yet: %s\n",
                argument, pending[i].reason);
            return 1;
        }
    }
    return 0;
}

int cli_parse(int argc, char *argv[], struct options *options, int *should_exit)
{
    int option;
    int i;

    memset(options, 0, sizeof(*options));
    options->output = NULL;
    *should_exit = 0;

    /*
     * Check for roadmap options first: getopt would report them as unknown,
     * which says nothing about why they are unavailable.
     */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && reject_unimplemented(argv[i])) {
            *should_exit = 1;
            return EXIT_FAILURE;
        }
    }

    opterr = 0;         /* report unknown options in our own wording */
    /*
     * 0, not 1: GNU getopt keeps internal scan state between calls, and only
     * a zero here makes it reinitialise. Without this a second parse in the
     * same process -- which is exactly what the unit tests do -- silently
     * reads the wrong arguments.
     */
    optind = 0;

    while ((option = getopt_long(argc, argv, "o:I:D:ESW:whv", long_options, NULL)) != -1) {
        switch (option) {
            case 'o':
                options->output = optarg;
                break;
            case 'S':
                break;      /* the only code-generating mode there is */
            case 'E':
                options->preprocess_only = 1;
                break;
            case 'I':
                if (options->include_path_count >= MAX_INCLUDE_PATHS) {
                    fprintf(stderr, "too many -I options (limit is %d)\n",
                        MAX_INCLUDE_PATHS);
                    *should_exit = 1;
                    return EXIT_FAILURE;
                }
                options->include_paths[options->include_path_count++] = optarg;
                break;
            case 'D':
                options->defines[options->define_count++] = optarg;
                if (options->define_count >= MAX_DEFINES) {
                    fprintf(stderr, "too many -D options (limit is %d)\n",
                        MAX_DEFINES);
                    *should_exit = 1;
                    return EXIT_FAILURE;
                }
                break;
            case 'W':
                if (strcmp(optarg, "error") == 0) {
                    options->warnings_are_errors = 1;
                } else if (strcmp(optarg, "all") == 0) {
                    /* every warning is already on */
                } else {
                    fprintf(stderr, "unknown warning option: -W%s\n", optarg);
                    *should_exit = 1;
                    return EXIT_FAILURE;
                }
                break;
            case 'w':
                options->suppress_warnings = 1;
                break;
            case OPT_DUMP_TOKENS:
                options->dump_tokens = 1;
                break;
            case OPT_DUMP_AST:
                options->dump_ast = 1;
                break;
            case OPT_DUMP_IR:
                options->dump_ir = 1;
                break;
            case OPT_DUMP_SSA:
                options->dump_ssa = 1;
                break;
            case 'v':
                options->verbose = 1;
                break;
            case 'h':
                cli_usage(argv[0], stdout);
                *should_exit = 1;
                return EXIT_SUCCESS;
            case OPT_VERSION:
                printf("donkey %s\n", DONKEY_VERSION);
                *should_exit = 1;
                return EXIT_SUCCESS;
            default:
                fprintf(stderr, "unrecognised option: %s\n", argv[optind - 1]);
                cli_usage(argv[0], stderr);
                *should_exit = 1;
                return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "%s: no input file\n", argv[0]);
        cli_usage(argv[0], stderr);
        *should_exit = 1;
        return EXIT_FAILURE;
    }

    options->input = argv[optind++];

    /* Legacy form: donkey input.c output.asm */
    if (optind < argc && !options->output) {
        options->output = argv[optind++];
    }

    if (optind < argc) {
        fprintf(stderr, "%s: unexpected argument '%s'\n", argv[0], argv[optind]);
        *should_exit = 1;
        return EXIT_FAILURE;
    }

    if (!options->output) {
        options->output = "output.asm";
    }

    return EXIT_SUCCESS;
}
