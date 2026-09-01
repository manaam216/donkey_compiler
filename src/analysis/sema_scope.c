#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "decl.h"
#include "type.h"
#include "symbol.h"
#include "diag.h"
#include "support/mem.h"
#include "sema_internal.h"

/*
 * Turn the parser's syntactic record (base type name, pointer stars, array
 * length, struct tag) into a resolved type. Structs must already be collected,
 * which sema_collect_top_level guarantees by visiting definitions first.
 */
static struct Type *base_type_for(struct sema_ctx *ctx, struct ast_node *node)
{
    if (node->struct_name) {
        int index = sema_find_struct(ctx, node->struct_name);
        if (index >= 0 && ctx->structs[index].ty) {
            return ctx->structs[index].ty;
        }
        return ty_int;
    }
    return ty_from_name(semantic_type_name(node->data_type));
}

struct Type *sema_resolve_type(struct sema_ctx *ctx, struct ast_node *node)
{
    struct Type *type = base_type_for(ctx, node);
    int i;

    /*
     * A declarator that nested records how it derives its type; applying the
     * steps in order is what distinguishes a pointer to an array from an array
     * of pointers.
     */
    if (node->derivation_count > 0) {
        for (i = 0; i < node->derivation_count; i++) {
            switch (node->derivations[i].kind) {
                case DERIVE_POINTER:
                    type = ty_pointer_to(type);
                    break;
                case DERIVE_ARRAY:
                    type = ty_array_of(type, node->derivations[i].length);
                    break;
                case DERIVE_FUNCTION:
                    type = ty_func(type);
                    break;
            }
        }
        return type;
    }

    /* A function pointer points at a function returning the base type. */
    if (node->is_function_pointer) {
        return ty_pointer_to(ty_func(type));
    }

    for (i = 0; i < node->pointer_depth; i++) {
        type = ty_pointer_to(type);
    }
    /*
     * Build the array type from the inside out: `int a[2][3]` is an array of 2
     * arrays of 3 ints, so the last dimension is applied first.
     */
    if (node->array_dim_count > 0) {
        for (i = node->array_dim_count - 1; i >= 0; i--) {
            type = ty_array_of(type, node->array_dims[i]);
        }
    } else if (node->array_length > 0) {
        type = ty_array_of(type, node->array_length);
    }
    return type;
}

void sema_add_struct(struct sema_ctx *ctx, struct ast_node *node)
{
    struct ast_node *field;
    struct Type *struct_type;
    int index;

    if (sema_find_struct(ctx, node->value) >= 0) {
        semantic_error_at(ctx, node, "duplicate struct definition '%s'", node->value);
        return;
    }
    grow_array((void **)&ctx->structs, ctx->struct_count,
        &ctx->struct_capacity, sizeof(*ctx->structs), 64, "the symbol table");
    memset(&ctx->structs[ctx->struct_count], 0, sizeof(ctx->structs[0]));
    struct_type = ty_struct(node->value);
    node->ty = struct_type;
    ctx->structs[ctx->struct_count].name = node->value;
    ctx->structs[ctx->struct_count].field_count = 0;
    ctx->structs[ctx->struct_count].ty = struct_type;
    for (field = node->left; field; field = field->right) {
        struct ast_node *decl = field->left;
        int existing = sema_find_struct_field(ctx, ctx->struct_count, decl->value);
        index = ctx->structs[ctx->struct_count].field_count;
        if (existing >= 0) {
            semantic_error_at(ctx, decl, "duplicate field '%s'", decl->value);
            continue;
        }
        if (index >= 64) {
            semantic_error_at(ctx, decl, "too many fields in struct '%s'", node->value);
            break;
        }
        ctx->structs[ctx->struct_count].fields[index].name = decl->value;
        ctx->structs[ctx->struct_count].fields[index].type = decl->data_type;
        ctx->structs[ctx->struct_count].fields[index].pointer_depth = decl->pointer_depth;
        ctx->structs[ctx->struct_count].field_count++;
        ty_add_member(struct_type, decl->value, sema_resolve_type(ctx, decl));
    }

    /* Real offsets, padding, size, and alignment -- not field_index * 4. */
    ty_layout_struct(struct_type);
    for (index = 0; index < ctx->structs[ctx->struct_count].field_count; index++) {
        struct Member *member = ty_find_member(struct_type,
            ctx->structs[ctx->struct_count].fields[index].name);
        ctx->structs[ctx->struct_count].fields[index].offset = member ? member->offset : 0;
    }

    ctx->struct_count++;
}

