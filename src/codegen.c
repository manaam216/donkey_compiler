#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "decl.h"
#include "type.h"
#include "symbol.h"
#include "diag.h"

static struct {
    char *name;
    int array_length;
    struct Type *ty;
    int values[256];
} globals[256];
static int global_count = 0;

/*
 * Emitter state: label numbering, the active function's exit label, the
 * break/continue label stacks, and interned string literals.
 *
 * Codegen no longer keeps a symbol table. Storage locations, types, and frame
 * sizes are read from the symbols semantic analysis attached to the AST. The
 * globals array above remains only as the list of static data to emit.
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
static int count_args(struct ast_node *node);

static int find_global(const char *name)
{
    for (int i = 0; i < global_count; i++) {
        if (strcmp(globals[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
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
        diag_at(DIAG_ERROR, node->location,
            "redeclaration of global variable '%s'", node->value);
        return;
    }

    if (global_count >= 256) {
        diag_at(DIAG_ERROR, node->location,
            "too many global variables (limit is 256)");
        return;
    }

    if (node->array_length > 256) {
        diag_at(DIAG_ERROR, node->location,
            "global array '%s' exceeds the supported length of 256", node->value);
        return;
    }

    globals[global_count].name = strdup(node->value);
    globals[global_count].array_length = node->array_length;
    globals[global_count].ty = node->ty;
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

/*
 * Access width follows the type. A 4-byte movl for a char element would read
 * past it and, on a store, clobber its neighbours; an 8-byte pointer or long
 * needs movq. Values live in %rax, so 4-byte and narrower loads name %eax and
 * let the hardware zero the upper half.
 */
static void emit_load_indirect(struct Type *ty, FILE *output)
{
    int size = ty ? ty->size : 4;

    if (size == 1) {
        fprintf(output, "    %s  (%%rax), %%eax\n",
            ty && ty->is_unsigned ? "movzbl" : "movsbl");
    } else if (size == 2) {
        fprintf(output, "    %s  (%%rax), %%eax\n",
            ty && ty->is_unsigned ? "movzwl" : "movswl");
    } else if (size == 8) {
        fprintf(output, "    movq    (%%rax), %%rax\n");
    } else {
        fprintf(output, "    movl    (%%rax), %%eax\n");
    }
}

/* Store the accumulator into a frame slot, using the type's width. */
static void emit_store_offset(struct Type *ty, int offset, FILE *output)
{
    int size = ty ? ty->size : 4;

    if (size == 1) {
        fprintf(output, "    movb    %%al, %d(%%rbp)\n", offset);
    } else if (size == 2) {
        fprintf(output, "    movw    %%ax, %d(%%rbp)\n", offset);
    } else if (size == 8) {
        fprintf(output, "    movq    %%rax, %d(%%rbp)\n", offset);
    } else {
        fprintf(output, "    movl    %%eax, %d(%%rbp)\n", offset);
    }
}

static void emit_zero_offset(struct Type *ty, int offset, FILE *output)
{
    int size = ty ? ty->size : 4;

    if (size == 1) {
        fprintf(output, "    movb    $0, %d(%%rbp)\n", offset);
    } else if (size == 2) {
        fprintf(output, "    movw    $0, %d(%%rbp)\n", offset);
    } else if (size == 8) {
        fprintf(output, "    movq    $0, %d(%%rbp)\n", offset);
    } else {
        fprintf(output, "    movl    $0, %d(%%rbp)\n", offset);
    }
}

/* Address in %rax, value in %rdx. */
static void emit_store_indirect(struct Type *ty, FILE *output)
{
    int size = ty ? ty->size : 4;

    if (size == 1) {
        fprintf(output, "    movb    %%dl, (%%rax)\n");
    } else if (size == 2) {
        fprintf(output, "    movw    %%dx, (%%rax)\n");
    } else if (size == 8) {
        fprintf(output, "    movq    %%rdx, (%%rax)\n");
    } else {
        fprintf(output, "    movl    %%edx, (%%rax)\n");
    }
}

