#ifndef DONKEY_DECL_H
#define DONKEY_DECL_H

void lex(FILE *infile, struct token **tokens, int *token_count);
void add_token(struct token **tokens, int *token_count, TokenType type, const char *value);
void free_tokens(struct token *tokens, int token_count);

struct ast_node* create_ast_node(ASTNodeType type, char *value, struct ast_node *left, struct ast_node *right);
void free_ast_node(struct ast_node *node);

struct ast_node* parse_program(struct token *tokens, int *token_index);
struct ast_node* parse_function_list(struct token *tokens, int *token_index);
struct ast_node* parse_function(struct token *tokens, int *token_index);
struct ast_node* parse_param_list(struct token *tokens, int *token_index);
struct ast_node* parse_block(struct token *tokens, int *token_index);
struct ast_node* parse_statement_list(struct token *tokens, int *token_index);
struct ast_node* parse_statement(struct token *tokens, int *token_index);
struct ast_node* parse_declaration(struct token *tokens, int *token_index);
struct ast_node* parse_if_statement(struct token *tokens, int *token_index);
struct ast_node* parse_while_statement(struct token *tokens, int *token_index);
struct ast_node* parse_for_statement(struct token *tokens, int *token_index);
struct ast_node* parse_for_init(struct token *tokens, int *token_index);
struct ast_node* parse_optional_exp(struct token *tokens, int *token_index);
struct ast_node* parse_exp(struct token *tokens, int *token_index);
struct ast_node* parse_assignment(struct token *tokens, int *token_index);
struct ast_node* parse_logical_or(struct token *tokens, int *token_index);
struct ast_node* parse_logical_and(struct token *tokens, int *token_index);
struct ast_node* parse_bitwise_or(struct token *tokens, int *token_index);
struct ast_node* parse_bitwise_xor(struct token *tokens, int *token_index);
struct ast_node* parse_bitwise_and(struct token *tokens, int *token_index);
struct ast_node* parse_equality(struct token *tokens, int *token_index);
struct ast_node* parse_relational(struct token *tokens, int *token_index);
struct ast_node* parse_additive(struct token *tokens, int *token_index);
struct ast_node* parse_term(struct token *tokens, int *token_index);
struct ast_node* parse_factor(struct token *tokens, int *token_index);
struct ast_node* parse_arg_list(struct token *tokens, int *token_index);

char* generate(struct ast_node *ast);
void generate_function(struct ast_node *node, FILE *output);
void generate_program(struct ast_node *node, FILE *output);
void generate_statement(struct ast_node *node, FILE *output);
void generate_exp(struct ast_node *node, FILE *output);
int generate_call_args(struct ast_node *node, FILE *output);
void write_assembly_to_file(const char *filename, struct ast_node *ast);

#endif
