#ifndef DONKEY_PARSER_INTERNAL_H
#define DONKEY_PARSER_INTERNAL_H

#include "defs.h"

struct enum_constant {
    char *name;
    long value;
};
struct declarator_result {
    char *name;
    SourceLocation location;
};

/* The source path diagnostics name, set once per translation unit. */
extern const char *parser_source_path;


/*
 * Shared between the parser's four files.
 *
 * parser.c holds what every level of the grammar needs -- diagnostics and
 * recovery, the typedef and enum tables, type names, declarators, and
 * initializers -- while parser_decl.c, parser_stmt.c, and parser_expr.c take
 * one level each. These declarations are the seam between them, and are
 * internal to the parser: nothing outside src/frontend includes this.
 */

void add_enum_constant(const char *name, long value);
void add_typedef(const char *name, const char *type_name, int pointer_depth);
void derive(struct ast_node *node, int kind, int length,
    struct token *tokens, int *token_index);
const struct enum_constant *find_enum_constant(const char *name);
const struct typedef_name *find_typedef(const char *name);
int is_declaration_prefix(TokenType type);
int is_type_start(TokenType type);
int is_typedef_name(struct token *token);
int parse_array_dims(struct token *tokens, int *token_index, int *dims);
int parse_array_length(struct token *tokens, int *token_index);
struct ast_node* parse_case_label(struct token *tokens, int *token_index);
int parse_declarator(struct ast_node *node, struct token *tokens,
    int *token_index, struct declarator_result *result);
void parse_declarator_suffix(struct ast_node *node, struct token *tokens,
    int *token_index);
struct ast_node* parse_do_while_statement(struct token *tokens, int *token_index);
int parse_enum_specifier(struct token *tokens, int *token_index);
void parse_error_at(struct token *token, const char *format, ...);
char *parse_function_pointer_declarator(struct token *tokens,
    int *token_index);
struct ast_node* parse_initializer(struct token *tokens, int *token_index);
struct ast_node* parse_initializer_list(struct token *tokens, int *token_index);
int parse_pointer_stars(struct token *tokens, int *token_index);
const char *parse_struct_name(struct token *tokens, int *token_index);
struct ast_node* parse_switch_statement(struct token *tokens, int *token_index);
const char* parse_type_name(struct token *tokens, int *token_index);
void parser_reset_enums(void);
void parser_reset_typedefs(void);
void skip_declaration_prefixes(struct token *tokens, int *token_index);
void skip_parenthesised(struct token *tokens, int *token_index);
void synchronize(struct token *tokens, int *token_index);
CType type_from_name(const char *name);

#endif
