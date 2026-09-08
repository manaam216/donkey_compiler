#ifndef DONKEY_IR_H
#define DONKEY_IR_H

#include <stdio.h>
#include "defs.h"
#include "type.h"
#include "symbol.h"

/*
 * A three-address intermediate representation in static single assignment
 * form.
 *
 * Until now the code generator walked the syntax tree and emitted instructions
 * as it went, which meant every decision had to be made with only one node in
 * view. An optimiser cannot work that way: constant propagation needs to know
 * what reached a value, dead code elimination needs to know whether anything
 * reads it, and neither question can be asked of a tree being consumed
 * depth-first. So the tree is lowered into a linear form first.
 *
 * Three things make that form useful:
 *
 *   Three-address code. Every instruction computes one value from operands
 *   that are already values. There are no nested expressions left, so an
 *   instruction can be examined, moved, or deleted on its own.
 *
 *   A control flow graph. Straight-line runs of instructions are grouped into
 *   basic blocks, and the branches between them are edges. Questions about
 *   what can run before what become graph reachability.
 *
 *   Static single assignment. Every value is written exactly once, so a value
 *   *is* its definition: to find what produced an operand you follow the
 *   pointer, with no need to scan backwards for the last write that reached
 *   it. Where control flow merges and two definitions could arrive, a phi
 *   names both.
 *
 * Lowering does not build SSA directly. Every local variable becomes a frame
 * slot (IR_ALLOCA) that is read and written through memory, which is always
 * correct and needs no analysis. A separate pass then promotes the slots that
 * never have their address taken into values, inserting phis where the
 * dominance frontier says a merge needs one. Splitting it this way means the
 * hard part -- deciding where a phi belongs -- lives in one algorithm rather
 * than being smeared across the lowering of every statement.
 */

typedef enum {
    IR_NOP,

    /* Values that come from nowhere but themselves. */
    IR_CONST,           /* imm */
    IR_CONST_FP,        /* text: the constant as written */
    IR_STR,             /* text: string literal; the value is its address */
    IR_GLOBAL,          /* sym: the value is the global's address */
    IR_ALLOCA,          /* sym: the value is a frame slot's address */
    IR_PARAM,           /* sym: the incoming argument */

    /* Memory. */
    IR_LOAD,            /* args[0] is an address */
    IR_STORE,           /* args[0] is an address, args[1] the value */
    IR_COPY,            /* args[0]; a register move, not a memory one */
    IR_MEMCPY,          /* args[0] destination, args[1] source, imm bytes */

    /* Arithmetic and logic, two operands unless noted. */
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_MOD,
    IR_SHL,
    IR_SHR,
    IR_AND,
    IR_OR,
    IR_XOR,
    IR_NEG,             /* one operand */
    IR_COMPLEMENT,      /* one operand */
    IR_LOGNOT,          /* one operand */

    /* Comparisons, each producing 0 or 1. */
    IR_EQ,
    IR_NE,
    IR_LT,
    IR_LE,
    IR_GT,
    IR_GE,

    /*
     * A conversion between types. The source type is the operand's, the target
     * is the result's, so no extra field is needed to say what is being
     * converted to what.
     */
    IR_CAST,

    /*
     * A call. args[0] is the callee -- a function address, so a call through a
     * pointer is the same instruction with a different operand -- and the rest
     * are the arguments in source order. Assigning registers to them is the
     * back end's job, not the IR's.
     */
    IR_CALL,

    /* Terminators. Every block ends with exactly one. */
    IR_JMP,             /* target */
    IR_BR,              /* args[0] is the condition; then_block, else_block */
    IR_RET,             /* args[0] if the function returns a value */

    /*
     * A merge: one argument per incoming edge. phi_blocks[i] names the block
     * args[i] arrives from. The pairing is by that name rather than by
     * position -- SSA construction fills phis in dominator-tree order, which
     * is not the order predecessors were recorded in -- so the invariant is
     * that the two sets match, not that the two arrays line up.
     */
    IR_PHI,

    IR_OP_COUNT
} IROp;

struct ir_instr;
struct ir_block;
struct ir_func;

/*
 * A value: the result of one instruction. Defined once, used anywhere it is
 * dominated by its definition, and referred to by pointer -- so rewriting a
 * use is a pointer store, not a search.
 */
struct ir_value {
    int id;                     /* %id in the dump; unique within a function */
    struct Type *ty;
    struct ir_instr *def;       /* the instruction that produces it */
};

struct ir_instr {
    IROp op;

    /* NULL for instructions that produce nothing: stores, branches, void calls. */
    struct ir_value *dst;

    struct ir_value **args;
    int arg_count;
    int arg_capacity;

    /*
     * IR_PHI only: the block each argument arrives from, parallel to args. A
     * phi has to name the edge and not just the value, because the same value
     * can arrive along two different edges.
     */
    struct ir_block **phi_blocks;

    long imm;                   /* IR_CONST, IR_MEMCPY size */
    char *text;                 /* IR_CONST_FP, IR_STR */
    struct Symbol *sym;         /* IR_GLOBAL, IR_ALLOCA, IR_PARAM */
    struct Type *mem_type;      /* IR_LOAD, IR_STORE: the type in memory */

