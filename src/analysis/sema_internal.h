#ifndef DONKEY_SEMA_INTERNAL_H
#define DONKEY_SEMA_INTERNAL_H

#include "defs.h"
#include "type.h"
#include "symbol.h"

struct global_symbol {
    const char *name;
    struct Symbol *sym;
    int array_dims[DONKEY_MAX_ARRAY_DIMS];
    int array_dim_count;
    int is_function;
    int is_defined;             /* a body was seen, not just a prototype */
    int is_variadic;            /* the parameter list ended with ... */
    CType type;
    int pointer_depth;
    int array_length;
    const char *struct_name;
    int parameter_count;
    CType parameter_types[64];
    int parameter_pointer_depths[64];
};

struct local_symbol {
    const char *name;
    CType type;
    int pointer_depth;
    int array_length;
    int array_dims[DONKEY_MAX_ARRAY_DIMS];
    int array_dim_count;
    int is_function_pointer;
    const char *struct_name;
    int depth;
    struct Symbol *sym;
};

struct struct_field {
    const char *name;
    CType type;
    int pointer_depth;
    int offset;
};

struct struct_symbol {
    const char *name;
    int field_count;
    struct struct_field fields[64];
    struct Type *ty;            /* resolved layout: offsets, size, alignment */
};

/*
 * All analysis state lives in one context that the caller owns, rather than in
 * file-scope arrays with fixed capacities. This removes the MAX_SYMBOLS ceiling,
 * makes the pass re-runnable in-process (needed for unit tests), and is a step
 * toward compiling several translation units.
 */
struct sema_ctx {
    struct global_symbol *globals;
    int global_count;
    int global_capacity;

    struct local_symbol *locals;
    int local_count;
    int local_capacity;

    struct struct_symbol *structs;
    int struct_count;
    int struct_capacity;

    int scope_depth;
    int loop_depth;
    int error_count;

    /*
     * Stack layout for the function being analysed. frame_offset is the bytes
     * used by the enclosing scopes; leaving a scope rewinds it so disjoint
     * blocks reuse the same slots. frame_max is the high-water mark, which is
     * what the function actually needs to reserve.
     */
    int frame_offset;
    int frame_max;

    /* Registers used by the parameters of the function being analysed. */
    int float_param_count;
    int integer_param_count;

    const char *current_function;
    CType current_return_type;
    int current_return_pointer_depth;
    const char *source_path;
};

/*
 * Shared between the semantic pass's four files.
 *
 * Analysis runs as two passes over the same tree. sema_resolve.c is the first:
 * it binds every name to a storage identity and lays out the frame.
 * sema_type.c is the second: it gives every expression a resolved type for the
 * code generator to read. sema_scope.c owns the tables both consult, and
 * semantic.c holds the diagnostics and the entry point.
 *
 * These declarations are the seam between them, and are internal to the pass:
 * nothing outside src/analysis includes this. The sema_ prefix is not
 * decoration -- find_global and initializer_items are also exported, with
 * different signatures, by the code generator.
 */

/* semantic.c -- diagnostics, type names, and the shared table lookups. */
const char *semantic_type_name(CType type);
void semantic_format_type(CType type, int pointer_depth, int array_length,
        char *buffer, size_t size);
int semantic_type_matches(CType left_type, int left_pointer_depth,
        CType right_type, int right_pointer_depth);
int semantic_is_integer(CType type, int pointer_depth, int array_length);
int semantic_effective_pointer_depth(struct ast_node *node);
int sema_find_struct(struct sema_ctx *ctx, const char *name);
int sema_find_struct_field(struct sema_ctx *ctx, int struct_index, const char *name);
CType semantic_type_from_name(const char *name);
void semantic_error_at(struct sema_ctx *ctx, struct ast_node *node, const char *format, ...);
int sema_count_list(struct ast_node *node, ASTNodeType list_type);
struct ast_node *sema_initializer_items(struct ast_node *node);

/* sema_scope.c -- the struct, global, local, and parameter tables. */
struct Type *sema_resolve_type(struct sema_ctx *ctx, struct ast_node *node);
void sema_add_struct(struct sema_ctx *ctx, struct ast_node *node);
int sema_find_global(struct sema_ctx *ctx, const char *name);
int sema_find_local(struct sema_ctx *ctx, const char *name);
void sema_add_global(struct sema_ctx *ctx, struct ast_node *node);
void sema_add_local(struct sema_ctx *ctx, struct ast_node *node, CType type);
void sema_add_parameter(struct sema_ctx *ctx, struct ast_node *node, int index);
void sema_enter_scope(struct sema_ctx *ctx);
void sema_leave_scope(struct sema_ctx *ctx);

/* sema_resolve.c -- pass one: names, scopes, and storage. */
void sema_analyze_expression(struct sema_ctx *ctx, struct ast_node *node);
void sema_analyze_statement(struct sema_ctx *ctx, struct ast_node *node);
void sema_collect_top_level(struct sema_ctx *ctx, struct ast_node *node);
void sema_analyze_top_level(struct sema_ctx *ctx, struct ast_node *node);

/* sema_type.c -- pass two: expression and statement types. */
void sema_check_top_level_types(struct sema_ctx *ctx, struct ast_node *node);

#endif