/*
 * A scalar local owns a whole slot -- at least a word, eight bytes for a
 * pointer or long -- and is read back at that width, so writes must fill it.
 * The narrow stores above are only for packed array elements.
 */
static void emit_store_slot(struct Type *ty, int offset, FILE *output)
{
    if (ty && ty->size == 8) {
        fprintf(output, "    movq    %%rax, %d(%%rbp)\n", offset);
    } else {
        fprintf(output, "    movl    %%eax, %d(%%rbp)\n", offset);
    }
}

static void emit_zero_slot(struct Type *ty, int offset, FILE *output)
{
    if (ty && ty->size == 8) {
        fprintf(output, "    movq    $0, %d(%%rbp)\n", offset);
    } else {
        fprintf(output, "    movl    $0, %d(%%rbp)\n", offset);
    }
}

/* Load a frame slot into the accumulator at the type's width. */
static void emit_load_frame(struct Type *ty, int offset, FILE *output)
{
    int size = ty ? ty->size : 4;

    if (size == 1) {
        fprintf(output, "    %s  %d(%%rbp), %%eax\n",
            ty && ty->is_unsigned ? "movzbl" : "movsbl", offset);
    } else if (size == 2) {
        fprintf(output, "    %s  %d(%%rbp), %%eax\n",
            ty && ty->is_unsigned ? "movzwl" : "movswl", offset);
    } else if (size == 8) {
        fprintf(output, "    movq    %d(%%rbp), %%rax\n", offset);
    } else {
        fprintf(output, "    movl    %d(%%rbp), %%eax\n", offset);
    }
}

static void emit_load_global(struct Type *ty, const char *name, FILE *output)
{
    int size = ty ? ty->size : 4;

    if (size == 1) {
        fprintf(output, "    %s  %s(%%rip), %%eax\n",
            ty && ty->is_unsigned ? "movzbl" : "movsbl", name);
    } else if (size == 2) {
        fprintf(output, "    %s  %s(%%rip), %%eax\n",
            ty && ty->is_unsigned ? "movzwl" : "movswl", name);
    } else if (size == 8) {
        fprintf(output, "    movq    %s(%%rip), %%rax\n", name);
    } else {
        fprintf(output, "    movl    %s(%%rip), %%eax\n", name);
    }
}

static void emit_store_global(struct Type *ty, const char *name, FILE *output)
{
    int size = ty ? ty->size : 4;

    if (size == 1) {
        fprintf(output, "    movb    %%al, %s(%%rip)\n", name);
    } else if (size == 2) {
        fprintf(output, "    movw    %%ax, %s(%%rip)\n", name);
    } else if (size == 8) {
        fprintf(output, "    movq    %%rax, %s(%%rip)\n", name);
    } else {
        fprintf(output, "    movl    %%eax, %s(%%rip)\n", name);
    }
}

/*
 * ++ and -- step a pointer by one element and everything else by one. On a
 * pointer the step is 64-bit address arithmetic, and the element size comes
 * from the type rather than being assumed to be a word.
 */
static void emit_step(struct Type *ty, int is_increment, FILE *output)
{
    int pointer = ty && ty->kind == TY_PTR;
    int step = pointer ? ty_element_size(ty) : 1;

    if (pointer) {
        fprintf(output, "    %s    $%d, %%rax\n", is_increment ? "addq" : "subq", step);
    } else {
        fprintf(output, "    %s    $%d, %%eax\n", is_increment ? "addl" : "subl", step);
    }
}

/* Compare the two operands at the wider of their widths. */
static void emit_compare(struct Type *left, struct Type *right, FILE *output)
{
    int size = 4;

    if (left && left->size > size) size = left->size;
    if (right && right->size > size) size = right->size;

    fprintf(output, size == 8 ? "    cmpq    %%rax, %%rdx\n"
                              : "    cmpl    %%eax, %%edx\n");
}

