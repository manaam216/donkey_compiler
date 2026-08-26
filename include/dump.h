#ifndef DONKEY_DUMP_H
#define DONKEY_DUMP_H

#include "defs.h"

/*
 * Debug output for the compiler's intermediate state, reached with
 * --dump-tokens and --dump-ast. Dumping the tree after semantic analysis also
 * shows the resolved types and storage locations attached to each node.
 */
void dump_tokens(const struct token *tokens, int token_count);
void dump_ast(const struct ast_node *root);

#endif