int sema_find_global(struct sema_ctx *ctx, const char *name)
{
    int i;

    for (i = 0; i < ctx->global_count; i++) {
        if (strcmp(ctx->globals[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int sema_find_local(struct sema_ctx *ctx, const char *name)
{
    int i;

    for (i = ctx->local_count - 1; i >= 0; i--) {
        if (strcmp(ctx->locals[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void sema_add_global(struct sema_ctx *ctx, struct ast_node *node)
{
    const char *name = node->value;
    int is_function = node->type == AST_FUNCTION || node->type == AST_FUNCTION_DECL;
    int existing = sema_find_global(ctx, name);
    struct ast_node *param;

    if (existing >= 0) {
        /*
         * A prototype may be repeated, and may be followed by the definition.
         * Only two definitions of the same function are an error.
         */
        int both_functions = ctx->globals[existing].is_function && is_function;

        if (both_functions && node->type == AST_FUNCTION_DECL) {
            /* A later prototype adds nothing; keep the entry already made. */
            node->sym = ctx->globals[existing].sym;
            node->ty = sema_resolve_type(ctx, node);
            return;
        }
        if (both_functions && !ctx->globals[existing].is_defined) {
            /* The definition for a function that was only declared before. */
            ctx->globals[existing].is_defined = node->type == AST_FUNCTION;
            node->sym = ctx->globals[existing].sym;
            node->ty = sema_resolve_type(ctx, node);
            return;
        }
        semantic_error_at(ctx, node, "duplicate top-level declaration of '%s'", name);
        return;
    }
    grow_array((void **)&ctx->globals, ctx->global_count,
        &ctx->global_capacity, sizeof(*ctx->globals), 64, "the symbol table");
    /*
     * The table grows with realloc, so a fresh entry holds whatever was in that
     * memory. Clearing it means a field nobody sets here still reads as zero --
     * is_variadic was left uninitialised once, which made every function look
     * variadic on some platforms and not others.
     */
    memset(&ctx->globals[ctx->global_count], 0, sizeof(ctx->globals[0]));
    if (node->struct_name && sema_find_struct(ctx, node->struct_name) < 0) {
        semantic_error_at(ctx, node, "unknown struct type '%s'", node->struct_name);
        return;
    }

    node->ty = sema_resolve_type(ctx, node);
    if (!node->sym) {
        node->sym = sym_new(name, is_function ? SYM_FUNCTION : SYM_GLOBAL, node->ty);
    }
    ctx->globals[ctx->global_count].sym = node->sym;
    ctx->globals[ctx->global_count].name = name;
    ctx->globals[ctx->global_count].is_function = is_function;
    ctx->globals[ctx->global_count].is_defined = node->type == AST_FUNCTION;
    ctx->globals[ctx->global_count].type = node->data_type;
    ctx->globals[ctx->global_count].pointer_depth = node->pointer_depth;
    ctx->globals[ctx->global_count].array_length = node->array_length;
    memcpy(ctx->globals[ctx->global_count].array_dims, node->array_dims,
        sizeof(node->array_dims));
    ctx->globals[ctx->global_count].array_dim_count = node->array_dim_count;
    ctx->globals[ctx->global_count].struct_name = node->struct_name;
    ctx->globals[ctx->global_count].parameter_count = 0;
    if (is_function) {
        for (param = node->left; param; param = param->right) {
            if (param->left && param->left->value &&
                strcmp(param->left->value, "...") == 0) {
                ctx->globals[ctx->global_count].is_variadic = 1;
                continue;       /* not a parameter, just a marker */
            }
            if (ctx->globals[ctx->global_count].parameter_count >= 64) {
                semantic_error_at(ctx, node, "function '%s' has too many parameters", name);
                break;
            }
            param->left->ty = sema_resolve_type(ctx, param->left);
            ctx->globals[ctx->global_count].parameter_types[ctx->globals[ctx->global_count].parameter_count++] =
                param->left->data_type;
            ctx->globals[ctx->global_count].parameter_pointer_depths[ctx->globals[ctx->global_count].parameter_count - 1] =
                param->left->pointer_depth;
        }
    }
    ctx->global_count++;
}

void sema_add_local(struct sema_ctx *ctx, struct ast_node *node, CType type)
{
    const char *name = node->value;
    int existing = sema_find_local(ctx, name);

    /*
     * Redeclaring a name in the same scope is an error; redeclaring it in an
     * inner scope shadows the outer one. Shadowing works because each
     * declaration now gets its own Symbol, so the two variables have distinct
     * storage even though they share a name. sema_find_local scans innermost-first,
     * so references resolve to the nearest declaration.
     */
    if (existing >= 0 && ctx->locals[existing].depth == ctx->scope_depth) {
        semantic_error_at(ctx, node, "duplicate declaration of '%s'", name);
        return;
    }
    grow_array((void **)&ctx->locals, ctx->local_count,
        &ctx->local_capacity, sizeof(*ctx->locals), 64, "the symbol table");
    memset(&ctx->locals[ctx->local_count], 0, sizeof(ctx->locals[0]));
    if (node->struct_name && sema_find_struct(ctx, node->struct_name) < 0) {
        semantic_error_at(ctx, node, "unknown struct type '%s'", node->struct_name);
        return;
    }

    node->ty = sema_resolve_type(ctx, node);
    if (!node->sym) {
        struct Type *resolved = sema_resolve_type(ctx, node);
        int size = resolved->size > 0 ? resolved->size : 4;

        int align = resolved->align > 0 ? resolved->align : 4;

        if (align < 4) {
            align = 4;          /* never pack scalars tighter than a word */
        }

        node->sym = sym_new(name, SYM_LOCAL, resolved);
        /*
         * The frame grows downward from %rbp, so round the running total up to
         * the type's alignment before claiming the slot. An 8-byte pointer or
         * long must land on an 8-byte boundary.
         */
        ctx->frame_offset += size;
        ctx->frame_offset = (ctx->frame_offset + align - 1) / align * align;
        node->sym->offset = -ctx->frame_offset;
        if (ctx->frame_offset > ctx->frame_max) {
            ctx->frame_max = ctx->frame_offset;
        }
    }

    ctx->locals[ctx->local_count].sym = node->sym;
    ctx->locals[ctx->local_count].name = name;
    ctx->locals[ctx->local_count].type = type;
    ctx->locals[ctx->local_count].pointer_depth = node->pointer_depth;
    ctx->locals[ctx->local_count].array_length = node->array_length;
    memcpy(ctx->locals[ctx->local_count].array_dims, node->array_dims,
        sizeof(node->array_dims));
    ctx->locals[ctx->local_count].array_dim_count = node->array_dim_count;
    ctx->locals[ctx->local_count].is_function_pointer = node->is_function_pointer;
    ctx->locals[ctx->local_count].struct_name = node->struct_name;
    ctx->locals[ctx->local_count].depth = ctx->scope_depth;
    ctx->local_count++;
}

/*
 * Parameters are already on the stack when the function is entered, above the
 * saved frame pointer and return address, so they get positive offsets and do
 * not consume frame space.
 */
void sema_add_parameter(struct sema_ctx *ctx, struct ast_node *node, int index)
{
    if (!node->sym) {
        struct Type *resolved = sema_resolve_type(ctx, node);

        node->sym = sym_new(node->value, SYM_PARAM, resolved);
        node->sym->param_index = index;
        node->sym->float_index = ctx->float_param_count;
        node->sym->integer_index = ctx->integer_param_count;
        if (ty_is_float(resolved)) {
            ctx->float_param_count++;
        } else if (resolved->kind == TY_STRUCT) {
            /* One integer register per eightbyte of the struct. */
            ctx->integer_param_count += (resolved->size + 7) / 8;
        } else {
            ctx->integer_param_count++;
        }

        if (index < 6) {
            /*
             * Passed in a register: give it a frame slot for the prologue to
             * spill into, so it can be addressed like any other local.
             */
            int size = resolved->size > 0 ? resolved->size : 8;
            int align = resolved->align > 4 ? resolved->align : 4;

            ctx->frame_offset += size;
            ctx->frame_offset = (ctx->frame_offset + align - 1) / align * align;
            node->sym->offset = -ctx->frame_offset;
            if (ctx->frame_offset > ctx->frame_max) {
                ctx->frame_max = ctx->frame_offset;
            }
        } else {
            /* Already on the stack, above the saved %rbp and return address. */
            node->sym->offset = 16 + ((index - 6) * 8);
        }
    }
    sema_add_local(ctx, node, node->data_type);
}

void sema_enter_scope(struct sema_ctx *ctx)
{
    ctx->scope_depth++;
}


void sema_leave_scope(struct sema_ctx *ctx)
{
    int i;

    while (ctx->local_count > 0 && ctx->locals[ctx->local_count - 1].depth == ctx->scope_depth) {
        ctx->local_count--;
    }

    /*
     * Rewind the frame to what the still-live locals occupy, so two disjoint
     * blocks reuse the same stack slots instead of each claiming their own.
     * frame_max already recorded the deepest point reached.
     */
    ctx->frame_offset = 0;
    for (i = 0; i < ctx->local_count; i++) {
        struct Symbol *sym = ctx->locals[i].sym;

        if (sym && sym->kind == SYM_LOCAL && -sym->offset > ctx->frame_offset) {
            ctx->frame_offset = -sym->offset;
        }
    }

    ctx->scope_depth--;
}

void sema_analyze_expression(struct sema_ctx *ctx, struct ast_node *node);
void sema_analyze_statement(struct sema_ctx *ctx, struct ast_node *node);