static void generate_epilogue(FILE *output)
{
    fprintf(output, "    leave\n");
    fprintf(output, "    ret\n");
}

/*
 * Storage comes straight off the symbol semantic analysis attached to the
 * node. There is no name lookup here any more, so an unresolved name cannot
 * reach the code generator: it is prevented by construction rather than by a
 * runtime check.
 */
static int is_frame_symbol(struct Symbol *sym)
{
    return sym->kind == SYM_LOCAL || sym->kind == SYM_PARAM;
}

static void generate_identifier_load(struct ast_node *node, FILE *output)
{
    struct Symbol *sym = node->sym;
    int is_array = sym->ty && sym->ty->kind == TY_ARRAY;

    if (is_frame_symbol(sym)) {
        /* An array's value is its address; anything else is loaded. */
        if (is_array) {
            fprintf(output, "    leaq    %d(%%rbp), %%rax\n", sym->offset);
        } else {
            emit_load_frame(sym->ty, sym->offset, output);
        }
        return;
    }

    if (is_array) {
        fprintf(output, "    leaq    %s(%%rip), %%rax\n", sym->name);
    } else {
        emit_load_global(sym->ty, sym->name, output);
    }
}

static void generate_identifier_store(struct ast_node *node, FILE *output)
{
    struct Symbol *sym = node->sym;

    if (is_frame_symbol(sym)) {
        emit_store_slot(sym->ty, sym->offset, output);
        return;
    }

    emit_store_global(sym->ty, sym->name, output);
}

