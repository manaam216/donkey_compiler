/*
 * Parser: declarations. Translation units, functions, parameters, structs,
 * and the declarators that introduce a name.
 */
#include "defs.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decl.h"
#include "diag.h"
#include "parser_internal.h"

struct ast_node* parse_program(struct token *tokens, int *token_index, const char *source_path)
{
    parser_source_path = source_path;
    parser_reset_typedefs();
    parser_reset_enums();
    return create_ast_node_at(AST_PROGRAM, NULL, parse_function_list(tokens, token_index), NULL,
        tokens[*token_index].location);
}

struct ast_node* parse_function_list(struct token *tokens, int *token_index)
{
    int errors_before;
    int start_index;
    struct ast_node *function;
    struct ast_node *rest;

    if (tokens[*token_index].type == T_EOF) {
        return NULL;
    }

    errors_before = diag_error_count();
    start_index = *token_index;
    function = parse_external_declaration(tokens, token_index);

    if (diag_error_count() > errors_before) {
        if (diag_too_many_errors()) {
            return function ? create_ast_node(AST_FUNCTION_LIST, NULL, function, NULL) : NULL;
        }
        if (*token_index == start_index && tokens[*token_index].type != T_EOF) {
            (*token_index)++;
        }
        synchronize(tokens, token_index);
    }

    rest = parse_function_list(tokens, token_index);
    return create_ast_node(AST_FUNCTION_LIST, NULL, function, rest);
}

struct ast_node* parse_external_declaration(struct token *tokens, int *token_index)
{
    int name_index;

    skip_declaration_prefixes(tokens, token_index);

    /*
     * `typedef <type> <name>;` records an alias and declares no object, so
     * there is nothing to hand on to the later passes.
     */
    if (tokens[*token_index].type == T_TYPEDEF) {
        const char *type_name;
        int pointer_depth;

        (*token_index)++;
        if (tokens[*token_index].type == T_STRUCT) {
            parse_struct_name(tokens, token_index);
            type_name = "int";      /* struct aliases keep the tag's layout */
        } else {
            type_name = parse_type_name(tokens, token_index);
        }
        if (!type_name) {
            parse_error_at(&tokens[*token_index], "expected a type after typedef");
            return NULL;
        }
        pointer_depth = parse_pointer_stars(tokens, token_index);

        if (tokens[*token_index].type != T_IDENTIFIER) {
            parse_error_at(&tokens[*token_index],
                "expected a name for the typedef, found '%s'",
                tokens[*token_index].value);
            return NULL;
        }
        add_typedef(tokens[*token_index].value, type_name, pointer_depth);
        (*token_index)++;

        if (tokens[*token_index].type != T_SEMICOLON) {
            parse_error_at(&tokens[*token_index], "expected ';' after typedef");
        } else {
            (*token_index)++;
        }
        return NULL;
    }

    /*
     * A standalone enum definition declares no object: parsing it records the
     * constants, and there is nothing to hand on to the later passes.
     */
    if (tokens[*token_index].type == T_ENUM) {
        int lookahead = *token_index;

        parse_enum_specifier(tokens, &lookahead);
        if (tokens[lookahead].type == T_SEMICOLON) {
            *token_index = lookahead + 1;
            return NULL;
        }
    }

    name_index = *token_index;

    if (tokens[*token_index].type == T_STRUCT &&
        tokens[*token_index + 1].type == T_IDENTIFIER &&
        tokens[*token_index + 2].type == T_OPENBRACE) {
        return parse_struct_definition(tokens, token_index);
    }

    if (tokens[*token_index].type == T_STRUCT) {
        name_index += 2;
        while (tokens[name_index].type == T_STAR) {
            name_index++;
        }
        if (tokens[name_index].type != T_IDENTIFIER) {
            parse_error_at(&tokens[name_index], "expected identifier in top-level declaration, found '%s'",
                tokens[name_index].value);
            return NULL;
        }
        /* A struct type can be a return type as well as a variable's type. */
        if (tokens[name_index + 1].type == T_OPENPAREN) {
            return parse_function(tokens, token_index);
        }
        return parse_global_declaration(tokens, token_index);
    }

    if (!parse_type_name(tokens, &name_index)) {
        parse_error_at(&tokens[*token_index], "expected top-level declaration, found '%s'",
            tokens[*token_index].value);
        return NULL;
    }
    parse_pointer_stars(tokens, &name_index);

    if (tokens[name_index].type != T_IDENTIFIER) {
        parse_error_at(&tokens[name_index], "expected identifier in top-level declaration, found '%s'",
            tokens[name_index].value);
    }

