/*
 * Code generator: static data. Globals, string literals, floating constants, and
 * the constant folding their initializers need.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "decl.h"
#include "type.h"
#include "symbol.h"
#include "diag.h"
#include "support/mem.h"
#include "codegen_internal.h"

/*
 * System V passes the first six integer or pointer arguments in these
 * registers, in this order.
 */
const char *arg_reg64[6] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };
const char *arg_reg32[6] = { "%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d" };
const char *arg_reg16[6] = { "%di", "%si", "%dx", "%cx", "%r8w", "%r9w" };
const char *arg_reg8[6]  = { "%dil", "%sil", "%dl", "%cl", "%r8b", "%r9b" };

struct cg_double *fp_constants = NULL;
int fp_constant_count;
int fp_constant_capacity;

struct cg_global globals[256];
int global_count = 0;

int find_global(const char *name)
{
    for (int i = 0; i < global_count; i++) {
        if (strcmp(globals[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

int struct_field_offset(struct Type *ty, const char *field_name)
{
    struct Member *member = ty_find_member(ty, field_name);
    return member ? member->offset : 0;
}

void add_global_node(struct ast_node *node)
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

int intern_fp_constant(const char *text, int is_float)
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

void free_fp_constants(void)
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

int sizeof_node(struct ast_node *node)
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

int cast_constant(int value, const char *type)
{
    if (strcmp(type, "char") == 0) return (int)(int8_t)value;
    if (strcmp(type, "uchar") == 0) return (int)(uint8_t)value;
    if (strcmp(type, "short") == 0) return (int)(int16_t)value;
    if (strcmp(type, "ushort") == 0) return (int)(uint16_t)value;
    return value;
}

int eval_const_exp(struct ast_node *node)
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

void collect_globals(struct ast_node *node)
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

int add_string_literal(struct cg_ctx *ctx, const char *value)
{
    for (int i = 0; i < ctx->string_count; i++) {
        if (strcmp(ctx->strings[i].value, value) == 0) {
            return ctx->strings[i].label;
        }
    }
    grow_array((void **)&ctx->strings, ctx->string_count,
        &ctx->string_capacity, sizeof(*ctx->strings), 32, "code generator state");

    ctx->strings[ctx->string_count].value = strdup(value);
    ctx->strings[ctx->string_count].label = ctx->string_count;
    ctx->string_count++;
    return ctx->string_count - 1;
}

void collect_metadata(struct cg_ctx *ctx, struct ast_node *node)
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

void generate_globals(struct cg_ctx *ctx, FILE *output)
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

void generate_fp_constants(FILE *output)
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
