#ifndef DONKEY_SYMBOL_H
#define DONKEY_SYMBOL_H

#include "type.h"

/*
 * A Symbol is the storage identity of a declaration.
 *
 * Semantic analysis creates one per declaration and hangs it on the AST, so
 * the code generator reads storage locations instead of looking names up in a
 * table of its own. Two declarations that share a name in different scopes get
 * different Symbols, which is what makes shadowing expressible: the name is no
 * longer the identity.
 */

typedef enum {
    SYM_LOCAL,
    SYM_PARAM,
    SYM_GLOBAL,
    SYM_FUNCTION
} SymbolKind;

struct Symbol {
    char *name;
    SymbolKind kind;
    struct Type *ty;

    /*
     * Frame offset relative to %ebp: negative for locals, positive for
     * parameters. Globals and functions are addressed by name instead.
     */
    int offset;

    /* SYM_FUNCTION: bytes of stack needed for this function's locals. */
    int frame_size;

    /*
     * SYM_PARAM: which argument this is. System V passes the first six integer
     * or pointer arguments in registers, so those are spilled into the frame on
     * entry; later ones already sit on the stack.
     */
    int param_index;
};

struct Symbol *sym_new(const char *name, SymbolKind kind, struct Type *ty);

/* Release every symbol allocated by sym_new. */
void sym_cleanup(void);

#endif