    if (tokens[name_index + 1].type == T_OPENPAREN) {
        return parse_function(tokens, token_index);
    }

    return parse_global_declaration(tokens, token_index);
}

struct ast_node* parse_struct_definition(struct token *tokens, int *token_index)
{
    SourceLocation location = tokens[*token_index].location;
    struct ast_node *fields = NULL;
    struct ast_node **fields_tail = &fields;

    (*token_index)++;
    if (tokens[*token_index].type != T_IDENTIFIER) {
        parse_error_at(&tokens[*token_index], "expected struct name, found '%s'",
            tokens[*token_index].value);
    }
    char *name = tokens[*token_index].value;
    (*token_index)++;
    if (tokens[*token_index].type != T_OPENBRACE) {
        parse_error_at(&tokens[*token_index], "expected '{', found '%s'",
            tokens[*token_index].value);
    } else {
        (*token_index)++;
    }

    while (tokens[*token_index].type != T_CLOSEBRACE) {
        const char *type_name = parse_type_name(tokens, token_index);
        if (!type_name) {
            parse_error_at(&tokens[*token_index], "expected field type, found '%s'",
                tokens[*token_index].value);
        }
        int pointer_depth = parse_pointer_stars(tokens, token_index);
        if (tokens[*token_index].type != T_IDENTIFIER) {
            parse_error_at(&tokens[*token_index], "expected field name, found '%s'",
                tokens[*token_index].value);
        }
        struct ast_node *field = create_ast_node_at(AST_DECL, tokens[*token_index].value,
            NULL, NULL, tokens[*token_index].location);
        field->data_type = type_from_name(type_name);
        field->pointer_depth = pointer_depth;
        (*token_index)++;
        if (tokens[*token_index].type != T_SEMICOLON) {
            parse_error_at(&tokens[*token_index], "expected ';', found '%s'",
                tokens[*token_index].value);
        }
        (*token_index)++;
        /*
         * Append rather than prepend. Building the list backwards laid every
         * struct out in reverse: direct field access stayed self-consistent
         * and looked correct, but anything that depends on declaration order
         * -- a brace initializer, or matching another compiler's layout --
         * was wrong.
         */
        *fields_tail = create_ast_node(AST_FIELD_LIST, NULL, field, NULL);
        fields_tail = &(*fields_tail)->right;
    }
    (*token_index)++;
    if (tokens[*token_index].type != T_SEMICOLON) {
        parse_error_at(&tokens[*token_index], "expected ';' after struct definition, found '%s'",
            tokens[*token_index].value);
    }
    (*token_index)++;

    return create_ast_node_at(AST_STRUCT_DEF, name, fields, NULL, location);
}

struct ast_node* parse_function(struct token *tokens, int *token_index)
{
    const char *return_struct = NULL;
    const char *type_name;

    if (tokens[*token_index].type == T_STRUCT) {
        return_struct = parse_struct_name(tokens, token_index);
        type_name = "int";
    } else {
        type_name = parse_type_name(tokens, token_index);
    }
    struct token *tok;
    int pointer_depth;

    if (!type_name) {
        parse_error_at(&tokens[*token_index], "expected function return type, found '%s'",
            tokens[*token_index].value);
    }

    pointer_depth = parse_pointer_stars(tokens, token_index);
    tok = &tokens[*token_index];
    if (tok->type != T_IDENTIFIER) {
        parse_error_at(tok, "expected identifier, found '%s'", tok->value);
    }

    char *func_name = tok->value;
    SourceLocation function_location = tok->location;
    (*token_index)++;

    tok = &tokens[*token_index];
    if (tok->type != T_OPENPAREN) {
        parse_error_at(tok, "expected '(', found '%s'", tok->value);
    } else {
        (*token_index)++;
    }

    struct ast_node *params = parse_param_list(tokens, token_index);

    tok = &tokens[*token_index];
    if (tok->type != T_CLOSEPAREN) {
        parse_error_at(tok, "expected ')', found '%s'", tok->value);
    } else {
        (*token_index)++;
    }

    if (tokens[*token_index].type == T_SEMICOLON) {
        /*
         * A prototype: the signature without a body. It makes the function
         * callable before, or without, a definition -- which is what a header
         * is for.
         */
        struct ast_node *prototype = create_ast_node_at(AST_FUNCTION_DECL, func_name,
            params, NULL, function_location);

        (*token_index)++;
        prototype->data_type = type_from_name(type_name);
        prototype->pointer_depth = pointer_depth;
        prototype->struct_name = return_struct ? strdup(return_struct) : NULL;
        return prototype;
    }

