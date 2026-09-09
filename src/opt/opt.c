/*
 * The pass pipeline.
 *
 * The passes are not independent: each one exposes work for the others.
 * Folding a comparison makes a branch constant, straightening the branch makes
 * a block unreachable, dropping the block makes a phi trivial, replacing the
 * phi makes another expression foldable. Running them once in a fixed order
 * would stop partway through that chain, so the pipeline goes round until a
 * full round changes nothing.
 *
 * It is bounded as well, because "until nothing changes" is only a termination
 * argument if every pass strictly shrinks something -- and while each of these
 * does, a bug in one that made it undo another's work would hang the compiler
 * rather than produce a wrong answer. A round limit turns that into a slightly
 * less optimised program.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opt.h"

/*
 * The bound on rounds.
 *
 * What sets the count is chain length, not function size. A run of `&&`
 * operators over constants advances about a link per round: folding one
 * condition makes its branch constant, straightening the branch makes the next
 * phi trivial, and only then can the condition after it fold. Nine of them in
 * one expression takes seventeen rounds.
 *
 * Thirty-two covers everything the examples contain with room over. Hitting the
 * cap is not a failure -- the IR is valid at every round boundary, so the
 * result is a correct program that was optimised a little less than it could
 * have been.
 */
#define OPT_MAX_ROUNDS 32

/*
 * The analyses the passes read. Anything that changes the shape of the graph
 * invalidates them, so they are recomputed rather than patched: the cost is a
 * couple of walks, and a stale dominator tree is a wrong answer rather than a
 * slow one.
 */
static void refresh(struct ir_func *func)
{
    ir_analyze_cfg(func);
    ir_compute_dominators(func);
    ir_compute_frontiers(func);
}

void opt_run(struct ir_func *func, int level, struct opt_stats *stats,
    FILE *verify_output)
{
    int round;

    memset(stats, 0, sizeof(*stats));
    if (level <= 0) {
        return;
    }

    for (round = 0; round < OPT_MAX_ROUNDS; round++) {
        int changed = 0;
        int count;

        refresh(func);
        stats->passes++;

        count = opt_fold(func);
        stats->folded += count;
        changed += count;

        count = opt_propagate_copies(func);
        stats->copies += count;
        changed += count;

        count = opt_dce(func);
        stats->dead += count;
        changed += count;

        if (level >= 2) {
            refresh(func);
            count = opt_cse(func);
            stats->cse += count;
            changed += count;
        }

        if (level >= 3) {
            refresh(func);
            count = opt_licm(func);
            stats->hoisted += count;
            changed += count;
        }

        /*
         * After the others. Tail call elimination needs the frame slots gone,
         * and they are gone only once promotion and dead code removal have had
         * a turn -- so on the first round there is usually nothing to match,
         * and on the second there is.
         */
        refresh(func);
        count = opt_tail_calls(func);
        stats->tails += count;
        changed += count;

        /* Leaves the CFG and the phis consistent, so nothing follows it. */
        count = opt_simplify_cfg(func);
        stats->cfg += count;
        changed += count;

        /*
         * Checked every round rather than once at the end, so a pass that
         * breaks an invariant is caught while it is still obvious which round
         * it happened in.
         *
         * Refreshed first. The check that a definition dominates its use reads
         * the dominator tree, and the passes above have just moved blocks
         * around -- so without this the verifier would be answering a question
         * about the shape the function used to have.
         */
        if (verify_output) {
            refresh(func);
            if (ir_verify(func, verify_output) > 0) {
                return;
            }
        }

        if (!changed) {
            break;
        }
    }

    /*
     * The dominator tree is left current: whatever reads the IR next -- the
     * dump, the verifier, eventually instruction selection -- should not have
     * to know whether an optimiser ran.
     */
    refresh(func);
}

/*
 * Inlining first, then everything else on what it exposed. It runs once rather
 * than in the round loop: it is the only pass that makes the program bigger,
 * and letting it back in every round would let a chain of small callees grow
 * without a bound that is easy to reason about.
 */
void opt_run_program(struct ir_program *program, int level,
    struct opt_stats *stats, FILE *verify_output)
{
    struct ir_func *func;

    memset(stats, 0, sizeof(*stats));
    if (level <= 0) {
        return;
    }

    stats->inlined = opt_inline(program, level);

    for (func = program->first; func; func = func->next) {
        struct opt_stats one;

        opt_run(func, level, &one, verify_output);

        stats->folded += one.folded;
        stats->cfg += one.cfg;
        stats->copies += one.copies;
        stats->cse += one.cse;
        stats->hoisted += one.hoisted;
        stats->dead += one.dead;
        stats->tails += one.tails;
        if (one.passes > stats->passes) {
            stats->passes = one.passes;
        }
    }
}

void opt_report(const struct opt_stats *stats, const char *name, FILE *output)
{
    fprintf(output,
        "%s: %d round(s); inlined %d, tails %d, folded %d, copies %d, "
        "cse %d, hoisted %d, dead %d, cfg %d\n",
        name, stats->passes, stats->inlined, stats->tails, stats->folded,
        stats->copies, stats->cse, stats->hoisted, stats->dead, stats->cfg);
}
