#include <stdio.h>
#include <stdlib.h>
#include "defs.h"
#include "decl.h"

void generate_function(struct ast_node *node, FILE *output)
{
    fprintf(output, ".globl _%s\n", node->value);
    fprintf(output, "_%s:\n", node->value);
    generate_statement(node->left, output);
}

void generate_statement(struct ast_node *node, FILE *output)
{
    if (node->type == AST_RETURN) {
        generate_exp(node->left, output);
        fprintf(output, "    ret\n");
    }
}

void generate_binop(struct ast_node *node, FILE *output)
{
    generate_exp(node->left, output);
    fprintf(output, "    push    %%eax\n");

    generate_exp(node->right, output);
    fprintf(output, "    pop     %%ecx\n");

    switch (node->type) {
        case AST_ADD:
            fprintf(output, "    addl    %%ecx, %%eax\n");
            break;
        case AST_SUB:
            fprintf(output, "    subl    %%eax, %%ecx\n");
            fprintf(output, "    movl    %%ecx, %%eax\n");
            break;
        case AST_MUL:
            fprintf(output, "    imull   %%ecx, %%eax\n");
            break;
        case AST_DIV:
            fprintf(output, "    push    %%eax\n");
            fprintf(output, "    movl    %%ecx, %%eax\n");
            fprintf(output, "    pop     %%ecx\n");
            fprintf(output, "    cdq\n");
            fprintf(output, "    idivl   %%ecx\n");
            break;
        default:
            fprintf(stderr, "Unsupported operation in AST\n");
            exit(1);
    }
}

void generate_exp(struct ast_node *node, FILE *output)
{
    switch (node->type) {
        case AST_INTLIT:
            fprintf(output, "    movl    $%s, %%eax\n", node->value);
            break;
        case AST_IDENTIFIER:
            fprintf(output, "    movl    %s, %%eax\n", node->value);
            break;
        case AST_ADD:
        case AST_SUB:
        case AST_MUL:
        case AST_DIV:
            generate_binop(node, output);
            break;
        case AST_NEGATION:
            generate_exp(node->left, output);
            fprintf(output, "    negl    %%eax\n");
            break;
        case AST_BITWISE_COMPLEMENT:
            generate_exp(node->left, output);
            fprintf(output, "    notl    %%eax\n");
            break;
        case AST_LOGICAL_NEGATION:
            generate_exp(node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    sete    %%al\n");
            break;
        default:
            fprintf(stderr, "Unsupported AST node type: %d\n", node->type);
            exit(1);
    }
}

char* generate(struct ast_node *ast)
{
    FILE *output = fopen("donkey_generate.tmp", "w+");
    if (output == NULL) {
        perror("Failed to create temporary file for assembly generation");
        exit(EXIT_FAILURE);
    }

    generate_function(ast, output);
    rewind(output);
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

    fclose(output);
    remove("donkey_generate.tmp");

    return assembly;
}

void write_assembly_to_file(const char *filename, struct ast_node *ast)
{
    FILE *out_file = fopen(filename, "w");
    if (!out_file) {
        perror("Failed to open file for writing");
        exit(EXIT_FAILURE);
    }

    generate_function(ast, out_file);
    fclose(out_file);
}
