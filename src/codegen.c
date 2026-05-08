#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "decl.h"

static struct {
    char *name;
    int offset;
} symbols[256];
static int symbol_count = 0;
static int local_stack_count = 0;
static int label_count = 0;
static int current_function_end_label = 0;
static int loop_break_labels[128];
static int loop_continue_labels[128];
static int loop_depth = 0;

static int find_local(const char *name)
{
    for (int i = 0; i < symbol_count; i++) {
        if (strcmp(symbols[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

static int local_offset(const char *name)
{
    int index = find_local(name);
    if (index < 0) {
        fprintf(stderr, "Use of undeclared identifier '%s'\n", name);
        exit(1);
    }

    return symbols[index].offset;
}

static void add_symbol(const char *name, int offset)
{
    if (!name) {
        return;
    }

    if (find_local(name) >= 0) {
        fprintf(stderr, "Redeclaration of local variable '%s'\n", name);
        exit(1);
    }

    if (symbol_count >= 256) {
        fprintf(stderr, "Too many local variables or parameters\n");
        exit(1);
    }

    symbols[symbol_count].name = strdup(name);
    symbols[symbol_count].offset = offset;
    symbol_count++;
}

static void add_param(const char *name, int index)
{
    add_symbol(name, 8 + (index * 4));
}

static void add_local(const char *name)
{
    local_stack_count++;
    add_symbol(name, -4 * local_stack_count);
}

static int collect_params(struct ast_node *node, int index)
{
    if (!node) {
        return index;
    }

    if (node->type != AST_PARAM_LIST) {
        fprintf(stderr, "Unsupported parameter node type: %d\n", node->type);
        exit(1);
    }

    add_param(node->left->value, index);
    return collect_params(node->right, index + 1);
}

static void collect_locals(struct ast_node *node)
{
    if (!node) {
        return;
    }

    if (node->type == AST_DECL) {
        add_local(node->value);
    }

    collect_locals(node->left);
    collect_locals(node->right);
}

static void free_locals(void)
{
    for (int i = 0; i < symbol_count; i++) {
        free(symbols[i].name);
        symbols[i].name = NULL;
        symbols[i].offset = 0;
    }
    symbol_count = 0;
    local_stack_count = 0;
}

static void generate_epilogue(FILE *output)
{
    fprintf(output, "    leave\n");
    fprintf(output, "    ret\n");
}

static void push_loop(int break_label, int continue_label)
{
    if (loop_depth >= 128) {
        fprintf(stderr, "Too many nested loops\n");
        exit(1);
    }

    loop_break_labels[loop_depth] = break_label;
    loop_continue_labels[loop_depth] = continue_label;
    loop_depth++;
}

static void pop_loop(void)
{
    if (loop_depth > 0) {
        loop_depth--;
    }
}

void generate_function(struct ast_node *node, FILE *output)
{
    collect_params(node->left, 0);
    collect_locals(node->right);
    current_function_end_label = label_count++;

    fprintf(output, ".globl _%s\n", node->value);
    fprintf(output, "_%s:\n", node->value);
    fprintf(output, "    push    %%ebp\n");
    fprintf(output, "    movl    %%esp, %%ebp\n");
    if (local_stack_count > 0) {
        fprintf(output, "    subl    $%d, %%esp\n", local_stack_count * 4);
    }
    generate_statement(node->right, output);
    fprintf(output, "    movl    $0, %%eax\n");
    fprintf(output, ".L%d:\n", current_function_end_label);
    generate_epilogue(output);
    free_locals();
}

void generate_program(struct ast_node *node, FILE *output)
{
    if (!node) {
        return;
    }

    switch (node->type) {
        case AST_PROGRAM:
            generate_program(node->left, output);
            break;
        case AST_FUNCTION_LIST:
            generate_program(node->left, output);
            generate_program(node->right, output);
            break;
        case AST_FUNCTION:
            generate_function(node, output);
            break;
        default:
            fprintf(stderr, "Unsupported program node type: %d\n", node->type);
            exit(1);
    }
}

void generate_statement(struct ast_node *node, FILE *output)
{
    if (!node) {
        return;
    }

    switch (node->type) {
        case AST_BLOCK:
            generate_statement(node->left, output);
            break;
        case AST_STATEMENT_LIST:
            generate_statement(node->left, output);
            generate_statement(node->right, output);
            break;
        case AST_DECL:
            if (node->left) {
                generate_exp(node->left, output);
                fprintf(output, "    movl    %%eax, %d(%%ebp)\n", local_offset(node->value));
            } else {
                fprintf(output, "    movl    $0, %d(%%ebp)\n", local_offset(node->value));
            }
            break;
        case AST_EXPR_STMT:
            generate_exp(node->left, output);
            break;
        case AST_RETURN:
            generate_exp(node->left, output);
            fprintf(output, "    jmp     .L%d\n", current_function_end_label);
            break;
        case AST_IF: {
            int else_label = label_count++;
            int end_label = label_count++;

            generate_exp(node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    je      .L%d\n", else_label);
            generate_statement(node->right->left, output);
            fprintf(output, "    jmp     .L%d\n", end_label);
            fprintf(output, ".L%d:\n", else_label);
            if (node->right->right) {
                generate_statement(node->right->right, output);
            }
            fprintf(output, ".L%d:\n", end_label);
            break;
        }
        case AST_WHILE: {
            int start_label = label_count++;
            int end_label = label_count++;

            push_loop(end_label, start_label);
            fprintf(output, ".L%d:\n", start_label);
            generate_exp(node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    je      .L%d\n", end_label);
            generate_statement(node->right, output);
            fprintf(output, "    jmp     .L%d\n", start_label);
            fprintf(output, ".L%d:\n", end_label);
            pop_loop();
            break;
        }
        case AST_FOR: {
            struct ast_node *init = node->left->left;
            struct ast_node *cond = node->left->right->left;
            struct ast_node *post = node->left->right->right;
            int start_label = label_count++;
            int post_label = label_count++;
            int end_label = label_count++;

            if (init) {
                if (init->type == AST_DECL) {
                    generate_statement(init, output);
                } else {
                    generate_exp(init, output);
                }
            }

            push_loop(end_label, post_label);
            fprintf(output, ".L%d:\n", start_label);
            if (cond) {
                generate_exp(cond, output);
                fprintf(output, "    cmpl    $0, %%eax\n");
                fprintf(output, "    je      .L%d\n", end_label);
            }
            generate_statement(node->right, output);
            fprintf(output, ".L%d:\n", post_label);
            if (post) {
                generate_exp(post, output);
            }
            fprintf(output, "    jmp     .L%d\n", start_label);
            fprintf(output, ".L%d:\n", end_label);
            pop_loop();
            break;
        }
        case AST_BREAK:
            if (loop_depth == 0) {
                fprintf(stderr, "break used outside of loop\n");
                exit(1);
            }
            fprintf(output, "    jmp     .L%d\n", loop_break_labels[loop_depth - 1]);
            break;
        case AST_CONTINUE:
            if (loop_depth == 0) {
                fprintf(stderr, "continue used outside of loop\n");
                exit(1);
            }
            fprintf(output, "    jmp     .L%d\n", loop_continue_labels[loop_depth - 1]);
            break;
        default:
            fprintf(stderr, "Unsupported statement node type: %d\n", node->type);
            exit(1);
    }
}

void generate_binop(struct ast_node *node, FILE *output)
{
    generate_exp(node->left, output);
    fprintf(output, "    push    %%eax\n");

    generate_exp(node->right, output);
    fprintf(output, "    pop     %%edx\n");

    switch (node->type) {
        case AST_ADD:
            fprintf(output, "    addl    %%edx, %%eax\n");
            break;
        case AST_SUB:
            fprintf(output, "    subl    %%eax, %%edx\n");
            fprintf(output, "    movl    %%edx, %%eax\n");
            break;
        case AST_MUL:
            fprintf(output, "    imull   %%edx, %%eax\n");
            break;
        case AST_DIV:
            fprintf(output, "    push    %%eax\n");
            fprintf(output, "    movl    %%edx, %%eax\n");
            fprintf(output, "    pop     %%ecx\n");
            fprintf(output, "    cdq\n");
            fprintf(output, "    idivl   %%ecx\n");
            break;
        case AST_MOD:
            fprintf(output, "    push    %%eax\n");
            fprintf(output, "    movl    %%edx, %%eax\n");
            fprintf(output, "    pop     %%ecx\n");
            fprintf(output, "    cdq\n");
            fprintf(output, "    idivl   %%ecx\n");
            fprintf(output, "    movl    %%edx, %%eax\n");
            break;
        case AST_BITWISE_AND:
            fprintf(output, "    andl    %%edx, %%eax\n");
            break;
        case AST_BITWISE_OR:
            fprintf(output, "    orl     %%edx, %%eax\n");
            break;
        case AST_BITWISE_XOR:
            fprintf(output, "    xorl    %%edx, %%eax\n");
            break;
        case AST_EQUAL:
            fprintf(output, "    cmpl    %%eax, %%edx\n");
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    sete    %%al\n");
            break;
        case AST_NOT_EQUAL:
            fprintf(output, "    cmpl    %%eax, %%edx\n");
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    setne   %%al\n");
            break;
        case AST_LESS:
            fprintf(output, "    cmpl    %%eax, %%edx\n");
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    setl    %%al\n");
            break;
        case AST_LESS_EQUAL:
            fprintf(output, "    cmpl    %%eax, %%edx\n");
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    setle   %%al\n");
            break;
        case AST_GREATER:
            fprintf(output, "    cmpl    %%eax, %%edx\n");
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    setg    %%al\n");
            break;
        case AST_GREATER_EQUAL:
            fprintf(output, "    cmpl    %%eax, %%edx\n");
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
            fprintf(output, "    movl    %d(%%ebp), %%eax\n", local_offset(node->value));
            break;
        case AST_CALL: {
            int arg_count = generate_call_args(node->left, output);
            fprintf(output, "    call    _%s\n", node->value);
            if (arg_count > 0) {
                fprintf(output, "    addl    $%d, %%esp\n", arg_count * 4);
            }
            break;
        }
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
            fprintf(output, "    movl    %%eax, %d(%%ebp)\n", local_offset(node->left->value));
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

int generate_call_args(struct ast_node *node, FILE *output)
{
    if (!node) {
        return 0;
    }

    if (node->type != AST_ARG_LIST) {
        fprintf(stderr, "Unsupported argument node type: %d\n", node->type);
        exit(1);
    }

    int count = generate_call_args(node->right, output);
    generate_exp(node->left, output);
    fprintf(output, "    push    %%eax\n");

    return count + 1;
}

char* generate(struct ast_node *ast)
{
    FILE *output = fopen("donkey_generate.tmp", "w+");
    if (output == NULL) {
        perror("Failed to create temporary file for assembly generation");
        exit(EXIT_FAILURE);
    }

    generate_program(ast, output);
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

    generate_program(ast, out_file);
    fclose(out_file);
}