    struct ast_node *body = parse_block(tokens, token_index);

    struct ast_node *function = create_ast_node_at(AST_FUNCTION, func_name, params, body, function_location);
    function->data_type = type_from_name(type_name);
    function->pointer_depth = pointer_depth;
    function->struct_name = return_struct ? strdup(return_struct) : NULL;
    return function;
}

struct ast_node* parse_global_declaration(struct token *tokens, int *token_index)
{
    const char *struct_name = NULL;
    const char *type_name;
    if (tokens[*token_index].type == T_STRUCT) {
        struct_name = parse_struct_name(tokens, token_index);
        type_name = "int";
    } else {
        type_name = parse_type_name(tokens, token_index);
    }
    int pointer_depth = parse_pointer_stars(tokens, token_index);

    struct token *tok = &tokens[*token_index];
    if (tok->type != T_IDENTIFIER) {
        parse_error_at(tok, "expected global variable name, found '%s'", tok->value);
    }

    char *name = tok->value;
    SourceLocation declaration_location = tok->location;
    (*token_index)++;
    int dims[DONKEY_MAX_ARRAY_DIMS];
    int dim_count = parse_array_dims(tokens, token_index, dims);
    int array_length = dim_count > 0 ? dims[0] : 0;

    struct ast_node *initializer = NULL;
    if (tokens[*token_index].type == T_ASSIGN) {
        (*token_index)++;
        initializer = parse_initializer(tokens, token_index);
    }

    if (tokens[*token_index].type != T_SEMICOLON) {
        parse_error_at(&tokens[*token_index], "expected ';' after global declaration, found '%s'",
            tokens[*token_index].value);
    }
    (*token_index)++;

    struct ast_node *declaration = create_ast_node_at(AST_GLOBAL_DECL, name, initializer, NULL,
        declaration_location);
    declaration->data_type = type_from_name(type_name);
    declaration->pointer_depth = pointer_depth;
    declaration->array_length = array_length;
    memcpy(declaration->array_dims, dims, sizeof(dims));
    declaration->array_dim_count = dim_count;
    declaration->struct_name = struct_name ? strdup(struct_name) : NULL;
    return declaration;
}

struct ast_node* parse_param_list(struct token *tokens, int *token_index)
{
    const char *struct_name = NULL;
    const char *type_name;
    int pointer_depth;
    struct token *tok;
    struct ast_node *param;
    struct ast_node *rest = NULL;

    if (tokens[*token_index].type == T_CLOSEPAREN) {
        return NULL;
    }

    /*
     * `...` ends the list and marks the function variadic. It is recorded as a
     * parameter node of its own so the shape of the list stays uniform.
     */
    if (tokens[*token_index].type == T_ELLIPSIS) {
        struct ast_node *ellipsis = create_ast_node_at(AST_IDENTIFIER, "...",
            NULL, NULL, tokens[*token_index].location);

        ellipsis->data_type = TYPE_INVALID;
        (*token_index)++;
        return create_ast_node(AST_PARAM_LIST, NULL, ellipsis, NULL);
    }

    skip_declaration_prefixes(tokens, token_index);

    if (tokens[*token_index].type == T_STRUCT) {
        struct_name = parse_struct_name(tokens, token_index);
        type_name = "int";
    } else {
        type_name = parse_type_name(tokens, token_index);
    }
    if (!type_name) {
        parse_error_at(&tokens[*token_index], "expected parameter type, found '%s'",
            tokens[*token_index].value);
        return NULL;
    }

    skip_declaration_prefixes(tokens, token_index);

    /* A parameter may itself be a function pointer: int (*op)(int, int). */
    {
        char *function_pointer_name =
            parse_function_pointer_declarator(tokens, token_index);

        if (function_pointer_name) {
            struct ast_node *fp = create_ast_node_at(AST_IDENTIFIER,
                function_pointer_name, NULL, NULL, tokens[*token_index].location);
            struct ast_node *rest_of_list = NULL;

            fp->data_type = type_from_name(type_name);
            fp->pointer_depth = 1;
            fp->is_function_pointer = 1;

            if (tokens[*token_index].type == T_COMMA) {
                (*token_index)++;
                rest_of_list = parse_param_list(tokens, token_index);
            }
            return create_ast_node(AST_PARAM_LIST, NULL, fp, rest_of_list);
        }
    }

    pointer_depth = parse_pointer_stars(tokens, token_index);
    skip_declaration_prefixes(tokens, token_index);

