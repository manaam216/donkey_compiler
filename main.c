#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "defs.h" // Include lexer functions
#include "decl.h"

// Main function for testing
int main(int argc, char *argv[]) 
{
    if (argc != 2) {
    fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
    exit(EXIT_FAILURE);
    }

    FILE *infile = fopen(argv[1], "r");
    if (!infile) {
    perror("Error opening file");
    exit(EXIT_FAILURE);
    }

    struct token *tokens = NULL;
    int token_count = 0;

    lex(infile, &tokens, &token_count);


    // Create a local copy of the tokens array to pass by value
    struct token *tokens_copy = malloc(token_count * sizeof(struct token));
    if (tokens_copy == NULL) {
        perror("Error allocating memory for token copy");
        exit(EXIT_FAILURE);
    }

    // Copy the tokens into the new array
    memcpy(tokens_copy, tokens, token_count * sizeof(struct token));

    // Print tokens (for debugging)
    for (int i = 0; i < token_count; i++) {
        printf("Token type: %d, value: %s\n", tokens_copy[i].type, tokens_copy[i].value);
    }

    int token_index = 0;
    // Parse the program with the local copy of tokens
    struct ast_node *ast = parse_program(tokens_copy, &token_index);

    // If you reach here, parsing succeeded, you can print the AST or process it further
    printf("Program parsed successfully\n");

    // Generate assembly and print it
    write_assembly_to_file("output.asm", ast);
    
    // Free the AST nodes
    free_ast_node(ast);

    // Clean up
    free_tokens(tokens, token_count);
    fclose(infile);

  return EXIT_SUCCESS;
}

