/*
 * Code generator: the emitter context, function prologue and epilogue, and the
 * entry point that drives the rest.
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
#include "support/mem.h"


/*
 * Emitter state: label numbering, the active function's exit label, the
 * break/continue label stacks, and interned string literals.
 *
 * Codegen no longer keeps a symbol table. Storage locations, types, and frame
 * sizes are read from the symbols semantic analysis attached to the AST. The
 * globals array above remains only as the list of static data to emit.
 */


/* A goto target: the source name paired with the emitted label number. */









/* ------------------------------------------------------ floating point -- */

/*
 * Floating-point values live in %xmm0, not %rax, so they need a parallel set
 * of moves. The suffix follows the width: ss for a 4-byte float, sd for an
 * 8-byte double. Everything else about the stack machine is unchanged --
 * intermediate values are pushed and popped, just through %xmm registers.
 */


/* Interned floating-point constants, emitted into .data and loaded from there. */


/* Push and pop the floating-point accumulator, mirroring pushq/popq. */


/* Convert whatever is in %eax to floating point in %xmm0. */


/* Convert %xmm0 to an integer in %eax, truncating as C requires. */


/*
 * Access width follows the type. A 4-byte movl for a char element would read
 * past it and, on a store, clobber its neighbours; an 8-byte pointer or long
 * needs movq. Values live in %rax, so 4-byte and narrower loads name %eax and
 * let the hardware zero the upper half.
 */


/* Store the accumulator into a frame slot, using the type's width. */


/* Address in %rax, value in %rdx. */


/*
 * A scalar local owns a whole slot -- at least a word, eight bytes for a
 * pointer or long -- and is read back at that width, so writes must fill it.
 * The narrow stores above are only for packed array elements.
 */


/* Load a frame slot into the accumulator at the type's width. */


/*
 * ++ and -- step a pointer by one element and everything else by one. On a
 * pointer the step is 64-bit address arithmetic, and the element size comes
 * from the type rather than being assumed to be a word.
 */


/* Compare the two operands at the wider of their widths. */


/*
 * Labels are gathered before the function body is emitted: a goto may target a
 * label further down, so the number has to exist before the jump is written.
 */


/*
 * Emit the comparisons for a switch.
 *
 * Each case is tested against the control value in turn and jumps to its own
 * label. That is a chain of compares rather than a jump table -- correct for
 * any set of case values, including sparse ones, and the shape an optimiser
 * would later turn into a table where the values are dense.
 */


/*
 * ++ and -- on something that is not a plain variable: an array element or a
 * struct field. The address is computed once and kept, so the operand is not
 * evaluated twice -- which would be wrong the moment the subscript had a side
 * effect. The result is the new value for a prefix operator and the old one
 * for a postfix operator.
 */


/*
 * Copy a struct from one place to another.
 *
 * A struct is wider than a register, so assigning one is a block copy rather
 * than a move: the destination address is in %rax and the source in %rdx, and
 * the bytes are moved in the largest chunks that fit. Sizes here are small and
 * known at compile time, so the copy is unrolled rather than looped.
 */


/*
 * Storage comes straight off the symbol semantic analysis attached to the
 * node. There is no name lookup here any more, so an unresolved name cannot
 * reach the code generator: it is prevented by construction rather than by a
 * runtime check.
 */


/*
 * Fill an object at a frame offset from a brace initializer. This is the same
 * work a declaration with an initializer does, factored out so a compound
 * literal -- which has storage but no declaration -- can reuse it.
 */


/*
 * sizeof yields the operand's real size: sizeof(char[10]) is 10, not 4 and not
 * 40. A named type measures that type; an expression measures its resolved
 * type without being evaluated.
 */


/*
 * A cast that crosses between integer and floating point is a conversion
 * instruction, not a narrowing move: the value changes representation as well
 * as width. The node's own type says what it is being converted to, and the
 * operand's says what from.
 */


/*
 * Floating constants are written after the code, not before it. SSE cannot
 * take an immediate, so each one becomes a labelled datum -- but which ones
 * exist is only known once every function body has been generated.
 */


/*
 * Tell the linker the program does not need an executable stack. Without this
 * note GNU ld assumes it might, marks the stack executable, and warns -- which
 * is both a security regression and noise on every link.
 */



/* Spill an incoming register argument into the frame slot it was given. */

void emit_spill_parameter(struct Symbol *sym, FILE *output)
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

void generate_function(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
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

void generate_program(struct cg_ctx *ctx, struct ast_node *node, FILE *output)
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


/*
 * Floating arithmetic. Both operands are evaluated into %xmm0 in turn, the
 * first parked on the stack, so the shape matches the integer stack machine.
 * The left operand ends up in %xmm1 and the right in %xmm0, which is the wrong
 * way round for the non-commutative operators -- hence the swap.
 */


/*
 * How many argument slots a value occupies. System V splits a struct into
 * eightbytes and classifies each: with no floating fields every one is INTEGER,
 * so a struct of up to sixteen bytes takes one register per eightbyte and
 * everything else takes exactly one.
 */

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
