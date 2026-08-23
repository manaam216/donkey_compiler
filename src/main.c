#include <stdio.h>
#include <stdlib.h>
#include "defs.h"
#include "decl.h"
#include "type.h"
#include "symbol.h"
#include "diag.h"

/*
 * Each stage reports everything it finds rather than stopping at the first
 * problem, so one run can surface several. Compilation stops between stages,
 * where it is safe to: there is no point type-checking a tree the parser could
 * not build, or generating code for one that failed to type-check.
 */
int main(int argc, char *argv[])
{
    const char *output_file;
    FILE *infile;
    struct token *tokens = NULL;
    int token_count = 0;
    int token_index = 0;
    struct ast_node *ast;
    int status = EXIT_SUCCESS;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <input_file> [output_file]\n", argv[0]);
        return EXIT_FAILURE;
    }

    output_file = argc == 3 ? argv[2] : "output.asm";

    infile = fopen(argv[1], "r");
    if (!infile) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    diag_init(argv[1]);

    lex(infile, argv[1], &tokens, &token_count);

    /*
     * A token stream with holes in it would send the parser down paths its
     * error recovery was not written for, and every message after that would
     * be a consequence of the first.
     */
    if (diag_has_errors()) {
        status = EXIT_FAILURE;
        goto done_tokens;
    }

    ast = parse_program(tokens, &token_index, argv[1]);

    if (diag_has_errors()) {
        status = EXIT_FAILURE;
        goto done_ast;
    }

    if (!semantic_analyze(ast, argv[1]) || diag_has_errors()) {
        status = EXIT_FAILURE;
        goto done_ast;
    }

    write_assembly_to_file(output_file, ast);

    if (diag_has_errors()) {
        status = EXIT_FAILURE;
        goto done_ast;
    }

    printf("Compiled %s -> %s\n", argv[1], output_file);

done_ast:
    free_ast_node(ast);
done_tokens:
    free_tokens(tokens, token_count);
    ty_cleanup();
    sym_cleanup();
    diag_cleanup();
    fclose(infile);

    if (status != EXIT_SUCCESS && diag_error_count() > 1) {
        fprintf(stderr, "%d errors\n", diag_error_count());
    }
    return status;
}
