#ifndef DONKEY_OPT_H
#define DONKEY_OPT_H

#include <stdio.h>
#include "ir.h"

/*
 * Optimisation passes over the SSA IR.
 *
 * Each pass rewrites the IR into a cheaper form that computes the same thing,
 * and reports how many changes it made. That count is what drives the pipeline:
 * passes expose work for each other -- folding a comparison makes a branch
 * constant, deleting the branch makes a block unreachable, deleting the block
 * makes a phi trivial, and removing the phi may make another value constant --
 * so the pipeline runs until nothing changes rather than in a fixed order once.
 *
 * SSA is what makes most of these short. A value is its definition, so constant
 * propagation is not a separate pass at all: folding an instruction to a
 * constant means every use is already looking at that constant. Dead code
 * elimination is a use count. Common subexpressions are equal operand pointers.
 * None of this needed a dataflow framework, because the form is the framework.
 *
 * Every pass preserves SSA and the CFG invariants, and the driver runs the
 * verifier between passes when asked, so a pass that breaks one is named rather
 * than blamed on whatever runs next.
 */

struct opt_stats {
    /* Constants, algebraic identities and strength reductions: one pass. */
    int folded;
    int cfg;                    /* branches straightened, blocks merged */
    int copies;                 /* copies and trivial phis propagated away */
    int cse;                    /* redundant computations removed */
    int hoisted;                /* loop-invariant instructions moved out */
    int dead;                   /* instructions removed as unused */
    int passes;                 /* how many times the pipeline went round */
};

/*
 * Run the pipeline for an optimisation level. Level 0 does nothing at all, so
 * -O0 is not "the same passes with the flags off" but genuinely the unoptimised
 * IR, which is what makes it useful for comparison.
 */
void opt_run(struct ir_func *func, int level, struct opt_stats *stats,
    FILE *verify_output);

void opt_report(const struct opt_stats *stats, const char *name, FILE *output);

/*
 * The individual passes. Exposed so each can be tested on its own: a pipeline
 * that ends up in the right place says nothing about which pass got it there.
 * Each returns the number of changes it made.
 */

/* Constant folding, algebraic identities, and strength reduction. */
int opt_fold(struct ir_func *func);

/*
 * Branches on a constant become jumps, blocks that nothing reaches go away,
 * and a block with one predecessor that has one successor is merged into it.
 * Requires the CFG and dominators to be current, and leaves them stale.
 */
int opt_simplify_cfg(struct ir_func *func);

/* Copies, and phis whose arguments all agree, are replaced by their value. */
int opt_propagate_copies(struct ir_func *func);

/* Remove pure instructions nothing reads. */
int opt_dce(struct ir_func *func);

/*
 * Global value numbering: a pure instruction whose operands and opcode match
 * one already computed in a dominating block is replaced by it. Needs the
 * dominator tree.
 */
int opt_cse(struct ir_func *func);

/*
 * Move computations that do not change within a loop to the block before it.
 * Needs the dominator tree. Only operations that cannot fault are moved: a
 * loop may run zero times, and hoisting a division out of one would introduce
 * a trap the original program never had.
 */
int opt_licm(struct ir_func *func);

#endif