    /*
     * `void` alone means the function takes nothing, as in `int f(void)`. It is
     * not a parameter, so the list is empty.
     */
    if (strcmp(type_name, "void") == 0 && pointer_depth == 0 &&
        tokens[*token_index].type == T_CLOSEPAREN) {
        return NULL;
    }

    tok = &tokens[*token_index];
    if (tok->type == T_IDENTIFIER) {
        param = create_ast_node_at(AST_IDENTIFIER, tok->value, NULL, NULL, tok->location);
        (*token_index)++;
    } else {
        /*
         * A prototype may name its types and not its parameters. The
         * declaration is still complete, so an unnamed one is accepted.
         */
        param = create_ast_node_at(AST_IDENTIFIER, NULL, NULL, NULL, tok->location);
    }

    param->data_type = type_from_name(type_name);
    param->pointer_depth = pointer_depth;

    if (tokens[*token_index].type == T_OPENBRACKET) {
        parse_array_length(tokens, token_index);
        param->pointer_depth++;
    }
    param->struct_name = struct_name ? strdup(struct_name) : NULL;

    if (tokens[*token_index].type == T_COMMA) {
        (*token_index)++;
        rest = parse_param_list(tokens, token_index);
    }

    return create_ast_node(AST_PARAM_LIST, NULL, param, rest);
}

struct ast_node* parse_block(struct token *tokens, int *token_index)
{
    struct token *tok = &tokens[*token_index];

    if (tok->type != T_OPENBRACE) {
        parse_error_at(tok, "expected '{', found '%s'", tok->value);
    }
    SourceLocation block_location = tok->location;
    (*token_index)++;

    struct ast_node *statements = parse_statement_list(tokens, token_index);

    tok = &tokens[*token_index];
    if (tok->type != T_CLOSEBRACE) {
        parse_error_at(tok, "expected '}', found '%s'", tok->value);
    } else {
        (*token_index)++;
    }

    return create_ast_node_at(AST_BLOCK, NULL, statements, NULL, block_location);
}

struct ast_node* parse_statement_list(struct token *tokens, int *token_index)
{
    int errors_before;
    int start_index;
    struct ast_node *stmt;
    struct ast_node *rest;

    /*
     * EOF as well as '}': after an error the closing brace may have been
     * consumed, and without this the recursion would run off the token array.
     */
    if (tokens[*token_index].type == T_CLOSEBRACE ||
        tokens[*token_index].type == T_EOF) {
        return NULL;
    }

    errors_before = diag_error_count();
    start_index = *token_index;
    stmt = parse_statement(tokens, token_index);

    if (diag_error_count() > errors_before) {
        if (diag_too_many_errors()) {
            return stmt ? create_ast_node(AST_STATEMENT_LIST, NULL, stmt, NULL) : NULL;
        }
        /* Guarantee forward progress before resynchronising. */
        if (*token_index == start_index && tokens[*token_index].type != T_EOF) {
            (*token_index)++;
        }
        synchronize(tokens, token_index);
    }

    rest = parse_statement_list(tokens, token_index);
    return create_ast_node(AST_STATEMENT_LIST, NULL, stmt, rest);
}

struct ast_node* parse_declaration(struct token *tokens, int *token_index)
{
    const char *struct_name = NULL;
    const char *type_name;
    struct ast_node *first = NULL;
    struct ast_node **tail = &first;

    skip_declaration_prefixes(tokens, token_index);

    if (tokens[*token_index].type == T_STRUCT) {
        struct_name = parse_struct_name(tokens, token_index);
        type_name = "int";
    } else {
        type_name = parse_type_name(tokens, token_index);
    }
    if (!type_name) {
        parse_error_at(&tokens[*token_index], "expected a type, found '%s'",
            tokens[*token_index].value);
        return NULL;
    }

