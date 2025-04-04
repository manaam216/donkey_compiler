#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "decl.h"

// Helper function to generate assembly for a function node
void generate_function(struct ast_node *node, FILE *output) {
    // The function name
    fprintf(output, ".globl _%s\n", node->value); // Global function symbol
    fprintf(output, "_%s:\n", node->value);       // Function label

    // Generate the statement inside the function (the return value)
    generate_statement(node->left, output);      // 'left' is the return statement
}

// Helper function to generate assembly for a statement node
void generate_statement(struct ast_node *node, FILE *output) {
    if (node->type == AST_RETURN) {
        // Move the return value into the %eax register
        generate_exp(node->left, output);    // 'left' is the expression (integer literal)

        // Add the return instruction
        fprintf(output, "    ret\n");
    }
}

// Helper function to generate assembly for binary operations
void generate_binop(struct ast_node *node, FILE *output) {
    // Generate the left operand
    generate_exp(node->left, output);
    // Push the result of the left operand onto the stack
    fprintf(output, "    push %%eax\n");

    // Generate the right operand
    generate_exp(node->right, output);

    // Pop the left operand from the stack into %ecx
    fprintf(output, "    pop %%ecx\n");

    switch (node->type) {
        case AST_ADD:
            // Perform addition (eax = eax + ecx)
            fprintf(output, "    addl %%ecx, %%eax\n");
            break;
        case AST_SUB:
            // Perform subtraction (eax = eax - ecx)
            fprintf(output, "    subl %%ecx, %%eax\n");
            break;
        case AST_MUL:
            // Perform multiplication (eax = eax * ecx)
            fprintf(output, "    imull %%ecx, %%eax\n");
            break;
        case AST_DIV:
            // Perform division (eax = eax / ecx, edx = eax % ecx)
            fprintf(output, "    cdq\n");            // Sign-extend eax into edx
            fprintf(output, "    idivl %%ecx\n");   // Divide edx:eax by ecx, quotient in eax, remainder in edx
            break;
        default:
            fprintf(stderr, "Unsupported operation in AST\n");
            exit(1);
    }
}


// Helper function to generate assembly for an expression node
void generate_exp(struct ast_node *node, FILE *output) {
    switch (node->type) {
        case AST_INTLIT:
            // Move the integer literal into the %eax register
            fprintf(output, "    movl    $%s, %%eax\n", node->value);  // Integer literal
            break;

        case AST_IDENTIFIER:
            // Move the value of the identifier into the %eax register
            fprintf(output, "    movl    %s, %%eax\n", node->value);  // Assuming the variable is in memory
            break;
        case AST_ADD:
        case AST_SUB:
        case AST_MUL:
        case AST_DIV:
            // Generate assembly for binary operations
            generate_binop(node, output);
            break;
        case AST_NEGATION:
            // Generate code for the operand, then negate
            generate_exp(node->left, output); // Operand
            fprintf(output, "    negl   %%eax\n");  // Negate the value in %eax
            break;

        case AST_BITWISE_COMPLEMENT:
            // Generate code for the operand, then apply bitwise complement
            generate_exp(node->left, output); // Operand
            fprintf(output, "    notl   %%eax\n");  // Apply bitwise NOT
            break;

        case AST_LOGICAL_NEGATION:
            // Generate code for the operand, then logical negation
            generate_exp(node->left, output); // Operand
            fprintf(output, "    cmpl   $0, %%eax\n");  // Compare EAX with 0
            fprintf(output, "    movl   $0, %%eax\n");  // Zero out EAX
            fprintf(output, "    sete   %%al\n");  // Set AL based on ZF flag (1 if EAX was 0)
            break;
        default:
            fprintf(stderr, "Unsupported AST node type: %d\n", node->type);
            exit(1);
    }
}

// Main function to generate the assembly for the entire AST
char* generate(struct ast_node *ast) {
    FILE *output = tmpfile();  // Use a temporary file to store the assembly code
    if (output == NULL) {
        perror("Failed to create temporary file for assembly generation");
        exit(EXIT_FAILURE);
    }

    // Generate assembly code for the program (which is just one function)
    generate_function(ast, output);

    // Rewind the file pointer to the beginning
    rewind(output);

    // Read the generated assembly code into a string
    fseek(output, 0, SEEK_END);
    long size = ftell(output);
    rewind(output);

    char *assembly = malloc(size + 1);
    if (assembly == NULL) {
        perror("Failed to allocate memory for assembly string");
        exit(EXIT_FAILURE);
    }

    fread(assembly, 1, size, output);
    assembly[size] = '\0';

    fclose(output);  // Close the temporary file

    return assembly;  // Return the generated assembly code
}

// Function to write the generated assembly to a file
void write_assembly_to_file(const char *filename, struct ast_node *ast) {
    FILE *out_file = fopen(filename, "w");
    if (!out_file) {
        perror("Failed to open file for writing");
        exit(EXIT_FAILURE);
    }

    // Generate assembly code and write it to the file
    char *assembly = generate(ast);
    fprintf(out_file, "%s", assembly);

    fclose(out_file);
    free(assembly);  // Don't forget to free the generated assembly
}
