#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol.h"

/*
 * Symbols outlive semantic analysis -- the code generator reads them off the
 * AST -- so they are tracked here and released once, at the end of the run.
 */
struct symbol_entry {
    struct Symbol *symbol;
    struct symbol_entry *next;
};

static struct symbol_entry *allocated_symbols;

struct Symbol *sym_new(const char *name, SymbolKind kind, struct Type *ty)
{
    struct Symbol *symbol = calloc(1, sizeof(*symbol));
    struct symbol_entry *entry = malloc(sizeof(*entry));

    if (!symbol || !entry) {
        fprintf(stderr, "Out of memory while allocating a symbol\n");
        exit(EXIT_FAILURE);
    }

    symbol->name = name ? strdup(name) : NULL;
    symbol->kind = kind;
    symbol->ty = ty;

    entry->symbol = symbol;
    entry->next = allocated_symbols;
    allocated_symbols = entry;

    return symbol;
}

void sym_cleanup(void)
{
    struct symbol_entry *entry = allocated_symbols;

    while (entry) {
        struct symbol_entry *next = entry->next;

        free(entry->symbol->name);
        free(entry->symbol);
        free(entry);
        entry = next;
    }

    allocated_symbols = NULL;
}
