#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "decl.h"
#include "type.h"

static struct {
    char *name;
    int offset;
    int array_length;
    char *struct_name;
    struct Type *ty;
} symbols[256];
static struct {
    char *name;
    int array_length;
    struct Type *ty;
    int values[256];
} globals[256];
static int symbol_count = 0;
static int global_count = 0;
static int local_stack_count = 0;

/*
 * Emitter state: label numbering, the active function's exit label, the
 * break/continue label stacks, and interned string literals. The symbol,
 * struct, and global tables above stay file-scope for now -- they duplicate
 * work semantic analysis already did and are slated for removal once codegen
 * reads its symbols from the annotated AST instead.
 */
struct cg_string {
    char *value;
    int label;
};

struct cg_ctx {
    int label_count;
    int current_function_end_label;

    int *loop_break_labels;
    int *loop_continue_labels;
    int loop_depth;
    int loop_capacity;

    struct cg_string *strings;
    int string_count;
    int string_capacity;
};

static void cg_grow(void **items, int count, int *capacity, size_t item_size)
{
    void *grown;
    int next;

    if (count < *capacity) {
        return;
    }

    next = *capacity ? *capacity * 2 : 32;
    grown = realloc(*items, (size_t)next * item_size);
    if (!grown) {
        fprintf(stderr, "Out of memory in the code generator\n");
        exit(EXIT_FAILURE);
    }

    *items = grown;
    *capacity = next;
}

/* Mutually recursive generators, previously declared in decl.h. */
static void generate_program(struct cg_ctx *ctx, struct ast_node *node, FILE *output);
static void generate_function(struct cg_ctx *ctx, struct ast_node *node, FILE *output);
static void generate_statement(struct cg_ctx *ctx, struct ast_node *node, FILE *output);
static void generate_exp(struct cg_ctx *ctx, struct ast_node *node, FILE *output);
static int generate_call_args(struct cg_ctx *ctx, struct ast_node *node, FILE *output);

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

static void add_symbol(const char *name, int offset, struct Type *ty)
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
    symbols[symbol_count].array_length = 0;
    symbols[symbol_count].struct_name = NULL;
    symbols[symbol_count].ty = ty;
    symbol_count++;
}

static int struct_field_offset(struct Type *ty, const char *field_name)
{
    struct Member *member = ty_find_member(ty, field_name);
    return member ? member->offset : 0;
}

static int eval_const_exp(struct ast_node *node);

static struct ast_node *initializer_items(struct ast_node *node)
{
    if (!node || node->type != AST_INITIALIZER_LIST) {
        return NULL;
    }
    if (!node->left || node->left->type == AST_INITIALIZER_LIST) {
        return node->left;
    }
    return node;
}

static void add_global_node(struct ast_node *node)
{
    int i;
    struct ast_node *item;

    if (!node->value) {
        return;
    }

    if (find_global(node->value) >= 0) {
        fprintf(stderr, "Redeclaration of global variable '%s'\n", node->value);
        exit(1);
    }

    if (global_count >= 256) {
        fprintf(stderr, "Too many global variables\n");
        exit(1);
    }

    if (node->array_length > 256) {
        fprintf(stderr, "Global array '%s' exceeds the supported length of 256\n",
            node->value);
        exit(1);
    }

    globals[global_count].name = strdup(node->value);
    globals[global_count].array_length = node->array_length;
    for (i = 0; i < 256; i++) {
        globals[global_count].values[i] = 0;
    }
    if (node->array_length > 0) {
        i = 0;
        for (item = initializer_items(node->left); item && i < node->array_length; item = item->right) {
            globals[global_count].values[i++] = eval_const_exp(item->left);
        }
    } else {
        globals[global_count].values[0] = eval_const_exp(node->left);
    }
    global_count++;
}

static void add_param(struct ast_node *node, int index)
{
    add_symbol(node->value, 8 + (index * 4), node->ty);
}

static void add_local_node(struct ast_node *node)
{
    /* Stack slots stay 4 bytes wide; round the type's size up to fill them. */
    int size = node->ty ? node->ty->size : 4;
    int slots = (size + 3) / 4;

    if (slots < 1) {
        slots = 1;
    }

    local_stack_count += slots;
    add_symbol(node->value, -4 * local_stack_count, node->ty);
    symbols[symbol_count - 1].array_length = node->array_length;
    symbols[symbol_count - 1].struct_name = node->struct_name ? strdup(node->struct_name) : NULL;
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

    add_param(node->left, index);
    return collect_params(node->right, index + 1);
}

