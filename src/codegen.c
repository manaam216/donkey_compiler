#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "decl.h"

static struct {
    char *name;
    int offset;
} symbols[256];
static struct {
    char *name;
    int value;
} globals[256];
static int symbol_count = 0;
static int global_count = 0;
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

static int find_global(const char *name)
{
    for (int i = 0; i < global_count; i++) {
        if (strcmp(globals[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
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

static int eval_const_exp(struct ast_node *node);

static void add_global(const char *name, int value)
{
    if (!name) {
        return;
    }

    if (find_global(name) >= 0) {
        fprintf(stderr, "Redeclaration of global variable '%s'\n", name);
        exit(1);
    }

    if (global_count >= 256) {
        fprintf(stderr, "Too many global variables\n");
        exit(1);
    }

    globals[global_count].name = strdup(name);
    globals[global_count].value = value;
    global_count++;
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

static void generate_identifier_load(const char *name, FILE *output)
{
    int local_index = find_local(name);
    if (local_index >= 0) {
        fprintf(output, "    movl    %d(%%ebp), %%eax\n", symbols[local_index].offset);
        return;
    }

    if (find_global(name) >= 0) {
        fprintf(output, "    movl    _%s, %%eax\n", name);
        return;
    }

    fprintf(stderr, "Use of undeclared identifier '%s'\n", name);
    exit(1);
}

static void generate_identifier_store(const char *name, FILE *output)
{
    int local_index = find_local(name);
    if (local_index >= 0) {
        fprintf(output, "    movl    %%eax, %d(%%ebp)\n", symbols[local_index].offset);
        return;
    }

    if (find_global(name) >= 0) {
        fprintf(output, "    movl    %%eax, _%s\n", name);
        return;
    }

    fprintf(stderr, "Use of undeclared identifier '%s'\n", name);
    exit(1);
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

static int type_size(const char *type)
{
    if (!type) {
        return 4;
    }
    if (strcmp(type, "char") == 0 || strcmp(type, "uchar") == 0) {
        return 1;
    }
    if (strcmp(type, "short") == 0 || strcmp(type, "ushort") == 0) {
        return 2;
    }
    return 4;
}

static void generate_cast(const char *type, FILE *output)
{
    if (!type) {
        return;
    }
    if (strcmp(type, "char") == 0) {
        fprintf(output, "    movsbl  %%al, %%eax\n");
    } else if (strcmp(type, "uchar") == 0) {
        fprintf(output, "    movzbl  %%al, %%eax\n");
    } else if (strcmp(type, "short") == 0) {
        fprintf(output, "    movswl  %%ax, %%eax\n");
    } else if (strcmp(type, "ushort") == 0) {
        fprintf(output, "    movzwl  %%ax, %%eax\n");
    }
}

static const char *codegen_type_name(CType type)
{
    switch (type) {
        case TYPE_CHAR: return "char";
        case TYPE_UCHAR: return "uchar";
        case TYPE_SHORT: return "short";
        case TYPE_USHORT: return "ushort";
        case TYPE_UINT: return "uint";
        case TYPE_LONG: return "long";
        case TYPE_ULONG: return "ulong";
        default: return "int";
    }
}

static int is_unsigned_type(CType type)
{
    return type == TYPE_UCHAR || type == TYPE_USHORT ||
        type == TYPE_UINT || type == TYPE_ULONG;
}

static int cast_constant(int value, const char *type)
{
    if (strcmp(type, "char") == 0) return (int)(int8_t)value;
    if (strcmp(type, "uchar") == 0) return (int)(uint8_t)value;
    if (strcmp(type, "short") == 0) return (int)(int16_t)value;
    if (strcmp(type, "ushort") == 0) return (int)(uint16_t)value;
    return value;
}

static int eval_const_exp(struct ast_node *node)
{
    if (!node) {
        return 0;
    }

    switch (node->type) {
        case AST_INTLIT:
            return atoi(node->value);
        case AST_NEGATION:
            return -eval_const_exp(node->left);
        case AST_BITWISE_COMPLEMENT:
            return ~eval_const_exp(node->left);
        case AST_LOGICAL_NEGATION:
            return !eval_const_exp(node->left);
        case AST_ADD:
            return eval_const_exp(node->left) + eval_const_exp(node->right);
        case AST_SUB:
            return eval_const_exp(node->left) - eval_const_exp(node->right);
        case AST_MUL:
            return eval_const_exp(node->left) * eval_const_exp(node->right);
        case AST_DIV:
            if (is_unsigned_type(node->left->data_type))
                return (int)((uint32_t)eval_const_exp(node->left) /
                    (uint32_t)eval_const_exp(node->right));
            return eval_const_exp(node->left) / eval_const_exp(node->right);
        case AST_MOD:
            if (is_unsigned_type(node->left->data_type))
                return (int)((uint32_t)eval_const_exp(node->left) %
                    (uint32_t)eval_const_exp(node->right));
            return eval_const_exp(node->left) % eval_const_exp(node->right);
        case AST_SHIFT_LEFT:
            return eval_const_exp(node->left) << eval_const_exp(node->right);
        case AST_SHIFT_RIGHT:
            if (is_unsigned_type(node->left->data_type))
                return (int)((uint32_t)eval_const_exp(node->left) >> eval_const_exp(node->right));
            return eval_const_exp(node->left) >> eval_const_exp(node->right);
        case AST_BITWISE_AND:
            return eval_const_exp(node->left) & eval_const_exp(node->right);
        case AST_BITWISE_OR:
            return eval_const_exp(node->left) | eval_const_exp(node->right);
        case AST_BITWISE_XOR:
            return eval_const_exp(node->left) ^ eval_const_exp(node->right);
        case AST_LOGICAL_AND:
            return eval_const_exp(node->left) && eval_const_exp(node->right);
        case AST_LOGICAL_OR:
            return eval_const_exp(node->left) || eval_const_exp(node->right);
        case AST_EQUAL:
            return eval_const_exp(node->left) == eval_const_exp(node->right);
        case AST_NOT_EQUAL:
            return eval_const_exp(node->left) != eval_const_exp(node->right);
        case AST_LESS:
            if (is_unsigned_type(node->left->data_type))
                return (uint32_t)eval_const_exp(node->left) < (uint32_t)eval_const_exp(node->right);
            return eval_const_exp(node->left) < eval_const_exp(node->right);
        case AST_LESS_EQUAL:
            if (is_unsigned_type(node->left->data_type))
                return (uint32_t)eval_const_exp(node->left) <= (uint32_t)eval_const_exp(node->right);
            return eval_const_exp(node->left) <= eval_const_exp(node->right);
        case AST_GREATER:
            if (is_unsigned_type(node->left->data_type))
                return (uint32_t)eval_const_exp(node->left) > (uint32_t)eval_const_exp(node->right);
            return eval_const_exp(node->left) > eval_const_exp(node->right);
        case AST_GREATER_EQUAL:
            if (is_unsigned_type(node->left->data_type))
                return (uint32_t)eval_const_exp(node->left) >= (uint32_t)eval_const_exp(node->right);
            return eval_const_exp(node->left) >= eval_const_exp(node->right);
        case AST_CONDITIONAL:
            return eval_const_exp(node->left) ? eval_const_exp(node->right->left) : eval_const_exp(node->right->right);
        case AST_COMMA:
            return eval_const_exp(node->right);
        case AST_SIZEOF:
            return type_size(node->value);
        case AST_CAST:
            return cast_constant(eval_const_exp(node->left), node->value);
        default:
            fprintf(stderr, "Global initializer must be a constant expression\n");
            exit(1);
    }
}

static void collect_globals(struct ast_node *node)
{
    if (!node) {
        return;
    }

    if (node->type == AST_FUNCTION_LIST) {
        collect_globals(node->left);
        collect_globals(node->right);
    } else if (node->type == AST_GLOBAL_DECL) {
        add_global(node->value, eval_const_exp(node->left));
    }
}

static void generate_globals(FILE *output)
{
    if (global_count == 0) {
        return;
    }

    fprintf(output, ".data\n");
    for (int i = 0; i < global_count; i++) {
        fprintf(output, ".globl _%s\n", globals[i].name);
        fprintf(output, "_%s:\n", globals[i].name);
        fprintf(output, "    .long   %d\n", globals[i].value);
    }
    fprintf(output, ".text\n");
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
            collect_globals(node->left);
            generate_globals(output);
            generate_program(node->left, output);
            break;
        case AST_FUNCTION_LIST:
            generate_program(node->left, output);
            generate_program(node->right, output);
            break;
        case AST_FUNCTION:
            generate_function(node, output);
            break;
        case AST_GLOBAL_DECL:
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
    int is_unsigned = is_unsigned_type(node->left->data_type);

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
            if (is_unsigned) {
                fprintf(output, "    xorl    %%edx, %%edx\n");
                fprintf(output, "    divl    %%ecx\n");
            } else {
                fprintf(output, "    cdq\n");
                fprintf(output, "    idivl   %%ecx\n");
            }
            break;
        case AST_MOD:
            fprintf(output, "    push    %%eax\n");
            fprintf(output, "    movl    %%edx, %%eax\n");
            fprintf(output, "    pop     %%ecx\n");
            if (is_unsigned) {
                fprintf(output, "    xorl    %%edx, %%edx\n");
                fprintf(output, "    divl    %%ecx\n");
            } else {
                fprintf(output, "    cdq\n");
                fprintf(output, "    idivl   %%ecx\n");
            }
            fprintf(output, "    movl    %%edx, %%eax\n");
            break;
        case AST_SHIFT_LEFT:
            fprintf(output, "    movl    %%eax, %%ecx\n");
            fprintf(output, "    movl    %%edx, %%eax\n");
            fprintf(output, "    sall    %%cl, %%eax\n");
            break;
        case AST_SHIFT_RIGHT:
            fprintf(output, "    movl    %%eax, %%ecx\n");
            fprintf(output, "    movl    %%edx, %%eax\n");
            fprintf(output, is_unsigned ? "    shrl    %%cl, %%eax\n" : "    sarl    %%cl, %%eax\n");
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
            fprintf(output, is_unsigned ? "    setb    %%al\n" : "    setl    %%al\n");
            break;
        case AST_LESS_EQUAL:
            fprintf(output, "    cmpl    %%eax, %%edx\n");
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, is_unsigned ? "    setbe   %%al\n" : "    setle   %%al\n");
            break;
        case AST_GREATER:
            fprintf(output, "    cmpl    %%eax, %%edx\n");
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, is_unsigned ? "    seta    %%al\n" : "    setg    %%al\n");
            break;
        case AST_GREATER_EQUAL:
            fprintf(output, "    cmpl    %%eax, %%edx\n");
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, is_unsigned ? "    setae   %%al\n" : "    setge   %%al\n");
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
            generate_identifier_load(node->value, output);
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
        case AST_SHIFT_LEFT:
        case AST_SHIFT_RIGHT:
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
        case AST_CONDITIONAL: {
            int else_label = label_count++;
            int end_label = label_count++;

            generate_exp(node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    je      .L%d\n", else_label);
            generate_exp(node->right->left, output);
            fprintf(output, "    jmp     .L%d\n", end_label);
            fprintf(output, ".L%d:\n", else_label);
            generate_exp(node->right->right, output);
            fprintf(output, ".L%d:\n", end_label);
            break;
        }
        case AST_COMMA:
            generate_exp(node->left, output);
            generate_exp(node->right, output);
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
            generate_identifier_store(node->left->value, output);
            break;
        case AST_PRE_INCREMENT:
            generate_identifier_load(node->left->value, output);
            fprintf(output, "    addl    $1, %%eax\n");
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left->value, output);
            break;
        case AST_PRE_DECREMENT:
            generate_identifier_load(node->left->value, output);
            fprintf(output, "    subl    $1, %%eax\n");
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left->value, output);
            break;
        case AST_POST_INCREMENT:
            generate_identifier_load(node->left->value, output);
            fprintf(output, "    push    %%eax\n");
            fprintf(output, "    addl    $1, %%eax\n");
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left->value, output);
            fprintf(output, "    pop     %%eax\n");
            break;
        case AST_POST_DECREMENT:
            generate_identifier_load(node->left->value, output);
            fprintf(output, "    push    %%eax\n");
            fprintf(output, "    subl    $1, %%eax\n");
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left->value, output);
            fprintf(output, "    pop     %%eax\n");
            break;
        case AST_SIZEOF:
            fprintf(output, "    movl    $%d, %%eax\n", type_size(node->value));
            break;
        case AST_CAST:
            generate_exp(node->left, output);
            generate_cast(node->value, output);
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