static void generate_lvalue_address(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
{
    switch (node->type) {
        case AST_IDENTIFIER:
            if (is_frame_symbol(node->sym)) {
                fprintf(output, "    leaq    %d(%%rbp), %%rax\n", node->sym->offset);
            } else {
                fprintf(output, "    leaq    %s(%%rip), %%rax\n", node->sym->name);
            }
            return;
        case AST_DEREFERENCE:
            generate_exp(ctx, node->left, output);
            return;
        case AST_ARRAY_SUBSCRIPT:
            if (node->left->array_length > 0) {
                generate_lvalue_address(ctx, node->left, output);
            } else {
                generate_exp(ctx, node->left, output);
            }
            fprintf(output, "    pushq   %%rax\n");
            generate_exp(ctx, node->right, output);
            /*
             * Widen the index before scaling: addresses are 64-bit, and a
             * 32-bit add would truncate a stack address to garbage.
             */
            fprintf(output, "    cltq\n");
            fprintf(output, "    imulq   $%d, %%rax\n",
                ty_element_size(node->left->ty));
            fprintf(output, "    popq    %%rdx\n");
            fprintf(output, "    addq    %%rdx, %%rax\n");
            return;
        case AST_FIELD_ACCESS:
            generate_lvalue_address(ctx, node->left, output);
            /* Adding to an address: 64-bit, or the pointer is truncated. */
            fprintf(output, "    addq    $%d, %%rax\n",
                struct_field_offset(node->left->ty, node->value));
            return;
        default:
            diag_internal(node->location, "expression is not assignable");
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
    } else if (strcmp(type, "long") == 0) {
        /*
         * Widening to 8 bytes: values are computed in %eax, so the upper half
         * of %rax is undefined until it is extended explicitly.
         */
        fprintf(output, "    cltq\n");
    } else if (strcmp(type, "ulong") == 0) {
        /* Writing %eax zeroes the upper half of %rax. */
        fprintf(output, "    movl    %%eax, %%eax\n");
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
            diag_internal(node->location,
                "global initializer is not a constant expression");
            return 0;   /* not reached: diag_internal exits */
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
        /* Element width follows the type, so a char array is bytes, not words. */
        struct Type *ty = globals[i].ty;
        struct Type *element = ty && ty->kind == TY_ARRAY ? ty->base : ty;
        int element_size = element ? element->size : 4;
        int count = globals[i].array_length > 0 ? globals[i].array_length : 1;
        int alignment = element ? element->align : 4;

        fprintf(output, "    .align  %d\n", alignment);
        fprintf(output, ".globl %s\n", globals[i].name);
        fprintf(output, "%s:\n", globals[i].name);
        for (int j = 0; j < count; j++) {
            const char *directive = element_size == 1 ? ".byte  " :
                                    element_size == 2 ? ".short " :
                                    element_size == 8 ? ".quad  " : ".long  ";
            int value = globals[i].values[j];

            /* Truncate to the element width, as a store would. */
            if (element_size == 1) {
                value = element && element->is_unsigned
                    ? (int)(unsigned char)value : (int)(signed char)value;
            } else if (element_size == 2) {
                value = element && element->is_unsigned
                    ? (int)(unsigned short)value : (int)(short)value;
            }
            fprintf(output, "    %s %d\n", directive, value);
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

/*
 * System V passes the first six integer or pointer arguments in these
 * registers, in this order.
 */
static const char *arg_reg64[6] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };
static const char *arg_reg32[6] = { "%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d" };

/* Spill an incoming register argument into the frame slot it was given. */
static void emit_spill_parameter(struct Symbol *sym, FILE *output)
{
    int size = sym->ty ? sym->ty->size : 8;
    int i = sym->param_index;

    if (i >= 6) {
        return;             /* already on the stack, addressed in place */
    }
    if (size == 8) {
        fprintf(output, "    movq    %s, %d(%%rbp)\n", arg_reg64[i], sym->offset);
    } else {
        /*
         * Narrower arguments arrive promoted to 32 bits, and the slot is at
         * least a word wide, so one 4-byte store is correct for all of them.
         */
        fprintf(output, "    movl    %s, %d(%%rbp)\n", arg_reg32[i], sym->offset);
    }
}

static void generate_function(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
{
    /*
     * Frame size was computed during semantic analysis, which knows the scopes
     * and can reuse slots between disjoint blocks. Nothing is collected here.
     *
     * The call pushed an 8-byte return address and the prologue pushes %rbp, so
     * %rsp is 16-byte aligned once both are on the stack. Rounding the frame to
     * a multiple of 16 keeps it that way, which System V requires at every call.
     */
    int frame_size = node->sym ? node->sym->frame_size : 0;
    struct ast_node *param;

    frame_size = (frame_size + 15) / 16 * 16;
    ctx->current_function_end_label = ctx->label_count++;

    fprintf(output, ".globl %s\n", node->value);
    fprintf(output, "%s:\n", node->value);
    fprintf(output, "    pushq   %%rbp\n");
    fprintf(output, "    movq    %%rsp, %%rbp\n");
    if (frame_size > 0) {
        fprintf(output, "    subq    $%d, %%rsp\n", frame_size);
    }

    for (param = node->left; param; param = param->right) {
        if (param->type == AST_PARAM_LIST && param->left->sym) {
            emit_spill_parameter(param->left->sym, output);
        }
    }

    generate_statement(ctx, node->right, output);
    fprintf(output, "    movl    $0, %%eax\n");
    fprintf(output, ".L%d:\n", ctx->current_function_end_label);
    generate_epilogue(output);
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
        case AST_FUNCTION_DECL:
            /* A prototype declares; there is nothing to emit for it. */
            break;
        case AST_STRUCT_DEF:
            break;
        default:
            diag_internal(node->location, "unsupported program node type %d",
                node->type);
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
                int offset = node->sym->offset;
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
                /*
                 * A scalar local owns a whole 4-byte slot and is read back with
                 * a 4-byte load, so write all four bytes. The value has already
                 * been narrowed and extended to its declared type. Narrow
                 * stores are only for packed array elements, above.
                 */
                generate_exp(ctx, node->left, output);
                emit_store_slot(node->ty, node->sym->offset, output);
            } else {
                emit_zero_slot(node->ty, node->sym->offset, output);
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
                diag_internal(node->location, "'break' outside a loop");
            }
            fprintf(output, "    jmp     .L%d\n", ctx->loop_break_labels[ctx->loop_depth - 1]);
            break;
        case AST_CONTINUE:
            if (ctx->loop_depth == 0) {
                diag_internal(node->location, "'continue' outside a loop");
            }
            fprintf(output, "    jmp     .L%d\n", ctx->loop_continue_labels[ctx->loop_depth - 1]);
            break;
        default:
            diag_internal(node->location, "unsupported statement node type %d",
                node->type);
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
        fprintf(output, "    pushq   %%rax\n");
        generate_exp(ctx, node->right, output);
        fprintf(output, "    popq    %%rdx\n");

        if (left_is_pointer && right_is_pointer && node->type == AST_SUB) {
            fprintf(output, "    subq    %%rax, %%rdx\n");
            fprintf(output, "    movq    %%rdx, %%rax\n");
            fprintf(output, "    cqto\n");
            fprintf(output, "    movq    $%d, %%rcx\n",
                ty_element_size(node->left->ty));
            fprintf(output, "    idivq   %%rcx\n");
            return;
        }
        if (left_is_pointer && !right_is_pointer) {
            fprintf(output, "    cltq\n");
            fprintf(output, "    imulq   $%d, %%rax\n",
                ty_element_size(node->left->ty));
            if (node->type == AST_ADD) {
                fprintf(output, "    addq    %%rdx, %%rax\n");
            } else {
                fprintf(output, "    subq    %%rax, %%rdx\n");
                fprintf(output, "    movq    %%rdx, %%rax\n");
            }
            return;
        }
        if (right_is_pointer && !left_is_pointer && node->type == AST_ADD) {
            fprintf(output, "    imulq   $%d, %%rdx\n",
                ty_element_size(node->right->ty));
            fprintf(output, "    addq    %%rdx, %%rax\n");
            return;
        }
    }

    generate_exp(ctx, node->left, output);
    fprintf(output, "    pushq   %%rax\n");

    generate_exp(ctx, node->right, output);
    fprintf(output, "    popq    %%rdx\n");

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
            fprintf(output, "    pushq   %%rax\n");
            fprintf(output, "    movl    %%edx, %%eax\n");
            fprintf(output, "    popq    %%rcx\n");
            if (is_unsigned) {
                fprintf(output, "    xorl    %%edx, %%edx\n");
                fprintf(output, "    divl    %%ecx\n");
            } else {
                fprintf(output, "    cdq\n");
                fprintf(output, "    idivl   %%ecx\n");
            }
            break;
        case AST_MOD:
            fprintf(output, "    pushq   %%rax\n");
            fprintf(output, "    movl    %%edx, %%eax\n");
            fprintf(output, "    popq    %%rcx\n");
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
            emit_compare(node->left->ty, node->right->ty, output);
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    sete    %%al\n");
            break;
        case AST_NOT_EQUAL:
            emit_compare(node->left->ty, node->right->ty, output);
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    setne   %%al\n");
            break;
        case AST_LESS:
            emit_compare(node->left->ty, node->right->ty, output);
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, is_unsigned ? "    setb    %%al\n" : "    setl    %%al\n");
            break;
        case AST_LESS_EQUAL:
            emit_compare(node->left->ty, node->right->ty, output);
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, is_unsigned ? "    setbe   %%al\n" : "    setle   %%al\n");
            break;
        case AST_GREATER:
            emit_compare(node->left->ty, node->right->ty, output);
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, is_unsigned ? "    seta    %%al\n" : "    setg    %%al\n");
            break;
        case AST_GREATER_EQUAL:
            emit_compare(node->left->ty, node->right->ty, output);
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, is_unsigned ? "    setae   %%al\n" : "    setge   %%al\n");
            break;
        default:
            diag_internal(node->location, "unsupported operation in AST");
    }
}