static void collect_locals(struct ast_node *node)
{
    if (!node) {
        return;
    }

    if (node->type == AST_DECL) {
        add_local_node(node);
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
        symbols[i].array_length = 0;
        free(symbols[i].struct_name);
        symbols[i].struct_name = NULL;
    }
    symbol_count = 0;
    local_stack_count = 0;
}

/*
 * Indirect access must match the element width. Using a 4-byte movl for a
 * char element would read past it and, on a store, clobber the three
 * neighbouring elements -- harmless while every type occupied its own 4-byte
 * slot, but wrong now that arrays are packed at their true element size.
 */
static void emit_load_indirect(struct Type *ty, FILE *output)
{
    int size = ty ? ty->size : 4;

    if (size == 1) {
        fprintf(output, "    %s  (%%eax), %%eax\n",
            ty && ty->is_unsigned ? "movzbl" : "movsbl");
    } else if (size == 2) {
        fprintf(output, "    %s  (%%eax), %%eax\n",
            ty && ty->is_unsigned ? "movzwl" : "movswl");
    } else {
        fprintf(output, "    movl    (%%eax), %%eax\n");
    }
}

/* Store %eax into a frame slot at the given offset, using the type's width. */
static void emit_store_offset(struct Type *ty, int offset, FILE *output)
{
    int size = ty ? ty->size : 4;

    if (size == 1) {
        fprintf(output, "    movb    %%al, %d(%%ebp)\n", offset);
    } else if (size == 2) {
        fprintf(output, "    movw    %%ax, %d(%%ebp)\n", offset);
    } else {
        fprintf(output, "    movl    %%eax, %d(%%ebp)\n", offset);
    }
}

static void emit_zero_offset(struct Type *ty, int offset, FILE *output)
{
    int size = ty ? ty->size : 4;

    if (size == 1) {
        fprintf(output, "    movb    $0, %d(%%ebp)\n", offset);
    } else if (size == 2) {
        fprintf(output, "    movw    $0, %d(%%ebp)\n", offset);
    } else {
        fprintf(output, "    movl    $0, %d(%%ebp)\n", offset);
    }
}

