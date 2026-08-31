#ifndef DONKEY_CODEGEN_INTERNAL_H
#define DONKEY_CODEGEN_INTERNAL_H

#include "defs.h"
#include "type.h"
#include "symbol.h"

/* An interned floating constant, emitted as a labelled datum. */
struct cg_double {
    char *text;
    int is_float;
    int label;
};

/*
 * Static data waiting to be emitted. Collected while walking the tree and
 * written out once the code is done, so codegen_data.c fills it and codegen.c
 * emits it.
 */
struct cg_global {
    char *name;
    int array_length;
    struct Type *ty;
    int values[256];
};

extern struct cg_global globals[256];
extern int global_count;
extern struct cg_double *fp_constants;
extern int fp_constant_count;
extern int fp_constant_capacity;

/*
 * Shared between the code generator's five files.
 *
 * codegen.c holds the emitter context, the function prologue and epilogue, and
 * the entry point. codegen_emit.c is the instruction layer -- one function per
 * thing the machine can be told to do. codegen_data.c handles what goes in
 * .data: globals, string literals, floating constants, and the constant
 * folding their initializers need. codegen_stmt.c and codegen_expr.c walk the
 * two halves of the tree.
 *
 * These declarations are the seam between them, and are internal to the code
 * generator: nothing outside src/backend includes this.
 */

struct cg_string {
    char *value;
    int label;
};

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

void add_global_node(struct ast_node *node);
int add_string_literal(struct cg_ctx *ctx, const char *value);
int arg_slots(struct ast_node *value);
int cast_constant(int value, const char *type);
const char *codegen_type_name(CType type);
void collect_globals(struct ast_node *node);
void collect_labels(struct cg_ctx *ctx, struct ast_node *node);
void collect_metadata(struct cg_ctx *ctx, struct ast_node *node);
int count_arg_slots(struct ast_node *node);
int count_args(struct ast_node *node);
void emit_compare(struct Type *left, struct Type *right, FILE *output);
void emit_fp_load_frame(struct Type *ty, int offset, FILE *output);
void emit_fp_pop(struct Type *ty, const char *reg, FILE *output);
void emit_fp_push(struct Type *ty, FILE *output);
void emit_fp_store_frame(struct Type *ty, int offset, FILE *output);
void emit_fp_to_int(struct Type *source, FILE *output);
void emit_fp_widen(struct Type *from, struct Type *to, FILE *output);
void emit_int_to_fp(struct Type *target, FILE *output);
void emit_load_frame(struct Type *ty, int offset, FILE *output);
void emit_load_global(struct Type *ty, const char *name, FILE *output);
void emit_load_indirect(struct Type *ty, FILE *output);
void emit_spill_parameter(struct Symbol *sym, FILE *output);
void emit_step(struct Type *ty, int is_increment, FILE *output);
void emit_store_global(struct Type *ty, const char *name, FILE *output);
void emit_store_indirect(struct Type *ty, FILE *output);
void emit_store_offset(struct Type *ty, int offset, FILE *output);
void emit_store_slot(struct Type *ty, int offset, FILE *output);
void emit_struct_copy(int size, FILE *output);
void emit_switch_tests(struct cg_ctx *ctx, struct ast_node *node,
    int *default_label, FILE *output);
void emit_zero_offset(struct Type *ty, int offset, FILE *output);
void emit_zero_slot(struct Type *ty, int offset, FILE *output);
int eval_const_exp(struct ast_node *node);
int find_global(const char *name);
const char *fp_suffix(struct Type *ty);
void free_fp_constants(void);
void free_labels(struct cg_ctx *ctx);
void generate_binop(struct cg_ctx *ctx, struct ast_node *node, FILE *output);
int generate_call_args(struct cg_ctx *ctx, struct ast_node *node, FILE *output);
void generate_cast(const char *type, FILE *output);
void generate_cast_between(struct Type *from, struct Type *to, FILE *output);
void generate_epilogue(FILE *output);
void generate_exp(struct cg_ctx *ctx, struct ast_node *node, FILE *output);
int generate_float_binop(struct cg_ctx *ctx, struct ast_node *node,
    FILE *output);
void generate_fp_constants(FILE *output);
void generate_function(struct cg_ctx *ctx, struct ast_node *node, FILE *output);
void generate_globals(struct cg_ctx *ctx, FILE *output);
void generate_identifier_load(struct ast_node *node, FILE *output);
void generate_identifier_store(struct ast_node *node, FILE *output);
void generate_incdec_lvalue(struct cg_ctx *ctx, struct ast_node *node,
    int is_increment, int is_prefix, FILE *output);
void generate_initializer_into(struct cg_ctx *ctx, struct Type *ty,
    int offset, struct ast_node *initializer, FILE *output);
void generate_lvalue_address(struct cg_ctx *ctx, struct ast_node *node, FILE *output);
void generate_program(struct cg_ctx *ctx, struct ast_node *node, FILE *output);
void generate_stack_note(FILE *output);
void generate_statement(struct cg_ctx *ctx, struct ast_node *node, FILE *output);
struct ast_node *initializer_items(struct ast_node *node);
int intern_fp_constant(const char *text, int is_float);
int is_frame_symbol(struct Symbol *sym);
int is_unsigned_type(CType type);
int label_for_name(struct cg_ctx *ctx, const char *name);
void pop_loop(struct cg_ctx *ctx);
void push_loop(struct cg_ctx *ctx, int break_label, int continue_label);
int sizeof_node(struct ast_node *node);
int struct_field_offset(struct Type *ty, const char *field_name);

/*
 * System V passes the first six integer or pointer arguments in these
 * registers, in this order. The call sequence fills them and the prologue
 * spills them, so both files need the names.
 */
extern const char *arg_reg64[6];
extern const char *arg_reg32[6];
extern const char *arg_reg16[6];
extern const char *arg_reg8[6];

#endif
