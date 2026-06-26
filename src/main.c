#include <stdio.h>
#include <stdlib.h>
#include "defs.h"
#include "decl.h"

int main(int argc, char *argv[])
{
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <input_file> [output_file]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *output_file = argc == 3 ? argv[2] : "output.asm";

    FILE *infile = fopen(argv[1], "r");
    if (!infile) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    struct token *tokens = NULL;
    int token_count = 0;

    lex(infile, argv[1], &tokens, &token_count);

    int token_index = 0;
    struct ast_node *ast = parse_program(tokens, &token_index, argv[1]);

    if (!semantic_analyze(ast, argv[1])) {
        free_ast_node(ast);
        free_tokens(tokens, token_count);
        fclose(infile);
        return EXIT_FAILURE;
    }

    write_assembly_to_file(output_file, ast);
    printf("Compiled %s -> %s\n", argv[1], output_file);

    free_ast_node(ast);
    free_tokens(tokens, token_count);
    fclose(infile);

    return EXIT_SUCCESS;
}
