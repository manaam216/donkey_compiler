#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "decl.h"

static char *variables[256];
static int variable_count = 0;
static int label_count = 0;

static int has_variable(const char *name)
{
    for (int i = 0; i < variable_count; i++) {
        if (strcmp(variables[i], name) == 0) {
            return 1;
        }
    }

    return 0;
}

static void add_variable(const char *name)
{
    if (!name || has_variable(name)) {
        return;
    }

    if (variable_count >= 256) {
        fprintf(stderr, "Too many identifiers in expression\n");
        exit(1);
    }

    variables[variable_count++] = strdup(name);
}

static void collect_variables(struct ast_node *node)
{
    if (!node) {
        return;
    }

    if (node->type == AST_IDENTIFIER) {
        add_variable(node->value);
    }

    collect_variables(node->left);
    collect_variables(node->right);
}

static void free_variables(void)
{
    for (int i = 0; i < variable_count; i++) {
        free(variables[i]);
        variables[i] = NULL;
    }
    variable_count = 0;
}

static void generate_variable_storage(FILE *output)
{
    if (variable_count == 0) {
        return;
    }

    fprintf(output, ".data\n");
    for (int i = 0; i < variable_count; i++) {
        fprintf(output, "_%s:\n", variables[i]);
        fprintf(output, "    .long   0\n");
    }
    fprintf(output, ".text\n");
}

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
        case AST_MOD:
            fprintf(output, "    push    %%eax\n");
            fprintf(output, "    movl    %%ecx, %%eax\n");
            fprintf(output, "    pop     %%ecx\n");
            fprintf(output, "    cdq\n");
            fprintf(output, "    idivl   %%ecx\n");
            fprintf(output, "    movl    %%edx, %%eax\n");
            break;
        case AST_BITWISE_AND:
            fprintf(output, "    andl    %%ecx, %%eax\n");
            break;
        case AST_BITWISE_OR:
            fprintf(output, "    orl     %%ecx, %%eax\n");
            break;
        case AST_BITWISE_XOR:
            fprintf(output, "    xorl    %%ecx, %%eax\n");
            break;
        case AST_EQUAL:
            fprintf(output, "    cmpl    %%eax, %%ecx\n");
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    sete    %%al\n");
            break;
        case AST_NOT_EQUAL:
            fprintf(output, "    cmpl    %%eax, %%ecx\n");
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    setne   %%al\n");
            break;
        case AST_LESS:
            fprintf(output, "    cmpl    %%eax, %%ecx\n");
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    setl    %%al\n");
            break;
        case AST_LESS_EQUAL:
            fprintf(output, "    cmpl    %%eax, %%ecx\n");
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    setle   %%al\n");
            break;
        case AST_GREATER:
            fprintf(output, "    cmpl    %%eax, %%ecx\n");
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    setg    %%al\n");
            break;
        case AST_GREATER_EQUAL:
            fprintf(output, "    cmpl    %%eax, %%ecx\n");
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    setge   %%al\n");
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
            fprintf(output, "    movl    _%s, %%eax\n", node->value);
            break;
        case AST_ADD:
        case AST_SUB:
        case AST_MUL:
        case AST_DIV:
        case AST_MOD:
        case AST_BITWISE_AND:
        case AST_BITWISE_OR:
        case AST_BITWISE_XOR:
        case AST_EQUAL:
        case AST_NOT_EQUAL:
        case AST_LESS:
        case AST_LESS_EQUAL:
        case AST_GREATER:
        case AST_GREATER_EQUAL:
            generate_binop(node, output);
            break;
        case AST_LOGICAL_AND: {
            int false_label = label_count++;
            int end_label = label_count++;

            generate_exp(node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    je      .L%d\n", false_label);
            generate_exp(node->right, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    je      .L%d\n", false_label);
            fprintf(output, "    movl    $1, %%eax\n");
            fprintf(output, "    jmp     .L%d\n", end_label);
            fprintf(output, ".L%d:\n", false_label);
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, ".L%d:\n", end_label);
            break;
        }
        case AST_LOGICAL_OR: {
            int true_label = label_count++;
            int end_label = label_count++;

            generate_exp(node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    jne     .L%d\n", true_label);
            generate_exp(node->right, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    jne     .L%d\n", true_label);
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    jmp     .L%d\n", end_label);
            fprintf(output, ".L%d:\n", true_label);
            fprintf(output, "    movl    $1, %%eax\n");
            fprintf(output, ".L%d:\n", end_label);
            break;
        }
        case AST_ASSIGN:
            generate_exp(node->right, output);
            fprintf(output, "    movl    %%eax, _%s\n", node->left->value);
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

    collect_variables(ast);
    generate_variable_storage(output);
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
    free_variables();

    return assembly;
}

void write_assembly_to_file(const char *filename, struct ast_node *ast)
{
    FILE *out_file = fopen(filename, "w");
    if (!out_file) {
        perror("Failed to open file for writing");
        exit(EXIT_FAILURE);
    }

    collect_variables(ast);
    generate_variable_storage(out_file);
    generate_function(ast, out_file);
    free_variables();
    fclose(out_file);
}