    for (;;) {
        int pointer_depth;
        struct token *tok;
        char *name;
        char *function_pointer_name;
        SourceLocation declaration_location;
        int array_length;
        int dims[DONKEY_MAX_ARRAY_DIMS];
        int dim_count;
        struct ast_node *initializer = NULL;
        struct ast_node *declaration;

        /*
         * A declarator with parentheses nests, so it goes through the general
         * grammar; the flat fields cannot tell a pointer to an array from an
         * array of pointers.
         */
        if (tokens[*token_index].type == T_OPENPAREN) {
            struct ast_node *declaration = create_ast_node_at(AST_DECL, NULL,
                NULL, NULL, tokens[*token_index].location);
            struct declarator_result found;
            int d;

            found.name = NULL;
            found.location = tokens[*token_index].location;
            if (!parse_declarator(declaration, tokens, token_index, &found)) {
                parse_error_at(&tokens[*token_index],
                    "expected a name in the declarator");
                return first;
            }

            declaration->value = found.name ? strdup(found.name) : NULL;
            declaration->location = found.location;
            declaration->data_type = type_from_name(type_name);
            declaration->struct_name = struct_name ? strdup(struct_name) : NULL;

            /* A pointer to a function is called indirectly. */
            for (d = 0; d + 1 < declaration->derivation_count; d++) {
                if (declaration->derivations[d].kind == DERIVE_FUNCTION &&
                    declaration->derivations[d + 1].kind == DERIVE_POINTER) {
                    declaration->is_function_pointer = 1;
                }
            }

            /*
             * The outermost step says what the object is. The flat fields the
             * type checker still reads cannot express the nesting, but they can
             * at least agree on that much -- a pointer to an array is a
             * pointer.
             */
            if (declaration->derivation_count > 0) {
                int last = declaration->derivation_count - 1;

                if (declaration->derivations[last].kind == DERIVE_POINTER) {
                    declaration->pointer_depth = 1;
                } else if (declaration->derivations[last].kind == DERIVE_ARRAY) {
                    declaration->array_length = declaration->derivations[last].length;
                    declaration->array_dims[0] = declaration->array_length;
                    declaration->array_dim_count = 1;
                }
            }

            if (tokens[*token_index].type == T_ASSIGN) {
                (*token_index)++;
                declaration->left = parse_initializer(tokens, token_index);
            }

            if (first == NULL) {
                first = declaration;
                tail = &first;
            } else {
                *tail = create_ast_node(AST_STATEMENT_LIST, NULL, *tail, declaration);
                tail = &(*tail)->right;
            }

            if (tokens[*token_index].type != T_COMMA) {
                break;
            }
            (*token_index)++;
            continue;
        }

        /* TYPE (*name)(params) declares a pointer to a function. */
        function_pointer_name = NULL;
        if (function_pointer_name) {
            struct ast_node *declaration = create_ast_node_at(AST_DECL,
                function_pointer_name, NULL, NULL, tokens[*token_index].location);

            declaration->data_type = type_from_name(type_name);
            declaration->pointer_depth = 1;
            declaration->is_function_pointer = 1;

            if (tokens[*token_index].type == T_ASSIGN) {
                (*token_index)++;
                declaration->left = parse_initializer(tokens, token_index);
            }

            if (first == NULL) {
                first = declaration;
                tail = &first;
            } else {
                *tail = create_ast_node(AST_STATEMENT_LIST, NULL, *tail, declaration);
                tail = &(*tail)->right;
            }

            if (tokens[*token_index].type != T_COMMA) {
                break;
            }
            (*token_index)++;
            continue;
        }

        pointer_depth = parse_pointer_stars(tokens, token_index);
        tok = &tokens[*token_index];

        if (tok->type != T_IDENTIFIER) {
            parse_error_at(tok, "expected identifier in declaration, found '%s'",
                tok->value);
            return first;
        }

        name = tok->value;
        declaration_location = tok->location;
        (*token_index)++;
        array_length = parse_array_dims(tokens, token_index, dims);
        dim_count = array_length;
        array_length = dim_count > 0 ? dims[0] : 0;

        if (tokens[*token_index].type == T_ASSIGN) {
            (*token_index)++;
            initializer = parse_initializer(tokens, token_index);
        }

        declaration = create_ast_node_at(AST_DECL, name, initializer, NULL,
            declaration_location);
        declaration->data_type = type_from_name(type_name);
        declaration->pointer_depth = pointer_depth;
        declaration->array_length = array_length;
        memcpy(declaration->array_dims, dims, sizeof(dims));
        declaration->array_dim_count = dim_count;
        declaration->struct_name = struct_name ? strdup(struct_name) : NULL;

        /* A single declarator stays a bare AST_DECL, as it always was. */
        if (first == NULL) {
            first = declaration;
            tail = &first;
        } else {
            *tail = create_ast_node(AST_STATEMENT_LIST, NULL, *tail, declaration);
            tail = &(*tail)->right;
        }

        if (tokens[*token_index].type != T_COMMA) {
            break;
        }
        (*token_index)++;
    }

    if (tokens[*token_index].type != T_SEMICOLON) {
        parse_error_at(&tokens[*token_index], "expected ';', found '%s'",
            tokens[*token_index].value);
    } else {
        (*token_index)++;
    }

    return first;
}
