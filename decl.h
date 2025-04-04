#ifndef DONKEY_DECL_H
#define DONKEY_DECL_H

// Function prototypes for lexer functions
void lex(FILE *infile, struct token **tokens, int *token_count);
void add_token(struct token **tokens, int *token_count, TokenType type, const char *value);
void free_tokens(struct token *tokens, int token_count);

// Function to create a new AST node
struct ast_node* create_ast_node(ASTNodeType type, char *value, struct ast_node *left, struct ast_node *right);

// Function to free an AST node
void free_ast_node(struct ast_node *node);

// Parser function to parse a program (top-level entry point)
struct ast_node* parse_program(struct token *tokens, int *token_index);

// Parser function to parse a function
struct ast_node* parse_function(struct token *tokens, int *token_index);

// Parser function to parse a statement
struct ast_node* parse_statement(struct token *tokens, int *token_index);

// Parser function to parse an expression (in this case, integer literals)
struct ast_node* parse_exp(struct token *tokens, int *token_index);

// Prototype for the main assembly generation function
char* generate(struct ast_node *ast);

// Prototype for generating assembly for a function node
void generate_function(struct ast_node *node, FILE *output);

// Prototype for generating assembly for a statement node (currently just return statements)
void generate_statement(struct ast_node *node, FILE *output);

// Prototype for generating assembly for an expression node (currently just integer literals)
void generate_exp(struct ast_node *node, FILE *output);

void write_assembly_to_file(const char *filename, struct ast_node *ast);

#endif // DONKEY_DECL_H