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

/* A goto target: the source name paired with the emitted label number. */
struct cg_label {
    char *name;
    int number;
};

struct cg_ctx {
    int label_count;
    int current_function_end_label;

    /*
     * Labels in the function being generated. They are collected before the
     * body is emitted, because a goto may jump forward to a label that has not
     * been reached yet.
     */
    struct cg_label *labels;
    int label_name_count;
    int label_name_capacity;

    /* The break target of the switch being generated, if any. */
    int switch_break_label;
    int in_switch;

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
static int arg_slots(struct ast_node *value);
static int count_arg_slots(struct ast_node *node);
static void generate_lvalue_address(struct cg_ctx *ctx, struct ast_node *node, FILE *output);
static void generate_cast(const char *type, FILE *output);
static const char *codegen_type_name(CType type);
static void emit_step(struct Type *ty, int is_increment, FILE *output);

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

/* ------------------------------------------------------ floating point -- */

/*
 * Floating-point values live in %xmm0, not %rax, so they need a parallel set
 * of moves. The suffix follows the width: ss for a 4-byte float, sd for an
 * 8-byte double. Everything else about the stack machine is unchanged --
 * intermediate values are pushed and popped, just through %xmm registers.
 */
static const char *fp_suffix(struct Type *ty)
{
    return ty && ty->size == 4 ? "ss" : "sd";
}

/* Interned floating-point constants, emitted into .data and loaded from there. */
struct cg_double {
    char *text;
    int is_float;
    int label;
};

static struct cg_double *fp_constants;
static int fp_constant_count;
static int fp_constant_capacity;

static int intern_fp_constant(const char *text, int is_float)
{
    int i;

    for (i = 0; i < fp_constant_count; i++) {
        if (fp_constants[i].is_float == is_float &&
            strcmp(fp_constants[i].text, text) == 0) {
            return fp_constants[i].label;
        }
    }

    if (fp_constant_count >= fp_constant_capacity) {
        fp_constant_capacity = fp_constant_capacity ? fp_constant_capacity * 2 : 16;
        fp_constants = realloc(fp_constants,
            (size_t)fp_constant_capacity * sizeof(*fp_constants));
        if (!fp_constants) {
            fprintf(stderr, "Out of memory in the code generator\n");
            exit(EXIT_FAILURE);
        }
    }
    fp_constants[fp_constant_count].text = strdup(text);
    /*
     * The f suffix tells the lexer the literal is a float; the assembler
     * directive takes only the number, so drop it here.
     */
    {
        char *stored = fp_constants[fp_constant_count].text;
        size_t length = strlen(stored);

        if (length > 0 && (stored[length - 1] == 'f' || stored[length - 1] == 'F')) {
            stored[length - 1] = 0;
        }
    }
    fp_constants[fp_constant_count].is_float = is_float;
    fp_constants[fp_constant_count].label = fp_constant_count;
    return fp_constants[fp_constant_count++].label;
}

static void free_fp_constants(void)
{
    int i;

    for (i = 0; i < fp_constant_count; i++) {
        free(fp_constants[i].text);
    }
    free(fp_constants);
    fp_constants = NULL;
    fp_constant_count = 0;
    fp_constant_capacity = 0;
}

/* Push and pop the floating-point accumulator, mirroring pushq/popq. */
static void emit_fp_push(struct Type *ty, FILE *output)
{
    fprintf(output, "    subq    $8, %%rsp\n");
    fprintf(output, "    mov%s   %%xmm0, (%%rsp)\n", fp_suffix(ty));
}

static void emit_fp_pop(struct Type *ty, const char *reg, FILE *output)
{
    fprintf(output, "    mov%s   (%%rsp), %%%s\n", fp_suffix(ty), reg);
    fprintf(output, "    addq    $8, %%rsp\n");
}

static void emit_fp_load_frame(struct Type *ty, int offset, FILE *output)
{
    fprintf(output, "    mov%s   %d(%%rbp), %%xmm0\n", fp_suffix(ty), offset);
}

static void emit_fp_store_frame(struct Type *ty, int offset, FILE *output)
{
    fprintf(output, "    mov%s   %%xmm0, %d(%%rbp)\n", fp_suffix(ty), offset);
}

/* Convert whatever is in %eax to floating point in %xmm0. */
static void emit_int_to_fp(struct Type *target, FILE *output)
{
    fprintf(output, "    cvtsi2%s %%eax, %%xmm0\n", fp_suffix(target));
}

/* Convert %xmm0 to an integer in %eax, truncating as C requires. */
static void emit_fp_to_int(struct Type *source, FILE *output)
{
    fprintf(output, "    cvtt%s2si %%xmm0, %%eax\n", fp_suffix(source));
}

static void emit_fp_widen(struct Type *from, struct Type *to, FILE *output)
{
    if (!from || !to || from->size == to->size) {
        return;
    }
    if (from->size == 4) {
        fprintf(output, "    cvtss2sd %%xmm0, %%xmm0\n");
    } else {
        fprintf(output, "    cvtsd2ss %%xmm0, %%xmm0\n");
    }
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
    /* A floating value is in %xmm0, not %rax. */
    if (ty_is_float(ty)) {
        emit_fp_store_frame(ty, offset, output);
        return;
    }
    if (ty && ty->size == 8) {
        fprintf(output, "    movq    %%rax, %d(%%rbp)\n", offset);
    } else {
        fprintf(output, "    movl    %%eax, %d(%%rbp)\n", offset);
    }
}

static void emit_zero_slot(struct Type *ty, int offset, FILE *output)
{
    /* Zeroing a floating slot writes the bit pattern, so an integer store. */
    if (ty_is_float(ty)) {
        fprintf(output, "    movq    $0, %d(%%rbp)\n", offset);
        return;
    }
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

/*
 * Labels are gathered before the function body is emitted: a goto may target a
 * label further down, so the number has to exist before the jump is written.
 */
static int label_for_name(struct cg_ctx *ctx, const char *name)
{
    int i;

    for (i = 0; i < ctx->label_name_count; i++) {
        if (strcmp(ctx->labels[i].name, name) == 0) {
            return ctx->labels[i].number;
        }
    }

    cg_grow((void **)&ctx->labels, ctx->label_name_count,
        &ctx->label_name_capacity, sizeof(*ctx->labels));
    ctx->labels[ctx->label_name_count].name = strdup(name);
    ctx->labels[ctx->label_name_count].number = ctx->label_count++;
    return ctx->labels[ctx->label_name_count++].number;
}

static void collect_labels(struct cg_ctx *ctx, struct ast_node *node)
{
    if (!node) {
        return;
    }
    if (node->type == AST_LABEL && node->value) {
        label_for_name(ctx, node->value);
    }
    collect_labels(ctx, node->left);
    collect_labels(ctx, node->right);
}

static void free_labels(struct cg_ctx *ctx)
{
    int i;

    for (i = 0; i < ctx->label_name_count; i++) {
        free(ctx->labels[i].name);
    }
    free(ctx->labels);
    ctx->labels = NULL;
    ctx->label_name_count = 0;
    ctx->label_name_capacity = 0;
}

/*
 * Emit the comparisons for a switch.
 *
 * Each case is tested against the control value in turn and jumps to its own
 * label. That is a chain of compares rather than a jump table -- correct for
 * any set of case values, including sparse ones, and the shape an optimiser
 * would later turn into a table where the values are dense.
 */
static void emit_switch_tests(struct cg_ctx *ctx, struct ast_node *node,
    int *default_label, FILE *output)
{
    if (!node) {
        return;
    }

    if (node->type == AST_CASE) {
        node->string_label = ctx->label_count++;    /* reused as this case's label */
        fprintf(output, "    cmpl    $%s, %%eax\n", node->value ? node->value : "0");
        fprintf(output, "    je      .L%d\n", node->string_label);
        emit_switch_tests(ctx, node->left, default_label, output);
        return;
    }
    if (node->type == AST_DEFAULT) {
        node->string_label = ctx->label_count++;
        *default_label = node->string_label;
        emit_switch_tests(ctx, node->left, default_label, output);
        return;
    }

    /*
     * Only the statements that can contain a case label of this switch are
     * followed. A nested switch owns its own cases, so it is not entered.
     */
    if (node->type == AST_SWITCH) {
        return;
    }
    emit_switch_tests(ctx, node->left, default_label, output);
    emit_switch_tests(ctx, node->right, default_label, output);
}

/*
 * ++ and -- on something that is not a plain variable: an array element or a
 * struct field. The address is computed once and kept, so the operand is not
 * evaluated twice -- which would be wrong the moment the subscript had a side
 * effect. The result is the new value for a prefix operator and the old one
 * for a postfix operator.
 */
static void generate_incdec_lvalue(struct cg_ctx *ctx, struct ast_node *node,
    int is_increment, int is_prefix, FILE *output)
{
    struct Type *ty = node->ty;

    generate_lvalue_address(ctx, node->left, output);
    fprintf(output, "    pushq   %%rax\n");          /* the address */
    emit_load_indirect(ty, output);

    if (!is_prefix) {
        fprintf(output, "    pushq   %%rax\n");      /* the value before */
    }

    emit_step(ty, is_increment, output);
    generate_cast(codegen_type_name(node->data_type), output);
    fprintf(output, "    movq    %%rax, %%rdx\n");   /* the value to store */

    if (!is_prefix) {
        fprintf(output, "    popq    %%rcx\n");      /* the value before */
    }
    fprintf(output, "    popq    %%rax\n");          /* the address */
    emit_store_indirect(ty, output);

    fprintf(output, is_prefix ? "    movq    %%rdx, %%rax\n"
                              : "    movq    %%rcx, %%rax\n");
}

/*
 * Copy a struct from one place to another.
 *
 * A struct is wider than a register, so assigning one is a block copy rather
 * than a move: the destination address is in %rax and the source in %rdx, and
 * the bytes are moved in the largest chunks that fit. Sizes here are small and
 * known at compile time, so the copy is unrolled rather than looped.
 */
static void emit_struct_copy(int size, FILE *output)
{
    int offset = 0;

    while (size - offset >= 8) {
        fprintf(output, "    movq    %d(%%rdx), %%rcx\n", offset);
        fprintf(output, "    movq    %%rcx, %d(%%rax)\n", offset);
        offset += 8;
    }
    while (size - offset >= 4) {
        fprintf(output, "    movl    %d(%%rdx), %%ecx\n", offset);
        fprintf(output, "    movl    %%ecx, %d(%%rax)\n", offset);
        offset += 4;
    }
    while (size - offset >= 1) {
        fprintf(output, "    movb    %d(%%rdx), %%cl\n", offset);
        fprintf(output, "    movb    %%cl, %d(%%rax)\n", offset);
        offset += 1;
    }
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

    /* A function's name is its address; there is nothing to load from. */
    if (sym->kind == SYM_FUNCTION) {
        fprintf(output, "    leaq    %s(%%rip), %%rax\n", sym->name);
        return;
    }

    if (ty_is_float(sym->ty)) {
        if (is_frame_symbol(sym)) {
            emit_fp_load_frame(sym->ty, sym->offset, output);
        } else {
            fprintf(output, "    mov%s   %s(%%rip), %%xmm0\n",
                fp_suffix(sym->ty), sym->name);
        }
        return;
    }

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

    if (ty_is_float(sym->ty)) {
        if (is_frame_symbol(sym)) {
            emit_fp_store_frame(sym->ty, sym->offset, output);
        } else {
            fprintf(output, "    mov%s   %%xmm0, %s(%%rip)\n",
                fp_suffix(sym->ty), sym->name);
        }
        return;
    }

    if (is_frame_symbol(sym)) {
        emit_store_slot(sym->ty, sym->offset, output);
        return;
    }

    emit_store_global(sym->ty, sym->name, output);
}

/*
 * Fill an object at a frame offset from a brace initializer. This is the same
 * work a declaration with an initializer does, factored out so a compound
 * literal -- which has storage but no declaration -- can reuse it.
 */
static void generate_initializer_into(struct cg_ctx *ctx, struct Type *ty,
    int offset, struct ast_node *initializer, FILE *output)
{
    struct ast_node *item;

    if (!ty) {
        return;
    }

    if (ty->kind == TY_ARRAY) {
        int stride = ty_element_size(ty);
        struct Type *element = ty->base;
        int index;
        char *written = calloc((size_t)ty->array_length, 1);

        if (!written) {
            return;
        }
        index = 0;
        for (item = initializer_items(initializer); item; item = item->right) {
            if (item->left && item->left->designator_index >= 0) {
                index = item->left->designator_index;
            }
            if (index >= 0 && index < ty->array_length) {
                written[index] = 1;
            }
            index++;
        }
        for (index = 0; index < ty->array_length; index++) {
            if (!written[index]) {
                emit_zero_offset(element, offset + (index * stride), output);
            }
        }
        free(written);

        index = 0;
        for (item = initializer_items(initializer); item; item = item->right) {
            if (item->left && item->left->designator_index >= 0) {
                index = item->left->designator_index;
            }
            if (index >= ty->array_length) {
                break;
            }
            generate_exp(ctx, item->left, output);
            emit_store_offset(element, offset + (index * stride), output);
            index++;
        }
        return;
    }

    if (ty->kind == TY_STRUCT) {
        struct Member *member = ty->members;
        int i;

        for (i = 0; i < ty->size; i += 4) {
            fprintf(output, "    movl    $0, %d(%%rbp)\n", offset + i);
        }
        for (item = initializer_items(initializer); item; item = item->right) {
            /*
             * A designator names its member, so it is looked up before the
             * running one is consulted. Testing that first matters: after a
             * designator that named the last member there is no next one, and
             * requiring one would drop every element after it.
             */
            if (item->left && item->left->designator_field) {
                member = ty_find_member(ty, item->left->designator_field);
            }
            if (!member) {
                break;
            }
            generate_exp(ctx, item->left, output);
            emit_store_offset(member->ty, offset + member->offset, output);
            member = member->next;
        }
        return;
    }

    /* A scalar compound literal, as in (int){5}. */
    if (initializer) {
        generate_exp(ctx, initializer_items(initializer)
            ? initializer_items(initializer)->left : initializer, output);
        emit_store_slot(ty, offset, output);
    }
}

static void generate_lvalue_address(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
{
    switch (node->type) {
        case AST_COMPOUND_LITERAL:
            /*
             * The literal is written into its slot at the point it appears,
             * then behaves like any other object at that address.
             */
            generate_initializer_into(ctx, node->ty, node->sym->offset,
                node->left, output);
            fprintf(output, "    leaq    %d(%%rbp), %%rax\n", node->sym->offset);
            return;
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
    /*
     * sizeof(struct X): the layout semantic analysis attached to the node.
     * Only when a tag was named -- every sizeof node carries a ty of its own
     * (unsigned, the type of the result), which is not what is being measured.
     */
    if (!node->left && node->struct_name && node->ty) {
        return node->ty->size;
    }
    if (node->value) {
        return ty_from_name(node->value)->size;
    }
    return 4;
}

/*
 * A cast that crosses between integer and floating point is a conversion
 * instruction, not a narrowing move: the value changes representation as well
 * as width. The node's own type says what it is being converted to, and the
 * operand's says what from.
 */
static void generate_cast_between(struct Type *from, struct Type *to, FILE *output)
{
    if (ty_is_float(from) && ty_is_float(to)) {
        emit_fp_widen(from, to, output);
        return;
    }
    if (ty_is_float(from)) {
        emit_fp_to_int(from, output);       /* truncates, as C requires */
        return;
    }
    if (ty_is_float(to)) {
        emit_int_to_fp(to, output);
    }
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
 * Floating constants are written after the code, not before it. SSE cannot
 * take an immediate, so each one becomes a labelled datum -- but which ones
 * exist is only known once every function body has been generated.
 */
static void generate_fp_constants(FILE *output)
{
    int i;

    if (fp_constant_count == 0) {
        return;
    }

    fprintf(output, ".data\n");
    for (i = 0; i < fp_constant_count; i++) {
        fprintf(output, "    .align  %d\n", fp_constants[i].is_float ? 4 : 8);
        fprintf(output, ".LF%d:\n", fp_constants[i].label);
        fprintf(output, "    .%s   %s\n",
            fp_constants[i].is_float ? "float " : "double",
            fp_constants[i].text);
    }
    fprintf(output, ".text\n");
}

/*
 * Tell the linker the program does not need an executable stack. Without this
 * note GNU ld assumes it might, marks the stack executable, and warns -- which
 * is both a security regression and noise on every link.
 */
static void generate_stack_note(FILE *output)
{
    fprintf(output, ".section .note.GNU-stack,\"\",@progbits\n");
}

/*
 * System V passes the first six integer or pointer arguments in these
 * registers, in this order.
 */
static const char *arg_reg64[6] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };
static const char *arg_reg32[6] = { "%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d" };
static const char *arg_reg16[6] = { "%di", "%si", "%dx", "%cx", "%r8w", "%r9w" };
static const char *arg_reg8[6]  = { "%dil", "%sil", "%dl", "%cl", "%r8b", "%r9b" };

/* Spill an incoming register argument into the frame slot it was given. */
static void emit_spill_parameter(struct Symbol *sym, FILE *output)
{
    int size = sym->ty ? sym->ty->size : 8;
    int i = sym->param_index;

    if (i >= 6) {
        return;             /* already on the stack, addressed in place */
    }

    /*
     * A struct arrived in one register per eightbyte; write them back into its
     * slot in order so it looks like any other object in memory.
     */
    if (sym->ty && sym->ty->kind == TY_STRUCT) {
        int slots = (sym->ty->size + 7) / 8;
        int slot;

        for (slot = 0; slot < slots; slot++) {
            int remaining = sym->ty->size - slot * 8;
            int reg = sym->integer_index + slot;
            int at = sym->offset + slot * 8;

            /*
             * The last eightbyte may be partial. Writing all eight bytes of it
             * would run past the end of the struct and into whatever the frame
             * put next -- a four-byte struct owns four bytes, not eight.
             */
            if (remaining >= 8) {
                fprintf(output, "    movq    %s, %d(%%rbp)\n", arg_reg64[reg], at);
            } else if (remaining > 2) {
                fprintf(output, "    movl    %s, %d(%%rbp)\n", arg_reg32[reg], at);
            } else if (remaining == 2) {
                fprintf(output, "    movw    %s, %d(%%rbp)\n", arg_reg16[reg], at);
            } else {
                fprintf(output, "    movb    %s, %d(%%rbp)\n", arg_reg8[reg], at);
            }
        }
        return;
    }

    /*
     * A floating parameter arrives in an SSE register, counted separately from
     * the integer ones. It is passed as a double, so a float parameter is
     * narrowed on the way into its slot.
     */
    if (ty_is_float(sym->ty)) {
        if (size == 4) {
            fprintf(output, "    cvtsd2ss %%xmm%d, %%xmm%d\n",
                sym->float_index, sym->float_index);
        }
        fprintf(output, "    mov%s   %%xmm%d, %d(%%rbp)\n",
            fp_suffix(sym->ty), sym->float_index, sym->offset);
        return;
    }
    if (size == 8) {
        fprintf(output, "    movq    %s, %d(%%rbp)\n", arg_reg64[sym->integer_index],
            sym->offset);
    } else {
        /*
         * Narrower arguments arrive promoted to 32 bits, and the slot is at
         * least a word wide, so one 4-byte store is correct for all of them.
         */
        fprintf(output, "    movl    %s, %d(%%rbp)\n", arg_reg32[sym->integer_index],
            sym->offset);
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
    collect_labels(ctx, node->right);

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
    free_labels(ctx);
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
                struct Type *element = node->ty ? node->ty->base : NULL;
                int index;
                struct ast_node *item;

                /*
                 * Work out which elements the initialisers cover before
                 * emitting anything, so only the gaps are zeroed. Designators
                 * can leave holes -- `{[3] = 1}` writes one element of four --
                 * while a plain list usually covers everything and needs no
                 * zeroing at all.
                 */
                char *written = calloc((size_t)node->array_length, 1);

                if (!written) {
                    fprintf(output, "    /* out of memory */\n");
                    break;
                }
                index = 0;
                for (item = initializer_items(node->left); item; item = item->right) {
                    if (item->left && item->left->designator_index >= 0) {
                        index = item->left->designator_index;
                    }
                    if (index >= 0 && index < node->array_length) {
                        written[index] = 1;
                    }
                    index++;
                }
                for (index = 0; index < node->array_length; index++) {
                    if (!written[index]) {
                        emit_zero_offset(element, offset + (index * stride), output);
                    }
                }
                free(written);

                index = 0;
                for (item = initializer_items(node->left); item; item = item->right) {
                    /*
                     * A designator says where its element goes; the ones after
                     * it continue from there, so the running index is set
                     * rather than stepped.
                     */
                    if (item->left && item->left->designator_index >= 0) {
                        index = item->left->designator_index;
                    }
                    if (index >= node->array_length) {
                        diag_at(DIAG_ERROR, item->left->location,
                            "initializer index %d is outside '%s'",
                            index, node->value);
                        break;
                    }
                    generate_exp(ctx, item->left, output);
                    emit_store_offset(element, offset + (index * stride), output);
                    index++;
                }
            } else if (node->ty && node->ty->kind == TY_STRUCT && node->left &&
                       node->left->type == AST_CALL) {
                /* Initialised from a call: the result arrived in %rax:%rdx. */
                int slots = (node->ty->size + 7) / 8;

                generate_exp(ctx, node->left, output);
                fprintf(output, "    movq    %%rax, %d(%%rbp)\n", node->sym->offset);
                if (slots > 1) {
                    fprintf(output, "    movq    %%rdx, %d(%%rbp)\n",
                        node->sym->offset + 8);
                }
            } else if (node->ty && node->ty->kind == TY_STRUCT && node->left) {
                /*
                 * Brace initialisation of a struct. Without a designator each
                 * value fills the next member in declaration order; with one it
                 * fills the member it names, and the rest follow from there.
                 */
                int offset = node->sym->offset;
                struct Member *member = node->ty->members;
                struct ast_node *item;
                int i;

                for (i = 0; i < node->ty->size; i += 4) {
                    fprintf(output, "    movl    $0, %d(%%rbp)\n", offset + i);
                }

                for (item = initializer_items(node->left); item; item = item->right) {
                    /*
                     * Look the designator up before consulting the running
                     * member: after one that named the last field there is no
                     * next, and requiring one would drop what follows.
                     */
                    if (item->left && item->left->designator_field) {
                        member = ty_find_member(node->ty,
                            item->left->designator_field);
                        if (!member) {
                            diag_at(DIAG_ERROR, item->left->location,
                                "'%s' has no field '%s'",
                                node->struct_name ? node->struct_name : "struct",
                                item->left->designator_field);
                            break;
                        }
                    }
                    if (!member) {
                        break;
                    }
                    generate_exp(ctx, item->left, output);
                    emit_store_offset(member->ty, offset + member->offset, output);
                    member = member->next;
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
            /*
             * `return;` carries no expression. The value in %eax is then
             * whatever the caller must not look at, which is exactly what a
             * void return means.
             */
            if (node->left && node->left->ty &&
                node->left->ty->kind == TY_STRUCT) {
                /*
                 * A struct of up to sixteen bytes comes back in %rax and %rdx,
                 * one eightbyte each, the same classification used to pass one.
                 */
                int slots = (node->left->ty->size + 7) / 8;

                generate_lvalue_address(ctx, node->left, output);
                if (slots > 1) {
                    fprintf(output, "    movq    8(%%rax), %%rdx\n");
                }
                fprintf(output, "    movq    (%%rax), %%rax\n");
            } else if (node->left) {
                generate_exp(ctx, node->left, output);
            }
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
        case AST_EMPTY:
            break;
        case AST_LABEL:
            fprintf(output, ".L%d:\n", label_for_name(ctx, node->value));
            generate_statement(ctx, node->left, output);
            break;
        case AST_GOTO:
            fprintf(output, "    jmp     .L%d\n", label_for_name(ctx, node->value));
            break;
        case AST_DO_WHILE: {
            int body_label = ctx->label_count++;
            int condition_label = ctx->label_count++;
            int end_label = ctx->label_count++;

            /*
             * The body comes first and the test last, so the body always runs
             * once. continue goes to the test, not to the top.
             */
            fprintf(output, ".L%d:\n", body_label);
            push_loop(ctx, end_label, condition_label);
            generate_statement(ctx, node->right, output);
            pop_loop(ctx);
            fprintf(output, ".L%d:\n", condition_label);
            generate_exp(ctx, node->left, output);
            fprintf(output, "    cmpl    $0, %%eax\n");
            fprintf(output, "    jne     .L%d\n", body_label);
            fprintf(output, ".L%d:\n", end_label);
            break;
        }
        case AST_SWITCH: {
            int end_label = ctx->label_count++;
            int default_label = -1;
            int saved_break = ctx->switch_break_label;
            int saved_in_switch = ctx->in_switch;

            /*
             * Evaluate the control value once, then compare it against each
             * case. The tests are emitted before the body, so control reaches
             * the matching label without running the statements above it.
             */
            generate_exp(ctx, node->left, output);
            emit_switch_tests(ctx, node->right, &default_label, output);
            fprintf(output, "    jmp     .L%d\n",
                default_label >= 0 ? default_label : end_label);

            ctx->switch_break_label = end_label;
            ctx->in_switch = 1;
            push_loop(ctx, end_label, ctx->loop_depth > 0
                ? ctx->loop_continue_labels[ctx->loop_depth - 1] : end_label);
            generate_statement(ctx, node->right, output);
            pop_loop(ctx);
            ctx->switch_break_label = saved_break;
            ctx->in_switch = saved_in_switch;

            fprintf(output, ".L%d:\n", end_label);
            break;
        }
        case AST_CASE:
        case AST_DEFAULT:
            /* The label number was assigned while the tests were emitted. */
            fprintf(output, ".L%d:\n", node->string_label);
            generate_statement(ctx, node->left, output);
            break;
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

/*
 * Floating arithmetic. Both operands are evaluated into %xmm0 in turn, the
 * first parked on the stack, so the shape matches the integer stack machine.
 * The left operand ends up in %xmm1 and the right in %xmm0, which is the wrong
 * way round for the non-commutative operators -- hence the swap.
 */
static int generate_float_binop(struct cg_ctx *ctx, struct ast_node *node,
    FILE *output)
{
    struct Type *ty = ty_is_float(node->left->ty) ? node->left->ty : node->right->ty;
    const char *suffix = fp_suffix(ty);
    const char *op = NULL;
    const char *set = NULL;
    int is_comparison = 0;

    switch (node->type) {
        case AST_ADD: op = "add"; break;
        case AST_SUB: op = "sub"; break;
        case AST_MUL: op = "mul"; break;
        case AST_DIV: op = "div"; break;
        case AST_EQUAL:         set = "sete";  is_comparison = 1; break;
        case AST_NOT_EQUAL:     set = "setne"; is_comparison = 1; break;
        case AST_LESS:          set = "seta";  is_comparison = 1; break;
        case AST_LESS_EQUAL:    set = "setae"; is_comparison = 1; break;
        case AST_GREATER:       set = "seta";  is_comparison = 1; break;
        case AST_GREATER_EQUAL: set = "setae"; is_comparison = 1; break;
        default: return 0;
    }

    generate_exp(ctx, node->left, output);
    emit_fp_push(ty, output);
    generate_exp(ctx, node->right, output);
    emit_fp_pop(ty, "xmm1", output);

    if (is_comparison) {
        /*
         * ucomis compares its second operand against its first and sets the
         * unsigned flags, so ordering the operands turns every relation into an
         * above or above-or-equal test.
         */
        if (node->type == AST_GREATER || node->type == AST_GREATER_EQUAL) {
            fprintf(output, "    ucomi%s %%xmm0, %%xmm1\n", suffix);
        } else {
            fprintf(output, "    ucomi%s %%xmm1, %%xmm0\n", suffix);
        }
        fprintf(output, "    movl    $0, %%eax\n");
        fprintf(output, "    %s   %%al\n", set);
        return 1;
    }

    if (node->type == AST_SUB || node->type == AST_DIV) {
        fprintf(output, "    %s%s   %%xmm0, %%xmm1\n", op, suffix);
        fprintf(output, "    mov%s   %%xmm1, %%xmm0\n", suffix);
    } else {
        fprintf(output, "    %s%s   %%xmm1, %%xmm0\n", op, suffix);
    }
    return 1;
}

void generate_binop(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
{
    /* Either operand being floating point makes the whole operation so. */
    if (ty_is_float(node->left->ty) || ty_is_float(node->right->ty)) {
        if (generate_float_binop(ctx, node, output)) {
            return;
        }
    }

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
        case AST_FLOATLIT: {
            int is_float = node->ty && node->ty->size == 4;
            int label = intern_fp_constant(node->value, is_float);

            fprintf(output, "    mov%s   .LF%d(%%rip), %%xmm0\n",
                fp_suffix(node->ty), label);
            break;
        }
        case AST_STRINGLIT:
            fprintf(output, "    leaq    .LC%d(%%rip), %%rax\n", node->string_label);
            break;
        case AST_IDENTIFIER:
            generate_identifier_load(node, output);
            break;
        case AST_CALL: {
            int arg_count = count_args(node->left);
            /* Registers are assigned per eightbyte, not per argument. */
            int slot_count = count_arg_slots(node->left);
            int stack_args = slot_count > 6 ? slot_count - 6 : 0;
            /*
             * %rsp is 16-byte aligned here. Each push moves it by 8, so an odd
             * number of stack arguments would leave it misaligned at the call,
             * which System V forbids.
             */
            int padding = (stack_args % 2) ? 8 : 0;
            int registers = slot_count < 6 ? slot_count : 6;

            (void)arg_count;
            int float_registers = 0;
            int i;

            if (padding) {
                fprintf(output, "    subq    $8, %%rsp\n");
            }

            /*
             * Arguments are pushed last-first, so the first one ends up on top
             * and the register arguments pop off in order.
             */
            generate_call_args(ctx, node->left, output);
            /*
             * System V has separate register sequences for integer and
             * floating arguments, so each is assigned from its own list in
             * argument order.
             */
            {
                struct ast_node *arg = node->left;
                int integer_index = 0;
                int float_index = 0;

                for (i = 0; i < registers && arg; arg = arg->right) {
                    struct ast_node *value = arg->type == AST_ARG_LIST ? arg->left : arg;

                    if (ty_is_float(value->ty)) {
                        fprintf(output, "    movsd   (%%rsp), %%xmm%d\n", float_index++);
                        fprintf(output, "    addq    $8, %%rsp\n");
                        i++;
                    } else {
                        /* A struct takes one register per eightbyte. */
                        int slots = arg_slots(value);
                        int slot;

                        for (slot = 0; slot < slots && i < registers; slot++, i++) {
                            fprintf(output, "    popq    %s\n",
                                arg_reg64[integer_index++]);
                        }
                    }
                }
                float_registers = float_index;
            }

            /*
             * A call through a function pointer loads the target and calls
             * through the register. The pointer is loaded after the arguments
             * are in place, so evaluating it cannot disturb them.
             */
            if (node->is_indirect_call) {
                fprintf(output, "    movq    %d(%%rbp), %%r10\n", node->sym->offset);
                fprintf(output, "    movl    $0, %%eax\n");
                fprintf(output, "    call    *%%r10\n");
            } else {
                /* A variadic callee reads %al for the vector register count. */
                fprintf(output, "    movl    $%d, %%eax\n", float_registers);
                fprintf(output, "    call    %s\n", node->value);
            }

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
            /*
             * A struct does not fit in a register, so it is copied rather than
             * moved. Both sides are addresses in that case, not values.
             */
            if (node->left->ty && node->left->ty->kind == TY_STRUCT) {
                /*
                 * A call returns its struct in %rax and %rdx, so there is no
                 * address to copy from -- the value is already in registers.
                 */
                if (node->right->type == AST_CALL) {
                    int slots = (node->left->ty->size + 7) / 8;

                    generate_exp(ctx, node->right, output);
                    fprintf(output, "    movq    %%rax, %%rcx\n");
                    if (slots > 1) {
                        fprintf(output, "    movq    %%rdx, %%rsi\n");
                    }
                    generate_lvalue_address(ctx, node->left, output);
                    fprintf(output, "    movq    %%rcx, (%%rax)\n");
                    if (slots > 1) {
                        fprintf(output, "    movq    %%rsi, 8(%%rax)\n");
                    }
                    break;
                }
                generate_lvalue_address(ctx, node->right, output);
                fprintf(output, "    pushq   %%rax\n");
                generate_lvalue_address(ctx, node->left, output);
                fprintf(output, "    popq    %%rdx\n");
                emit_struct_copy(node->left->ty->size, output);
                break;
            }
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
            if (node->left->type != AST_IDENTIFIER) {
                generate_incdec_lvalue(ctx, node, 1, 1, output);
                break;
            }
            generate_identifier_load(node->left, output);
            emit_step(node->ty, 1, output);
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left, output);
            break;
        case AST_PRE_DECREMENT:
            if (node->left->type != AST_IDENTIFIER) {
                generate_incdec_lvalue(ctx, node, 0, 1, output);
                break;
            }
            generate_identifier_load(node->left, output);
            emit_step(node->ty, 0, output);
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left, output);
            break;
        case AST_POST_INCREMENT:
            if (node->left->type != AST_IDENTIFIER) {
                generate_incdec_lvalue(ctx, node, 1, 0, output);
                break;
            }
            generate_identifier_load(node->left, output);
            fprintf(output, "    pushq   %%rax\n");
            emit_step(node->ty, 1, output);
            generate_cast(codegen_type_name(node->data_type), output);
            generate_identifier_store(node->left, output);
            fprintf(output, "    popq    %%rax\n");
            break;
        case AST_POST_DECREMENT:
            if (node->left->type != AST_IDENTIFIER) {
                generate_incdec_lvalue(ctx, node, 0, 0, output);
                break;
            }
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
            if (ty_is_float(node->ty) || ty_is_float(node->left->ty)) {
                generate_cast_between(node->left->ty, node->ty, output);
            } else {
                generate_cast(node->value, output);
            }
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

/*
 * How many argument slots a value occupies. System V splits a struct into
 * eightbytes and classifies each: with no floating fields every one is INTEGER,
 * so a struct of up to sixteen bytes takes one register per eightbyte and
 * everything else takes exactly one.
 */
static int arg_slots(struct ast_node *value)
{
    if (value->ty && value->ty->kind == TY_STRUCT) {
        return (value->ty->size + 7) / 8;
    }
    return 1;
}

static int count_arg_slots(struct ast_node *node)
{
    int slots = 0;

    for (; node; node = node->right) {
        slots += arg_slots(node->type == AST_ARG_LIST ? node->left : node);
    }
    return slots;
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

    /*
     * A struct of eight bytes or fewer is passed as one register-sized value,
     * so load its contents rather than evaluating it as an address.
     */
    if (node->left->ty && node->left->ty->kind == TY_STRUCT) {
        int slots = arg_slots(node->left);
        int slot;

        /* A call already left the struct in %rax and %rdx. */
        if (node->left->type == AST_CALL) {
            generate_exp(ctx, node->left, output);
            if (slots > 1) {
                fprintf(output, "    pushq   %%rdx\n");
            }
            fprintf(output, "    pushq   %%rax\n");
            return count + 1;
        }

        generate_lvalue_address(ctx, node->left, output);
        /*
         * Push the eightbytes highest first, so the lowest ends up on top and
         * pops into the first register.
         */
        for (slot = slots - 1; slot >= 0; slot--) {
            fprintf(output, "    movq    %d(%%rax), %%rdx\n", slot * 8);
            fprintf(output, "    pushq   %%rdx\n");
        }
    } else if (ty_is_float(node->left->ty)) {
        /*
         * A floating argument is always passed as a double: that is what the
         * ABI requires for a variadic call, and it is harmless otherwise.
         */
        generate_exp(ctx, node->left, output);
        if (node->left->ty->size == 4) {
            fprintf(output, "    cvtss2sd %%xmm0, %%xmm0\n");
        }
        fprintf(output, "    subq    $8, %%rsp\n");
        fprintf(output, "    movsd   %%xmm0, (%%rsp)\n");
    } else {
        generate_exp(ctx, node->left, output);
        fprintf(output, "    pushq   %%rax\n");
    }

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
    generate_fp_constants(out_file);
    free_fp_constants();
    generate_stack_note(out_file);
    fclose(out_file);

    for (int i = 0; i < ctx->string_count; i++) {
        free(ctx->strings[i].value);
    }
    free(ctx->strings);
    free(ctx->loop_break_labels);
    free(ctx->loop_continue_labels);
}