/* Address in %eax, value in %edx. */
static void emit_store_indirect(struct Type *ty, FILE *output)
{
    int size = ty ? ty->size : 4;

    if (size == 1) {
        fprintf(output, "    movb    %%dl, (%%eax)\n");
    } else if (size == 2) {
        fprintf(output, "    movw    %%dx, (%%eax)\n");
    } else {
        fprintf(output, "    movl    %%edx, (%%eax)\n");
    }
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
        if (symbols[local_index].array_length > 0) {
            fprintf(output, "    leal    %d(%%ebp), %%eax\n", symbols[local_index].offset);
            return;
        }
        fprintf(output, "    movl    %d(%%ebp), %%eax\n", symbols[local_index].offset);
        return;
    }

    if (find_global(name) >= 0) {
        int global_index = find_global(name);
        if (globals[global_index].array_length > 0) {
            fprintf(output, "    movl    $_%s, %%eax\n", name);
            return;
        }
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

static void generate_lvalue_address(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
{
    int local_index;

    switch (node->type) {
        case AST_IDENTIFIER:
            local_index = find_local(node->value);
            if (local_index >= 0) {
                fprintf(output, "    leal    %d(%%ebp), %%eax\n", symbols[local_index].offset);
                return;
            }
            if (find_global(node->value) >= 0) {
                fprintf(output, "    movl    $_%s, %%eax\n", node->value);
                return;
            }
            fprintf(stderr, "Use of undeclared identifier '%s'\n", node->value);
            exit(1);
        case AST_DEREFERENCE:
            generate_exp(ctx, node->left, output);
            return;
        case AST_ARRAY_SUBSCRIPT:
            if (node->left->array_length > 0) {
                generate_lvalue_address(ctx, node->left, output);
            } else {
                generate_exp(ctx, node->left, output);
            }
            fprintf(output, "    push    %%eax\n");
            generate_exp(ctx, node->right, output);
            fprintf(output, "    imull   $%d, %%eax\n",
                ty_element_size(node->left->ty));
            fprintf(output, "    pop     %%edx\n");
            fprintf(output, "    addl    %%edx, %%eax\n");
            return;
        case AST_FIELD_ACCESS:
            generate_lvalue_address(ctx, node->left, output);
            fprintf(output, "    addl    $%d, %%eax\n",
                struct_field_offset(node->left->ty, node->value));
            return;
        default:
            fprintf(stderr, "Expression is not assignable\n");
            exit(1);
    }
}

static void push_loop(struct cg_ctx *ctx, int break_label, int continue_label)
{
    int capacity_before = ctx->loop_capacity;

    cg_grow((void **)&ctx->loop_break_labels, ctx->loop_depth,
        &ctx->loop_capacity, sizeof(*ctx->loop_break_labels));
    /* Both stacks are kept the same size, so grow the second to match. */
    cg_grow((void **)&ctx->loop_continue_labels, ctx->loop_depth,
        &capacity_before, sizeof(*ctx->loop_continue_labels));

    ctx->loop_break_labels[ctx->loop_depth] = break_label;
    ctx->loop_continue_labels[ctx->loop_depth] = continue_label;
    ctx->loop_depth++;
}

static void pop_loop(struct cg_ctx *ctx)
{
    if (ctx->loop_depth > 0) {
        ctx->loop_depth--;
    }
}

/*
 * sizeof yields the operand's real size: sizeof(char[10]) is 10, not 4 and not
 * 40. A named type measures that type; an expression measures its resolved
 * type without being evaluated.
 */
static int sizeof_node(struct ast_node *node)
{
    if (node->left && node->left->ty) {
        return node->left->ty->size;
    }
    if (node->value) {
        return ty_from_name(node->value)->size;
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
            return sizeof_node(node);
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
        add_global_node(node);
    }
}

static int add_string_literal(struct cg_ctx *ctx, const char *value)
{
    for (int i = 0; i < ctx->string_count; i++) {
        if (strcmp(ctx->strings[i].value, value) == 0) {
            return ctx->strings[i].label;
        }
    }
    cg_grow((void **)&ctx->strings, ctx->string_count, &ctx->string_capacity,
        sizeof(*ctx->strings));

    ctx->strings[ctx->string_count].value = strdup(value);
    ctx->strings[ctx->string_count].label = ctx->string_count;
    ctx->string_count++;
    return ctx->string_count - 1;
}

static void collect_metadata(struct cg_ctx *ctx, struct ast_node *node)
{
    if (!node) return;
    /*
     * Struct definitions need no metadata here: field offsets, size, and
     * alignment come from the type semantic analysis attached to each node.
     */
    if (node->type == AST_STRUCT_DEF) {
        return;
    }
    if (node->type == AST_STRINGLIT) {
        node->string_label = add_string_literal(ctx, node->value);
    }
    collect_metadata(ctx, node->left);
    collect_metadata(ctx, node->right);
}

static void generate_globals(struct cg_ctx *ctx, FILE *output)
{
    if (global_count == 0 && ctx->string_count == 0) {
        return;
    }

    fprintf(output, ".data\n");
    for (int i = 0; i < global_count; i++) {
        fprintf(output, ".globl _%s\n", globals[i].name);
        fprintf(output, "_%s:\n", globals[i].name);
        int count = globals[i].array_length > 0 ? globals[i].array_length : 1;
        for (int j = 0; j < count; j++) {
            fprintf(output, "    .long   %d\n", globals[i].values[j]);
        }
    }
    for (int i = 0; i < ctx->string_count; i++) {
        fprintf(output, ".LC%d:\n", ctx->strings[i].label);
        fprintf(output, "    .byte   ");
        for (const unsigned char *p = (const unsigned char *)ctx->strings[i].value; *p; p++) {
            fprintf(output, "%u, ", *p);
        }
        fprintf(output, "0\n");
    }
    fprintf(output, ".text\n");
}

static void generate_function(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
{
    collect_params(node->left, 0);
    collect_locals(node->right);
    ctx->current_function_end_label = ctx->label_count++;

    fprintf(output, ".globl _%s\n", node->value);
    fprintf(output, "_%s:\n", node->value);
    fprintf(output, "    push    %%ebp\n");
    fprintf(output, "    movl    %%esp, %%ebp\n");
    if (local_stack_count > 0) {
        fprintf(output, "    subl    $%d, %%esp\n", local_stack_count * 4);
    }
    generate_statement(ctx, node->right, output);
    fprintf(output, "    movl    $0, %%eax\n");
    fprintf(output, ".L%d:\n", ctx->current_function_end_label);
    generate_epilogue(output);
    free_locals();
}

static void generate_program(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
{
    if (!node) {
        return;
    }

    switch (node->type) {
        case AST_PROGRAM:
            collect_metadata(ctx, node->left);
            collect_globals(node->left);
            generate_globals(ctx, output);
            generate_program(ctx, node->left, output);
            break;
        case AST_FUNCTION_LIST:
            generate_program(ctx, node->left, output);
            generate_program(ctx, node->right, output);
            break;
        case AST_FUNCTION:
            generate_function(ctx, node, output);
            break;
        case AST_GLOBAL_DECL:
            break;
        case AST_STRUCT_DEF:
            break;
        default:
            fprintf(stderr, "Unsupported program node type: %d\n", node->type);
            exit(1);
    }
}

static void generate_statement(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
{
    if (!node) {
        return;
    }

    switch (node->type) {
        case AST_BLOCK:
            generate_statement(ctx, node->left, output);
            break;
        case AST_STATEMENT_LIST:
            generate_statement(ctx, node->left, output);
            generate_statement(ctx, node->right, output);
            break;
        case AST_DECL:
            if (node->array_length > 0) {
                int offset = local_offset(node->value);
                /*
                 * Step by the element's real size. Striding by 4 through a
                 * char array would run past its end -- straight over the saved
                 * frame pointer and return address.
                 */
                int stride = ty_element_size(node->ty);
                int index = 0;
                struct ast_node *item;

                for (item = initializer_items(node->left); item; item = item->right) {
                    generate_exp(ctx, item->left, output);
                    emit_store_offset(node->ty ? node->ty->base : NULL,
                        offset + (index * stride), output);
                    index++;
                }
                while (index < node->array_length) {
                    emit_zero_offset(node->ty ? node->ty->base : NULL,
                        offset + (index * stride), output);
                    index++;
                }
            } else if (node->left) {
                generate_exp(ctx, node->left, output);
                fprintf(output, "    movl    %%eax, %d(%%ebp)\n", local_offset(node->value));
            } else {
                fprintf(output, "    movl    $0, %d(%%ebp)\n", local_offset(node->value));
            }
            break;
        case AST_EXPR_STMT:
            generate_exp(ctx, node->left, output);
            break;
        case AST_RETURN:
            generate_exp(ctx, node->left, output);
            fprintf(output, "    jmp     .L%d\n", ctx->current_function_end_label);
            break;
        case AST_IF: {
            int else_label = ctx->label_count++;
            int end_label = ctx->label_count++;

            generate_exp(ctx, node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    je      .L%d\n", else_label);
            generate_statement(ctx, node->right->left, output);
            fprintf(output, "    jmp     .L%d\n", end_label);
            fprintf(output, ".L%d:\n", else_label);
            if (node->right->right) {
                generate_statement(ctx, node->right->right, output);
            }
            fprintf(output, ".L%d:\n", end_label);
            break;
        }
        case AST_WHILE: {
            int start_label = ctx->label_count++;
            int end_label = ctx->label_count++;

            push_loop(ctx, end_label, start_label);
            fprintf(output, ".L%d:\n", start_label);
            generate_exp(ctx, node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    je      .L%d\n", end_label);
            generate_statement(ctx, node->right, output);
            fprintf(output, "    jmp     .L%d\n", start_label);
            fprintf(output, ".L%d:\n", end_label);
            pop_loop(ctx);
            break;
        }
        case AST_FOR: {
            struct ast_node *init = node->left->left;
            struct ast_node *cond = node->left->right->left;
            struct ast_node *post = node->left->right->right;
            int start_label = ctx->label_count++;
            int post_label = ctx->label_count++;
            int end_label = ctx->label_count++;

            if (init) {
                if (init->type == AST_DECL) {
                    generate_statement(ctx, init, output);
                } else {
                    generate_exp(ctx, init, output);
                }
            }

            push_loop(ctx, end_label, post_label);
            fprintf(output, ".L%d:\n", start_label);
            if (cond) {
                generate_exp(ctx, cond, output);
                fprintf(output, "    cmpl    $0, %%eax\n");
                fprintf(output, "    je      .L%d\n", end_label);
            }
            generate_statement(ctx, node->right, output);
            fprintf(output, ".L%d:\n", post_label);
            if (post) {
                generate_exp(ctx, post, output);
            }
            fprintf(output, "    jmp     .L%d\n", start_label);
            fprintf(output, ".L%d:\n", end_label);
            pop_loop(ctx);
            break;
        }
        case AST_BREAK:
            if (ctx->loop_depth == 0) {
                fprintf(stderr, "break used outside of loop\n");
                exit(1);
            }
            fprintf(output, "    jmp     .L%d\n", ctx->loop_break_labels[ctx->loop_depth - 1]);
            break;
        case AST_CONTINUE:
            if (ctx->loop_depth == 0) {
                fprintf(stderr, "continue used outside of loop\n");
                exit(1);
            }
            fprintf(output, "    jmp     .L%d\n", ctx->loop_continue_labels[ctx->loop_depth - 1]);
            break;
        default:
            fprintf(stderr, "Unsupported statement node type: %d\n", node->type);
            exit(1);
    }
}

void generate_binop(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
{
    int is_unsigned = is_unsigned_type(node->left->data_type);
    int left_is_pointer = node->left->pointer_depth > 0 || node->left->array_length > 0;
    int right_is_pointer = node->right->pointer_depth > 0 || node->right->array_length > 0;

    if ((node->type == AST_ADD || node->type == AST_SUB) &&
        (left_is_pointer || right_is_pointer)) {
        generate_exp(ctx, node->left, output);
        fprintf(output, "    push    %%eax\n");
        generate_exp(ctx, node->right, output);
        fprintf(output, "    pop     %%edx\n");

        if (left_is_pointer && right_is_pointer && node->type == AST_SUB) {
            fprintf(output, "    subl    %%eax, %%edx\n");
            fprintf(output, "    movl    %%edx, %%eax\n");
            fprintf(output, "    cdq\n");
            fprintf(output, "    movl    $%d, %%ecx\n",
                ty_element_size(node->left->ty));
            fprintf(output, "    idivl   %%ecx\n");
            return;
        }
        if (left_is_pointer && !right_is_pointer) {
            fprintf(output, "    imull   $%d, %%eax\n",
                ty_element_size(node->left->ty));
            if (node->type == AST_ADD) {
                fprintf(output, "    addl    %%edx, %%eax\n");
            } else {
                fprintf(output, "    subl    %%eax, %%edx\n");
                fprintf(output, "    movl    %%edx, %%eax\n");
            }
            return;
        }
        if (right_is_pointer && !left_is_pointer && node->type == AST_ADD) {
            fprintf(output, "    imull   $%d, %%edx\n",
                ty_element_size(node->right->ty));
            fprintf(output, "    addl    %%edx, %%eax\n");
            return;
        }
    }

    generate_exp(ctx, node->left, output);
    fprintf(output, "    push    %%eax\n");

    generate_exp(ctx, node->right, output);
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

static void generate_exp(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
{
    switch (node->type) {
        case AST_INTLIT:
            fprintf(output, "    movl    $%s, %%eax\n", node->value);
            break;
        case AST_STRINGLIT:
            fprintf(output, "    movl    $.LC%d, %%eax\n", node->string_label);
            break;
        case AST_IDENTIFIER:
            generate_identifier_load(node->value, output);
            break;
        case AST_CALL: {
            int arg_count = generate_call_args(ctx, node->left, output);
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
            generate_binop(ctx, node, output);
            break;
        case AST_CONDITIONAL: {
            int else_label = ctx->label_count++;
            int end_label = ctx->label_count++;

            generate_exp(ctx, node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    je      .L%d\n", else_label);
            generate_exp(ctx, node->right->left, output);
            fprintf(output, "    jmp     .L%d\n", end_label);
            fprintf(output, ".L%d:\n", else_label);
            generate_exp(ctx, node->right->right, output);
            fprintf(output, ".L%d:\n", end_label);
            break;
        }
        case AST_COMMA:
            generate_exp(ctx, node->left, output);
            generate_exp(ctx, node->right, output);
            break;
        case AST_LOGICAL_AND: {
            int false_label = ctx->label_count++;
            int end_label = ctx->label_count++;

            generate_exp(ctx, node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    je      .L%d\n", false_label);
            generate_exp(ctx, node->right, output);
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
            int true_label = ctx->label_count++;
            int end_label = ctx->label_count++;

            generate_exp(ctx, node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    jne     .L%d\n", true_label);
            generate_exp(ctx, node->right, output);
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
            generate_exp(ctx, node->right, output);
            fprintf(output, "    push    %%eax\n");
            generate_lvalue_address(ctx, node->left, output);
            fprintf(output, "    pop     %%edx\n");
            emit_store_indirect(node->left->ty, output);
            fprintf(output, "    movl    %%edx, %%eax\n");
            break;
        case AST_ADDRESS_OF:
            generate_lvalue_address(ctx, node->left, output);
            break;
        case AST_DEREFERENCE:
            generate_exp(ctx, node->left, output);
            emit_load_indirect(node->ty, output);
            break;
        case AST_ARRAY_SUBSCRIPT:
            generate_lvalue_address(ctx, node, output);
            emit_load_indirect(node->ty, output);
            break;
        case AST_FIELD_ACCESS:
            generate_lvalue_address(ctx, node, output);
            emit_load_indirect(node->ty, output);
            break;
        case AST_PRE_INCREMENT:
            generate_identifier_load(node->left->value, output);
            fprintf(output, "    addl    $%d, %%eax\n", node->pointer_depth > 0 ? 4 : 1);
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left->value, output);
            break;
        case AST_PRE_DECREMENT:
            generate_identifier_load(node->left->value, output);
            fprintf(output, "    subl    $%d, %%eax\n", node->pointer_depth > 0 ? 4 : 1);
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left->value, output);
            break;
        case AST_POST_INCREMENT:
            generate_identifier_load(node->left->value, output);
            fprintf(output, "    push    %%eax\n");
            fprintf(output, "    addl    $%d, %%eax\n", node->pointer_depth > 0 ? 4 : 1);
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left->value, output);
            fprintf(output, "    pop     %%eax\n");
            break;
        case AST_POST_DECREMENT:
            generate_identifier_load(node->left->value, output);
            fprintf(output, "    push    %%eax\n");
            fprintf(output, "    subl    $%d, %%eax\n", node->pointer_depth > 0 ? 4 : 1);
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left->value, output);
            fprintf(output, "    pop     %%eax\n");
            break;
        case AST_SIZEOF:
            fprintf(output, "    movl    $%d, %%eax\n", sizeof_node(node));
            break;
        case AST_CAST:
            generate_exp(ctx, node->left, output);
            generate_cast(node->value, output);
            break;
        case AST_NEGATION:
            generate_exp(ctx, node->left, output);
            fprintf(output, "    negl    %%eax\n");
            break;
        case AST_BITWISE_COMPLEMENT:
            generate_exp(ctx, node->left, output);
            fprintf(output, "    notl    %%eax\n");
            break;
        case AST_LOGICAL_NEGATION:
            generate_exp(ctx, node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    sete    %%al\n");
            break;
        default:
            fprintf(stderr, "Unsupported AST node type: %d\n", node->type);
            exit(1);
    }
}

static int generate_call_args(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
{
    if (!node) {
        return 0;
    }

    if (node->type != AST_ARG_LIST) {
        fprintf(stderr, "Unsupported argument node type: %d\n", node->type);
        exit(1);
    }

    int count = generate_call_args(ctx, node->right, output);
    generate_exp(ctx, node->left, output);
    fprintf(output, "    push    %%eax\n");

    return count + 1;
}


void write_assembly_to_file(const char *filename, struct ast_node *ast)
{
    FILE *out_file = fopen(filename, "w");
    if (!out_file) {
        perror("Failed to open file for writing");
        exit(EXIT_FAILURE);
    }

    struct cg_ctx ctx_storage;
    struct cg_ctx *ctx = &ctx_storage;

    memset(ctx, 0, sizeof(*ctx));

    generate_program(ctx, ast, out_file);
    fclose(out_file);

    for (int i = 0; i < ctx->string_count; i++) {
        free(ctx->strings[i].value);
    }
    free(ctx->strings);
    free(ctx->loop_break_labels);
    free(ctx->loop_continue_labels);
}