    struct ir_block *target;                    /* IR_JMP */
    struct ir_block *then_block, *else_block;   /* IR_BR */

    SourceLocation location;

    struct ir_block *block;     /* the block holding this instruction */
    struct ir_instr *prev, *next;
};

struct ir_block {
    int id;
    char *label;                /* a source label, when there was one */

    struct ir_instr *first, *last;

    /*
     * Instructions unlinked by a pass. Their result values may still be
     * referenced while a rewrite is in progress, so they are kept until the
     * whole function is freed rather than released at the point of removal.
     */
    struct ir_instr *dead;

    /*
     * Predecessors are stored; successors are read back out of the terminator.
     * Keeping one direction derived means an edge cannot be recorded in one
     * place and forgotten in the other.
     */
    struct ir_block **preds;
    int pred_count;
    int pred_capacity;

    /* Filled in by ir_analyze_cfg and ir_compute_dominators. */
    int rpo_index;              /* reverse postorder position; -1 if unreachable */
    struct ir_block *idom;      /* immediate dominator; NULL for the entry */
    struct ir_block **frontier;
    int frontier_count;
    int frontier_capacity;

    /* Scratch for the passes: a generation stamp, cheaper than clearing marks. */
    int visited;

    struct ir_block *next;      /* layout order within the function */
};

struct ir_func {
    char *name;
    struct Symbol *sym;
    struct Type *return_type;

    struct ir_block *entry;
    struct ir_block *last_block;
    int block_count;

    /*
     * Every value the function has ever created, so the dump can be read in a
     * stable order and everything can be freed without walking the graph.
     */
    struct ir_value **values;
    int value_count;
    int value_capacity;

    /* Blocks in reverse postorder, recomputed by ir_analyze_cfg. */
    struct ir_block **rpo;
    int rpo_count;

    int is_variadic;

    struct ir_func *next;
};

struct ir_program {
    struct ir_func *first;
    struct ir_func *last;
};

/* ------------------------------------------------------------ building -- */

struct ir_func *ir_func_new(struct ir_program *program, const char *name,
    struct Symbol *sym);
struct ir_block *ir_block_new(struct ir_func *func, const char *label);
struct ir_value *ir_value_new(struct ir_func *func, struct Type *ty);

/*
 * Append an instruction to a block. The builder returns the instruction so the
 * caller can fill in what only it knows -- a branch target that does not exist
 * yet, the members of a phi -- without a separate setter for each field.
 *
 * Produce an instruction's operands before emitting it, not while filling them
 * in afterwards. `ir_add_arg(ir_emit(...), lower(...))` reads naturally and is
 * wrong: whatever computes the operand is appended after the instruction that
 * consumes it, and after a terminator it is never reached at all. The verifier
 * catches it, but the shape is easy to write by accident.
 */
struct ir_instr *ir_emit(struct ir_block *block, IROp op);

/*
 * Emit at the top of a block instead of the bottom. Allocas go here: they are
 * created wherever the declaration appears, but the entry block may already
 * have been given its terminator by then, and nothing may follow one.
 */
struct ir_instr *ir_emit_front(struct ir_block *block, IROp op);
void ir_add_arg(struct ir_instr *instr, struct ir_value *value);
void ir_add_phi_arg(struct ir_instr *instr, struct ir_value *value,
    struct ir_block *from);

/* Give an instruction a result value of the given type, and return it. */
struct ir_value *ir_set_dst(struct ir_func *func, struct ir_instr *instr,
    struct Type *ty);

void ir_remove(struct ir_instr *instr);

/* The terminator of a block, or NULL while it is still being built. */
struct ir_instr *ir_terminator(struct ir_block *block);
int ir_is_terminator(IROp op);

/* Successors of a block, read out of its terminator. Returns how many. */
int ir_successors(struct ir_block *block, struct ir_block *out[2]);

void ir_program_free(struct ir_program *program);

/* --------------------------------------------------------------- passes -- */

/*
 * Lower an analysed syntax tree. Requires semantic analysis to have run: the
 * types and symbols it attached are what the IR is built from.
 */
void ir_lower_program(struct ir_program *program, struct ast_node *ast);

/*
 * Recompute predecessors and reverse postorder, and drop blocks that nothing
 * can reach. Every later pass assumes this has run.
 */
void ir_analyze_cfg(struct ir_func *func);

/* Immediate dominators, then dominance frontiers. Needs ir_analyze_cfg. */
void ir_compute_dominators(struct ir_func *func);
void ir_compute_frontiers(struct ir_func *func);
int ir_dominates(struct ir_block *a, struct ir_block *b);

/*
 * Promote frame slots to values, inserting phis where control flow merges.
 * A slot whose address escapes is left alone: something else may write it, so
 * its definitions are not all visible here.
 */
void ir_build_ssa(struct ir_func *func);

/*
 * Check the invariants the passes rely on. Returns the number of problems
 * found and reports each one; zero means the IR is well formed.
 */
int ir_verify(struct ir_func *func, FILE *output);
int ir_verify_program(struct ir_program *program, FILE *output);

/* ---------------------------------------------------------------- dump -- */

void ir_dump_program(struct ir_program *program, FILE *output);
void ir_dump_func(struct ir_func *func, FILE *output);
const char *ir_op_name(IROp op);

#endif
