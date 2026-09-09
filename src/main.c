#include <stdio.h>
#include <stdlib.h>
#include "defs.h"
#include "decl.h"
#include "type.h"
#include "symbol.h"
#include "diag.h"
#include "dump.h"
#include "cli.h"
#include "preprocess.h"
#include "ir.h"
#include "opt.h"

/*
 * Each stage reports everything it finds rather than stopping at the first
 * problem, so one run can surface several. Compilation stops between stages,
 * where it is safe to: there is no point type-checking a tree the parser could
 * not build, or generating code for one that failed to type-check.
 */
/*
 * Print the preprocessed token stream for -E. Original spacing is gone by this
 * point, so tokens are laid back out one line per source line -- enough to read
 * the result and diff it against another preprocessor.
 */
static void print_preprocessed(const struct token *tokens, int token_count)
{
    int i;

    for (i = 0; i < token_count; i++) {
        if (tokens[i].type == T_EOF) {
            break;
        }
        if (i > 0 && tokens[i].at_line_start) {
            printf("\n");
        } else if (i > 0) {
            printf(" ");
        }
        printf("%s", tokens[i].value ? tokens[i].value : "");
    }
    printf("\n");
}

int main(int argc, char *argv[])
{
    struct options options;
    struct pp_options pp_options;
    int ir_built = 0;
    struct token *tokens = NULL;
    int token_count = 0;
    int token_index = 0;
    struct ast_node *ast = NULL;
    struct ir_program ir;
    int should_exit = 0;
    int status;

    status = cli_parse(argc, argv, &options, &should_exit);
    if (should_exit) {
        return status;
    }

    diag_init(options.input);
    diag_set_warnings_are_errors(options.warnings_are_errors);
    diag_set_warnings_suppressed(options.suppress_warnings);

    if (options.verbose) {
        fprintf(stderr, "preprocessing %s\n", options.input);
    }

    pp_options.include_paths = options.include_paths;
    pp_options.include_path_count = options.include_path_count;
    pp_options.defines = options.defines;
    pp_options.define_count = options.define_count;
    preprocess_file(options.input, &pp_options, &tokens, &token_count);

    /*
     * -E stops here: the point is to see what the preprocessor produced, which
     * is worth having even when the result would not go on to parse.
     */
    if (options.preprocess_only) {
        print_preprocessed(tokens, token_count);
        status = diag_has_errors() ? EXIT_FAILURE : EXIT_SUCCESS;
        goto done_tokens;
    }

    if (options.dump_tokens) {
        dump_tokens(tokens, token_count);
        status = diag_has_errors() ? EXIT_FAILURE : EXIT_SUCCESS;
        goto done_tokens;
    }

    /*
     * A token stream with holes in it would send the parser down paths its
     * error recovery was not written for, and every message after that would
     * be a consequence of the first.
     */
    if (diag_has_errors()) {
        status = EXIT_FAILURE;
        goto done_tokens;
    }

    if (options.verbose) {
        fprintf(stderr, "parsing\n");
    }
    ast = parse_program(tokens, &token_index, options.input);

    if (diag_has_errors()) {
        status = EXIT_FAILURE;
        goto done_ast;
    }

    if (options.verbose) {
        fprintf(stderr, "analysing\n");
    }
    if (!semantic_analyze(ast, options.input) || diag_has_errors()) {
        status = EXIT_FAILURE;
        /* Still dump if asked: a partly annotated tree is what you want to see. */
        if (options.dump_ast) {
            dump_ast(ast);
        }
        goto done_ast;
    }

    /* Dumped after analysis, so the tree carries its types and storage. */
    if (options.dump_ast) {
        dump_ast(ast);
        status = EXIT_SUCCESS;
        goto done_ast;
    }

    /*
     * The IR is built on request rather than on every compile. Code generation
     * still runs straight off the syntax tree; until instruction selection
     * moves onto the IR, building it unconditionally would cost every user of
     * the compiler time for a result nothing reads.
     */
    if (options.dump_ir || options.dump_ssa) {
        struct ir_func *func;

        if (options.verbose) {
            fprintf(stderr, "lowering to IR\n");
        }
        ir.first = NULL;
        ir.last = NULL;
        ir_built = 1;
        ir_lower_program(&ir, ast);

        for (func = ir.first; func; func = func->next) {
            struct opt_stats stats;

            ir_analyze_cfg(func);
            ir_compute_dominators(func);
            ir_compute_frontiers(func);
            if (!options.dump_ssa) {
                continue;
            }

            ir_build_ssa(func);

            /*
             * Promotion deletes blocks' worth of loads and stores and adds
             * phis, so the predecessor lists and the dominator tree are
             * rebuilt before anything reads them again.
             */
            ir_analyze_cfg(func);
            ir_compute_dominators(func);
            ir_compute_frontiers(func);

            /*
             * The optimiser needs SSA, so -O has no effect on --dump-ir: what
             * that prints is what lowering produced, which is the thing worth
             * being able to look at unchanged.
             */
            opt_run(func, options.optimise, &stats, stderr);
            if (options.verbose && options.optimise > 0) {
                opt_report(&stats, func->name, stderr);
            }
        }

        if (ir_verify_program(&ir, stderr) > 0) {
            status = EXIT_FAILURE;
            goto done_ast;
        }
        ir_dump_program(&ir, stdout);
        status = EXIT_SUCCESS;
        goto done_ast;
    }

    if (options.verbose) {
        fprintf(stderr, "generating %s\n", options.output);
    }
    write_assembly_to_file(options.output, ast);

    if (diag_has_errors()) {
        status = EXIT_FAILURE;
        goto done_ast;
    }

    printf("Compiled %s -> %s\n", options.input, options.output);
    status = EXIT_SUCCESS;

done_ast:
    if (ir_built) {
        ir_program_free(&ir);
    }
    free_ast_node(ast);
done_tokens:
    free_tokens(tokens, token_count);
    ty_cleanup();
    sym_cleanup();
    preprocess_free();
    parser_reset_typedefs();
    parser_reset_enums();

    if (status != EXIT_SUCCESS && diag_error_count() > 1) {
        fprintf(stderr, "%d errors\n", diag_error_count());
    }
    diag_cleanup();
    return status;
}