static void generate_exp(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
{
    switch (node->type) {
        case AST_INTLIT:
            fprintf(output, "    movl    $%s, %%eax\n", node->value);
            break;
        case AST_STRINGLIT:
            fprintf(output, "    leaq    .LC%d(%%rip), %%rax\n", node->string_label);
            break;
        case AST_IDENTIFIER:
            generate_identifier_load(node, output);
            break;
        case AST_CALL: {
            int arg_count = count_args(node->left);
            int stack_args = arg_count > 6 ? arg_count - 6 : 0;
            /*
             * %rsp is 16-byte aligned here. Each push moves it by 8, so an odd
             * number of stack arguments would leave it misaligned at the call,
             * which System V forbids.
             */
            int padding = (stack_args % 2) ? 8 : 0;
            int registers = arg_count < 6 ? arg_count : 6;
            int i;

            if (padding) {
                fprintf(output, "    subq    $8, %%rsp\n");
            }

            /*
             * Arguments are pushed last-first, so the first one ends up on top
             * and the register arguments pop off in order.
             */
            generate_call_args(ctx, node->left, output);
            for (i = 0; i < registers; i++) {
                fprintf(output, "    popq    %s\n", arg_reg64[i]);
            }

            /* A variadic callee reads %al for the count of vector registers. */
            fprintf(output, "    movl    $0, %%eax\n");
            fprintf(output, "    call    %s\n", node->value);

            if (stack_args > 0 || padding) {
                fprintf(output, "    addq    $%d, %%rsp\n",
                    stack_args * 8 + padding);
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
            fprintf(output, "    pushq   %%rax\n");
            generate_lvalue_address(ctx, node->left, output);
            fprintf(output, "    popq    %%rdx\n");
            emit_store_indirect(node->left->ty, output);
            fprintf(output, node->left->ty && node->left->ty->size == 8
                ? "    movq    %%rdx, %%rax\n" : "    movl    %%edx, %%eax\n");
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
            generate_identifier_load(node->left, output);
            emit_step(node->ty, 1, output);
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left, output);
            break;
        case AST_PRE_DECREMENT:
            generate_identifier_load(node->left, output);
            emit_step(node->ty, 0, output);
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left, output);
            break;
        case AST_POST_INCREMENT:
            generate_identifier_load(node->left, output);
            fprintf(output, "    pushq   %%rax\n");
            emit_step(node->ty, 1, output);
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left, output);
            fprintf(output, "    popq    %%rax\n");
            break;
        case AST_POST_DECREMENT:
            generate_identifier_load(node->left, output);
            fprintf(output, "    pushq   %%rax\n");
            emit_step(node->ty, 0, output);
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left, output);
            fprintf(output, "    popq    %%rax\n");
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
            diag_internal(node->location, "unsupported AST node type %d",
                node->type);
    }
}

static int count_args(struct ast_node *node)
{
    int count = 0;

    for (; node; node = node->right) {
        count++;
    }
    return count;
}

static int generate_call_args(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
{
    if (!node) {
        return 0;
    }

    if (node->type != AST_ARG_LIST) {
        diag_internal(node->location, "unsupported argument node type %d",
            node->type);
    }

    int count = generate_call_args(ctx, node->right, output);
    generate_exp(ctx, node->left, output);
    fprintf(output, "    pushq   %%rax\n");

    return count + 1;
}

void write_assembly_to_file(const char *filename, struct ast_node *ast)
{
    /*
     * Binary mode, so a newline stays one byte on every platform. In text mode
     * Windows would write CRLF, and the generated assembly would differ from
     * the same compiler's output on Linux -- which would make the byte-for-byte
     * golden comparison platform-dependent. "b" is a no-op on POSIX.
     */
    FILE *out_file = fopen(filename, "wb");
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
