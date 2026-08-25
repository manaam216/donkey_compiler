#include <stdio.h>
#include <stdlib.h>
#include "defs.h"
#include "decl.h"
#include "type.h"
#include "symbol.h"
#include "diag.h"
#include "dump.h"
#include "cli.h"

/*
 * Each stage reports everything it finds rather than stopping at the first
 * problem, so one run can surface several. Compilation stops between stages,
 * where it is safe to: there is no point type-checking a tree the parser could
 * not build, or generating code for one that failed to type-check.
 */
int main(int argc, char *argv[])
{
    struct options options;
    FILE *infile;
    struct token *tokens = NULL;
    int token_count = 0;
    int token_index = 0;
    struct ast_node *ast = NULL;
    int should_exit = 0;
    int status;

    status = cli_parse(argc, argv, &options, &should_exit);
    if (should_exit) {
        return status;
    }

    infile = fopen(options.input, "r");
    if (!infile) {
        perror(options.input);
        return EXIT_FAILURE;
    }

    diag_init(options.input);
    diag_set_warnings_are_errors(options.warnings_are_errors);
    diag_set_warnings_suppressed(options.suppress_warnings);

    if (options.verbose) {
        fprintf(stderr, "lexing %s\n", options.input);
    }
    lex(infile, options.input, &tokens, &token_count);

    if (options.dump_tokens) {
        dump_tokens(tokens, token_count);
        status = diag_has_errors() ? EXIT_FAILURE : EXIT_SUCCESS;
        goto done_tokens;
    }

    /*
     * A token stream with holes in it would send the parser down paths its
     * error recovery was not written for, and every message after that would
     * be a consequence of the first.
     */
    if (diag_has_errors()) {
        status = EXIT_FAILURE;
        goto done_tokens;
    }

    if (options.verbose) {
        fprintf(stderr, "parsing\n");
    }
    ast = parse_program(tokens, &token_index, options.input);

    if (diag_has_errors()) {
        status = EXIT_FAILURE;
        goto done_ast;
    }

    if (options.verbose) {
        fprintf(stderr, "analysing\n");
    }
    if (!semantic_analyze(ast, options.input) || diag_has_errors()) {
        status = EXIT_FAILURE;
        /* Still dump if asked: a partly annotated tree is what you want to see. */
        if (options.dump_ast) {
            dump_ast(ast);
        }
        goto done_ast;
    }

    /* Dumped after analysis, so the tree carries its types and storage. */
    if (options.dump_ast) {
        dump_ast(ast);
        status = EXIT_SUCCESS;
        goto done_ast;
    }

    if (options.verbose) {
        fprintf(stderr, "generating %s\n", options.output);
    }
    write_assembly_to_file(options.output, ast);

    if (diag_has_errors()) {
        status = EXIT_FAILURE;
        goto done_ast;
    }

    printf("Compiled %s -> %s\n", options.input, options.output);
    status = EXIT_SUCCESS;

done_ast:
    free_ast_node(ast);
done_tokens:
    free_tokens(tokens, token_count);
    ty_cleanup();
    sym_cleanup();
    fclose(infile);

    if (status != EXIT_SUCCESS && diag_error_count() > 1) {
        fprintf(stderr, "%d errors\n", diag_error_count());
    }
    diag_cleanup();
    return status;
}
