#include "defs.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decl.h"
#include "diag.h"

static const char *parser_source_path;

/*
 * Report a syntax error and keep going. The caller is expected to put the
 * parser back on a token it can resume from -- see synchronize() -- so a single
 * missing semicolon does not hide everything after it.
 */
static void parse_error_at(struct token *token, const char *format, ...)
{
    va_list args;
    char message[256];

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    diag_at(DIAG_ERROR, token->location, "%s", message);
}

/*
 * Panic-mode recovery: skip tokens until something that plausibly starts a new
 * statement or declaration. Stopping after a ';' or on a '}' means the parser
 * resumes at a structural boundary rather than mid-expression, where it would
 * only produce follow-on noise.
 */
static void synchronize(struct token *tokens, int *token_index)
{
    while (tokens[*token_index].type != T_EOF) {
        if (tokens[*token_index].type == T_SEMICOLON) {
            (*token_index)++;
            return;
        }
        switch (tokens[*token_index].type) {
            case T_CLOSEBRACE:
            case T_IF:
            case T_WHILE:
            case T_FOR:
            case T_RETURN:
            case T_BREAK:
            case T_CONTINUE:
            case T_CHAR:
            case T_SHORT:
            case T_INT:
            case T_LONG:
            case T_SIGNED:
            case T_UNSIGNED:
            case T_STRUCT:
                return;
            default:
                (*token_index)++;
                break;
        }
    }
}

/*
 * Storage-class specifiers and type qualifiers. They are recognised so that
 * declarations written the way real headers write them will parse; none of
 * them changes the generated code yet, so they are skipped once seen.
 */
static int is_declaration_prefix(TokenType type)
{
    return type == T_EXTERN || type == T_STATIC ||
        type == T_CONST || type == T_VOLATILE;
}

static void skip_declaration_prefixes(struct token *tokens, int *token_index)
{
    while (is_declaration_prefix(tokens[*token_index].type)) {
        (*token_index)++;
    }
}

/*
 * typedef names.
 *
 * A typedef makes the parser's job context-dependent: `foo * bar;` declares a
 * pointer if foo is a type and multiplies if it is a variable. The only way to
 * tell is to remember which names have been typedef'd, which is what this
 * table is for. Definitions are visible from their point of declaration
 * onward, which is why it is consulted during parsing rather than later.
 */
struct typedef_name {
    char *name;
    char *type_name;            /* the base type it stands for */
    int pointer_depth;
};

static struct typedef_name *typedef_names;
static int typedef_count;
static int typedef_capacity;

/*
 * Pointer depth carried by a typedef the parser just resolved, as in
 * `typedef int *IntPtr;`. parse_type_name reports only a base type name, so
 * the stars the alias stands for are held here and added by the
 * parse_pointer_stars call that always follows.
 */
static int pending_typedef_pointers;

static void add_typedef(const char *name, const char *type_name, int pointer_depth)
{
    if (typedef_count >= typedef_capacity) {
        typedef_capacity = typedef_capacity ? typedef_capacity * 2 : 32;
        typedef_names = realloc(typedef_names,
            (size_t)typedef_capacity * sizeof(*typedef_names));
        if (!typedef_names) {
            perror("Error allocating typedef");
            exit(EXIT_FAILURE);
        }
    }
    typedef_names[typedef_count].name = strdup(name);
    typedef_names[typedef_count].type_name = strdup(type_name);
    typedef_names[typedef_count].pointer_depth = pointer_depth;
    typedef_count++;
}

static const struct typedef_name *find_typedef(const char *name)
{
    int i;

    if (!name) {
        return NULL;
    }
    for (i = typedef_count - 1; i >= 0; i--) {
        if (strcmp(typedef_names[i].name, name) == 0) {
            return &typedef_names[i];
        }
    }
    return NULL;
}

void parser_reset_typedefs(void)
{
    int i;

    for (i = 0; i < typedef_count; i++) {
        free(typedef_names[i].name);
        free(typedef_names[i].type_name);
    }
    free(typedef_names);
    typedef_names = NULL;
    typedef_count = 0;
    typedef_capacity = 0;
}

/*
 * enum { A, B = 5, C } -- the constants become ordinary integer values, so the
 * parser records them and every later use is just an int literal. The tag, if
 * present, is accepted and ignored: an enum is an int here.
 *
 * The table is file-scope because enum constants, unlike variables, are known
 * at parse time and are visible from the point of definition onward.
 */
struct enum_constant {
    char *name;
    long value;
};

static struct enum_constant *enum_constants;
static int enum_constant_count;
static int enum_constant_capacity;

static void add_enum_constant(const char *name, long value)
{
    if (enum_constant_count >= enum_constant_capacity) {
        enum_constant_capacity = enum_constant_capacity ? enum_constant_capacity * 2 : 32;
        enum_constants = realloc(enum_constants,
            (size_t)enum_constant_capacity * sizeof(*enum_constants));
        if (!enum_constants) {
            perror("Error allocating enum constant");
            exit(EXIT_FAILURE);
        }
    }
    enum_constants[enum_constant_count].name = strdup(name);
    enum_constants[enum_constant_count].value = value;
    enum_constant_count++;
}

static const struct enum_constant *find_enum_constant(const char *name)
{
    int i;

    if (!name) {
        return NULL;
    }
    for (i = enum_constant_count - 1; i >= 0; i--) {
        if (strcmp(enum_constants[i].name, name) == 0) {
            return &enum_constants[i];
        }
    }
    return NULL;
}

void parser_reset_enums(void)
{
    int i;

    for (i = 0; i < enum_constant_count; i++) {
        free(enum_constants[i].name);
    }
    free(enum_constants);
    enum_constants = NULL;
    enum_constant_count = 0;
    enum_constant_capacity = 0;
}

/* Parse an enum specifier, recording its constants. Returns 1 if one was there. */
static int parse_enum_specifier(struct token *tokens, int *token_index)
{
    long next_value = 0;

    if (tokens[*token_index].type != T_ENUM) {
        return 0;
    }
    (*token_index)++;

    if (tokens[*token_index].type == T_IDENTIFIER) {
        (*token_index)++;           /* the tag; an enum is an int here */
    }

    if (tokens[*token_index].type != T_OPENBRACE) {
        return 1;                   /* a reference to an existing enum type */
    }
    (*token_index)++;

    while (tokens[*token_index].type != T_CLOSEBRACE &&
           tokens[*token_index].type != T_EOF) {
        const char *name;

        if (tokens[*token_index].type != T_IDENTIFIER) {
            parse_error_at(&tokens[*token_index],
                "expected an enumerator name, found '%s'",
                tokens[*token_index].value);
            break;
        }
        name = tokens[*token_index].value;
        (*token_index)++;

        if (tokens[*token_index].type == T_ASSIGN) {
            (*token_index)++;
            if (tokens[*token_index].type == T_INTLIT ||
                tokens[*token_index].type == T_CHARLIT) {
                next_value = strtol(tokens[*token_index].value, NULL, 0);
                (*token_index)++;
            } else if (tokens[*token_index].type == T_IDENTIFIER) {
                const struct enum_constant *previous =
                    find_enum_constant(tokens[*token_index].value);

                if (previous) {
                    next_value = previous->value;
                } else {
                    parse_error_at(&tokens[*token_index],
                        "enumerator value must be a constant");
                }
                (*token_index)++;
            } else {
                parse_error_at(&tokens[*token_index],
                    "enumerator value must be a constant");
            }
        }

        add_enum_constant(name, next_value);
        next_value++;

        if (tokens[*token_index].type == T_COMMA) {
            (*token_index)++;
        }
    }

    if (tokens[*token_index].type == T_CLOSEBRACE) {
        (*token_index)++;
    } else {
        parse_error_at(&tokens[*token_index], "expected '}' to close the enum");
    }
    return 1;
}

static const char* parse_type_name(struct token *tokens, int *token_index)
{
    int is_unsigned = 0;
    int has_sign = 0;
    TokenType base_type;

    pending_typedef_pointers = 0;

    /* An enum is an int; parsing the specifier records its constants. */
    if (tokens[*token_index].type == T_ENUM) {
        parse_enum_specifier(tokens, token_index);
        return "int";
    }

    /* A name introduced by typedef stands in for its underlying type. */
    if (tokens[*token_index].type == T_IDENTIFIER) {
        const struct typedef_name *alias = find_typedef(tokens[*token_index].value);

        if (alias) {
            (*token_index)++;
            pending_typedef_pointers = alias->pointer_depth;
            return alias->type_name;
        }
    }

    if (tokens[*token_index].type == T_SIGNED || tokens[*token_index].type == T_UNSIGNED) {
        is_unsigned = tokens[*token_index].type == T_UNSIGNED;
        has_sign = 1;
        (*token_index)++;
    }

    base_type = tokens[*token_index].type;
    if (base_type == T_VOID && !has_sign) {
        (*token_index)++;
        return "void";
    }
    if (base_type == T_CHAR || base_type == T_SHORT || base_type == T_INT || base_type == T_LONG) {
        (*token_index)++;
    } else if (has_sign) {
        base_type = T_INT;
    } else {
        return NULL;
    }

    if ((base_type == T_SHORT || base_type == T_LONG) && tokens[*token_index].type == T_INT) {
        (*token_index)++;
    }

    if (base_type == T_CHAR) {
        return is_unsigned ? "uchar" : "char";
    }
    if (base_type == T_SHORT) {
        return is_unsigned ? "ushort" : "short";
    }
    if (base_type == T_LONG) {
        return is_unsigned ? "ulong" : "long";
    }

    return is_unsigned ? "uint" : "int";
}

static CType type_from_name(const char *name)
{
    if (!name) return TYPE_INT;
    if (strcmp(name, "void") == 0) return TYPE_VOID;
    if (strcmp(name, "char") == 0) return TYPE_CHAR;
    if (strcmp(name, "uchar") == 0) return TYPE_UCHAR;
    if (strcmp(name, "short") == 0) return TYPE_SHORT;
    if (strcmp(name, "ushort") == 0) return TYPE_USHORT;
    if (strcmp(name, "uint") == 0) return TYPE_UINT;
    if (strcmp(name, "long") == 0) return TYPE_LONG;
    if (strcmp(name, "ulong") == 0) return TYPE_ULONG;
    return TYPE_INT;
}

static int parse_pointer_stars(struct token *tokens, int *token_index)
{
    /* Stars the alias already stood for, plus any written here. */
    int pointer_depth = pending_typedef_pointers;

    pending_typedef_pointers = 0;
    while (tokens[*token_index].type == T_STAR) {
        pointer_depth++;
        (*token_index)++;
    }
    return pointer_depth;
}

static int parse_array_length(struct token *tokens, int *token_index)
{
    int length;

    if (tokens[*token_index].type != T_OPENBRACKET) {
        return 0;
    }
    (*token_index)++;
    if (tokens[*token_index].type != T_INTLIT) {
        parse_error_at(&tokens[*token_index], "expected array length, found '%s'",
            tokens[*token_index].value);
        return 1;               /* keep going with a plausible length */
    }
    length = atoi(tokens[*token_index].value);
    if (length <= 0) {
        parse_error_at(&tokens[*token_index], "array length must be greater than zero");
        length = 1;
    }
    (*token_index)++;
    if (tokens[*token_index].type != T_CLOSEBRACKET) {
        parse_error_at(&tokens[*token_index], "expected ']', found '%s'",
            tokens[*token_index].value);
    } else {
        (*token_index)++;
    }
    return length;
}

/*
 * Parse every `[N]` of a declarator, outermost first. Returns how many there
 * were; dims receives the sizes. A declarator with no brackets yields zero.
 */
static int parse_array_dims(struct token *tokens, int *token_index, int *dims)
{
    int count = 0;

    while (tokens[*token_index].type == T_OPENBRACKET) {
        int length = parse_array_length(tokens, token_index);

        if (count < DONKEY_MAX_ARRAY_DIMS) {
            dims[count] = length;
        } else {
            parse_error_at(&tokens[*token_index],
                "arrays may nest at most %d deep", DONKEY_MAX_ARRAY_DIMS);
        }
        count++;
    }
    return count < DONKEY_MAX_ARRAY_DIMS ? count : DONKEY_MAX_ARRAY_DIMS;
}

/*
 * Copy an expression subtree. Compound assignment is rewritten as
 * `target = target op value`, which needs the target twice; the two copies
 * must be independent nodes because semantic analysis annotates each one.
 */
static struct ast_node *clone_expression(const struct ast_node *node)
{
    struct ast_node *copy;

    if (!node) {
        return NULL;
    }

    copy = create_ast_node_at(node->type, node->value,
        clone_expression(node->left), clone_expression(node->right),
        node->location);
    copy->data_type = node->data_type;
    copy->pointer_depth = node->pointer_depth;
    copy->array_length = node->array_length;
    memcpy(copy->array_dims, node->array_dims, sizeof(copy->array_dims));
    copy->array_dim_count = node->array_dim_count;
    copy->struct_name = node->struct_name ? strdup(node->struct_name) : NULL;
    copy->string_label = node->string_label;
    return copy;
}

/*
 * Whether an expression can be evaluated twice safely, which the rewrite above
 * requires. A call could do anything, so a target containing one is refused
 * rather than silently run twice.
 */
static int is_repeatable(const struct ast_node *node)
{
    if (!node) {
        return 1;
    }
    if (node->type == AST_CALL || node->type == AST_ASSIGN ||
        node->type == AST_PRE_INCREMENT || node->type == AST_POST_INCREMENT ||
        node->type == AST_PRE_DECREMENT || node->type == AST_POST_DECREMENT) {
        return 0;
    }
    return is_repeatable(node->left) && is_repeatable(node->right);
}

/*
 * A function-pointer declarator, TYPE (*name)(params).
 *
 * Only this one nesting is recognised, not the general recursive declarator
 * grammar that would also give `int (*a)[10]`. It is the form that matters in
 * practice, and treating it specially keeps the rest of the parser as it is.
 *
 * Returns the name on success with *token_index past the parameter list, or
 * NULL with the index untouched.
 */
static char *parse_function_pointer_declarator(struct token *tokens,
    int *token_index)
{
    int look = *token_index;
    char *name;

    if (tokens[look].type != T_OPENPAREN || tokens[look + 1].type != T_STAR ||
        tokens[look + 2].type != T_IDENTIFIER ||
        tokens[look + 3].type != T_CLOSEPAREN ||
        tokens[look + 4].type != T_OPENPAREN) {
        return NULL;
    }

    name = tokens[look + 2].value;
    look += 5;

    /*
     * The parameter list is parsed for its syntax only. Calls through the
     * pointer are checked against the arguments given, not against a stored
     * signature, so the types are not recorded.
     */
    {
        int depth = 1;

        while (tokens[look].type != T_EOF && depth > 0) {
            if (tokens[look].type == T_OPENPAREN) depth++;
            else if (tokens[look].type == T_CLOSEPAREN) depth--;
            look++;
        }
    }

    *token_index = look;
    return name;
}

static struct ast_node* parse_initializer(struct token *tokens, int *token_index);
static struct ast_node* parse_do_while_statement(struct token *tokens, int *token_index);
static struct ast_node* parse_switch_statement(struct token *tokens, int *token_index);
static struct ast_node* parse_case_label(struct token *tokens, int *token_index);

static struct ast_node* parse_initializer_list(struct token *tokens, int *token_index)
{
    if (tokens[*token_index].type == T_CLOSEBRACE) {
        return NULL;
    }

    struct ast_node *initializer = parse_initializer(tokens, token_index);
    struct ast_node *rest = NULL;
    if (tokens[*token_index].type == T_COMMA) {
        (*token_index)++;
        rest = parse_initializer_list(tokens, token_index);
    }

    return create_ast_node(AST_INITIALIZER_LIST, NULL, initializer, rest);
}

static struct ast_node* parse_initializer(struct token *tokens, int *token_index)
{
    SourceLocation location;
    struct ast_node *list;

    if (tokens[*token_index].type != T_OPENBRACE) {
        return parse_assignment(tokens, token_index);
    }

    location = tokens[*token_index].location;
    (*token_index)++;
    list = parse_initializer_list(tokens, token_index);
    if (tokens[*token_index].type != T_CLOSEBRACE) {
        parse_error_at(&tokens[*token_index], "expected '}', found '%s'",
            tokens[*token_index].value);
    } else {
        (*token_index)++;
    }
    return create_ast_node_at(AST_INITIALIZER_LIST, NULL, list, NULL, location);
}

static int is_typedef_name(struct token *token)
{
    return token->type == T_IDENTIFIER && find_typedef(token->value) != NULL;
}

static int is_type_start(TokenType type)
{
    return type == T_CHAR || type == T_SHORT || type == T_INT ||
        type == T_LONG || type == T_SIGNED || type == T_UNSIGNED ||
        type == T_STRUCT || type == T_UNION || type == T_ENUM ||
        type == T_VOID || is_declaration_prefix(type);
}

static const char *parse_struct_name(struct token *tokens, int *token_index)
{
    const char *name;

    if (tokens[*token_index].type != T_STRUCT) {
        return NULL;
    }
    (*token_index)++;
    if (tokens[*token_index].type != T_IDENTIFIER) {
        parse_error_at(&tokens[*token_index], "expected struct name, found '%s'",
            tokens[*token_index].value);
        return NULL;
    }
    name = tokens[*token_index].value;
    (*token_index)++;
    return name;
}

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
        if (tokens[name_index].type != T_IDENTIFIER) {
            parse_error_at(&tokens[name_index], "expected identifier in top-level declaration, found '%s'",
                tokens[name_index].value);
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
        fields = create_ast_node(AST_FIELD_LIST, NULL, field, fields);
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
    const char *type_name = parse_type_name(tokens, token_index);
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
        return prototype;
    }

    struct ast_node *body = parse_block(tokens, token_index);

    struct ast_node *function = create_ast_node_at(AST_FUNCTION, func_name, params, body, function_location);
    function->data_type = type_from_name(type_name);
    function->pointer_depth = pointer_depth;
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

/*
 * do { body } while (condition);
 *
 * The body always runs once, so the condition is tested at the bottom. It is
 * stored the same way as a while loop -- condition on the left, body on the
 * right -- and the node type tells the code generator which order to emit.
 */
static struct ast_node* parse_do_while_statement(struct token *tokens, int *token_index)
{
    SourceLocation location = tokens[*token_index].location;
    struct ast_node *body;
    struct ast_node *condition;

    (*token_index)++;
    body = parse_statement(tokens, token_index);

    if (tokens[*token_index].type != T_WHILE) {
        parse_error_at(&tokens[*token_index], "expected 'while' after the body of a do loop");
        return create_ast_node_at(AST_DO_WHILE, NULL, NULL, body, location);
    }
    (*token_index)++;

    if (tokens[*token_index].type != T_OPENPAREN) {
        parse_error_at(&tokens[*token_index], "expected '(' after while");
    } else {
        (*token_index)++;
    }
    condition = parse_exp(tokens, token_index);
    if (tokens[*token_index].type != T_CLOSEPAREN) {
        parse_error_at(&tokens[*token_index], "expected ')' after the condition");
    } else {
        (*token_index)++;
    }
    if (tokens[*token_index].type != T_SEMICOLON) {
        parse_error_at(&tokens[*token_index], "expected ';' after do-while");
    } else {
        (*token_index)++;
    }

    return create_ast_node_at(AST_DO_WHILE, NULL, condition, body, location);
}

/*
 * `case <constant>:` and `default:`. Both introduce a labelled point inside a
 * switch body; the statement they label is parsed as their child, so a run of
 * cases falling into one another nests naturally.
 */
static struct ast_node* parse_case_label(struct token *tokens, int *token_index)
{
    SourceLocation location = tokens[*token_index].location;
    int is_default = tokens[*token_index].type == T_DEFAULT;
    struct ast_node *node;
    char value[32];

    (*token_index)++;

    if (is_default) {
        node = create_ast_node_at(AST_DEFAULT, NULL, NULL, NULL, location);
    } else {
        long constant = 0;

        if (tokens[*token_index].type == T_INTLIT ||
            tokens[*token_index].type == T_CHARLIT) {
            constant = strtol(tokens[*token_index].value, NULL, 0);
            (*token_index)++;
        } else {
            parse_error_at(&tokens[*token_index],
                "a case label must be an integer constant");
        }
        snprintf(value, sizeof(value), "%ld", constant);
        node = create_ast_node_at(AST_CASE, value, NULL, NULL, location);
    }

    if (tokens[*token_index].type != T_COLON) {
        parse_error_at(&tokens[*token_index], "expected ':' after the case label");
    } else {
        (*token_index)++;
    }

    /*
     * A label at the very end of a switch body labels nothing; treat that as
     * an empty statement rather than running off into the closing brace.
     */
    if (tokens[*token_index].type == T_CLOSEBRACE) {
        node->left = create_ast_node_at(AST_EMPTY, NULL, NULL, NULL, location);
    } else {
        node->left = parse_statement(tokens, token_index);
    }
    return node;
}

static struct ast_node* parse_switch_statement(struct token *tokens, int *token_index)
{
    SourceLocation location = tokens[*token_index].location;
    struct ast_node *control;
    struct ast_node *body;

    (*token_index)++;
    if (tokens[*token_index].type != T_OPENPAREN) {
        parse_error_at(&tokens[*token_index], "expected '(' after switch");
    } else {
        (*token_index)++;
    }
    control = parse_exp(tokens, token_index);
    if (tokens[*token_index].type != T_CLOSEPAREN) {
        parse_error_at(&tokens[*token_index], "expected ')' after the switch value");
    } else {
        (*token_index)++;
    }

    body = parse_statement(tokens, token_index);
    return create_ast_node_at(AST_SWITCH, NULL, control, body, location);
}

struct ast_node* parse_statement(struct token *tokens, int *token_index)
{
    struct token *tok = &tokens[*token_index];

    if (tok->type == T_OPENBRACE) {
        return parse_block(tokens, token_index);
    }

    if (is_type_start(tok->type) || is_typedef_name(tok)) {
        return parse_declaration(tokens, token_index);
    }

    if (tok->type == T_IF) {
        return parse_if_statement(tokens, token_index);
    }

    if (tok->type == T_WHILE) {
        return parse_while_statement(tokens, token_index);
    }

    if (tok->type == T_FOR) {
        return parse_for_statement(tokens, token_index);
    }

    if (tok->type == T_BREAK) {
        SourceLocation break_location = tok->location;
        (*token_index)++;
        tok = &tokens[*token_index];
        if (tok->type != T_SEMICOLON) {
            parse_error_at(tok, "expected ';', found '%s'", tok->value);
        }
        (*token_index)++;
        return create_ast_node_at(AST_BREAK, NULL, NULL, NULL, break_location);
    }

    if (tok->type == T_CONTINUE) {
        SourceLocation continue_location = tok->location;
        (*token_index)++;
        tok = &tokens[*token_index];
        if (tok->type != T_SEMICOLON) {
            parse_error_at(tok, "expected ';', found '%s'", tok->value);
        }
        (*token_index)++;
        return create_ast_node_at(AST_CONTINUE, NULL, NULL, NULL, continue_location);
    }

    if (tok->type == T_RETURN) {
        SourceLocation return_location = tok->location;
        struct ast_node *exp = NULL;

        (*token_index)++;

        /* `return;` with no value, which a void function needs. */
        if (tokens[*token_index].type != T_SEMICOLON) {
            exp = parse_exp(tokens, token_index);
        }

        tok = &tokens[*token_index];
        if (tok->type != T_SEMICOLON) {
            parse_error_at(tok, "expected ';', found '%s'", tok->value);
        } else {
            (*token_index)++;
        }

        return create_ast_node_at(AST_RETURN, NULL, exp, NULL, return_location);
    }

    if (tok->type == T_DO) {
        return parse_do_while_statement(tokens, token_index);
    }

    if (tok->type == T_SWITCH) {
        return parse_switch_statement(tokens, token_index);
    }

    if (tok->type == T_CASE || tok->type == T_DEFAULT) {
        return parse_case_label(tokens, token_index);
    }

    if (tok->type == T_GOTO) {
        SourceLocation goto_location = tok->location;
        char *label;

        (*token_index)++;
        if (tokens[*token_index].type != T_IDENTIFIER) {
            parse_error_at(&tokens[*token_index], "expected a label name after goto");
            return create_ast_node_at(AST_EMPTY, NULL, NULL, NULL, goto_location);
        }
        label = tokens[*token_index].value;
        (*token_index)++;
        if (tokens[*token_index].type != T_SEMICOLON) {
            parse_error_at(&tokens[*token_index], "expected ';' after goto");
        } else {
            (*token_index)++;
        }
        return create_ast_node_at(AST_GOTO, label, NULL, NULL, goto_location);
    }

    /*
     * `name:` is a label. It takes two tokens of lookahead to tell apart from
     * an expression statement that merely starts with an identifier.
     */
    if (tok->type == T_IDENTIFIER && tokens[*token_index + 1].type == T_COLON) {
        SourceLocation label_location = tok->location;
        char *label = tok->value;

        *token_index += 2;
        return create_ast_node_at(AST_LABEL, label,
            parse_statement(tokens, token_index), NULL, label_location);
    }

    /* A lone semicolon is a statement that does nothing. */
    if (tok->type == T_SEMICOLON) {
        SourceLocation empty_location = tok->location;

        (*token_index)++;
        return create_ast_node_at(AST_EMPTY, NULL, NULL, NULL, empty_location);
    }

    struct ast_node *exp = parse_exp(tokens, token_index);

    tok = &tokens[*token_index];
    if (tok->type != T_SEMICOLON) {
        parse_error_at(tok, "expected ';', found '%s'", tok->value);
    }
    (*token_index)++;

    return create_ast_node_at(AST_EXPR_STMT, NULL, exp, NULL, exp->location);
}

/*
 * One declaration may introduce several names: `int a, b = 2;`. The base type
 * is parsed once and each declarator after it produces its own AST_DECL, with
 * the results chained into a statement list so every later pass -- which
 * already walks statement lists -- sees them without changes.
 */
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

        /* TYPE (*name)(params) declares a pointer to a function. */
        function_pointer_name = parse_function_pointer_declarator(tokens, token_index);
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

struct ast_node* parse_if_statement(struct token *tokens, int *token_index)
{
    SourceLocation if_location = tokens[*token_index].location;
    (*token_index)++;

    if (tokens[*token_index].type != T_OPENPAREN) {
        parse_error_at(&tokens[*token_index], "expected '(' after if, found '%s'",
            tokens[*token_index].value);
    } else {
        (*token_index)++;
    }

    struct ast_node *cond = parse_exp(tokens, token_index);

    if (tokens[*token_index].type != T_CLOSEPAREN) {
        parse_error_at(&tokens[*token_index], "expected ')' after if condition, found '%s'",
            tokens[*token_index].value);
    } else {
        (*token_index)++;
    }

    struct ast_node *then_stmt = parse_statement(tokens, token_index);
    struct ast_node *else_stmt = NULL;

    if (tokens[*token_index].type == T_ELSE) {
        (*token_index)++;
        else_stmt = parse_statement(tokens, token_index);
    }

    return create_ast_node_at(AST_IF, NULL, cond,
        create_ast_node_at(AST_IF_BRANCHES, NULL, then_stmt, else_stmt, if_location),
        if_location);
}

struct ast_node* parse_while_statement(struct token *tokens, int *token_index)
{
    SourceLocation while_location = tokens[*token_index].location;
    (*token_index)++;

    if (tokens[*token_index].type != T_OPENPAREN) {
        parse_error_at(&tokens[*token_index], "expected '(' after while, found '%s'",
            tokens[*token_index].value);
    } else {
        (*token_index)++;
    }

    struct ast_node *cond = parse_exp(tokens, token_index);

    if (tokens[*token_index].type != T_CLOSEPAREN) {
        parse_error_at(&tokens[*token_index], "expected ')' after while condition, found '%s'",
            tokens[*token_index].value);
    } else {
        (*token_index)++;
    }

    return create_ast_node_at(AST_WHILE, NULL, cond, parse_statement(tokens, token_index),
        while_location);
}

struct ast_node* parse_for_statement(struct token *tokens, int *token_index)
{
    SourceLocation for_location = tokens[*token_index].location;
    (*token_index)++;

    if (tokens[*token_index].type != T_OPENPAREN) {
        parse_error_at(&tokens[*token_index], "expected '(' after for, found '%s'",
            tokens[*token_index].value);
    } else {
        (*token_index)++;
    }

    struct ast_node *init = parse_for_init(tokens, token_index);

    struct ast_node *cond = parse_optional_exp(tokens, token_index);
    if (tokens[*token_index].type != T_SEMICOLON) {
        parse_error_at(&tokens[*token_index], "expected ';' after for condition, found '%s'",
            tokens[*token_index].value);
    }
    (*token_index)++;

    struct ast_node *post = parse_optional_exp(tokens, token_index);
    if (tokens[*token_index].type != T_CLOSEPAREN) {
        parse_error_at(&tokens[*token_index], "expected ')' after for clauses, found '%s'",
            tokens[*token_index].value);
    } else {
        (*token_index)++;
    }

    struct ast_node *cond_post = create_ast_node_at(AST_FOR_PARTS, NULL, cond, post, for_location);
    struct ast_node *parts = create_ast_node_at(AST_FOR_PARTS, NULL, init, cond_post, for_location);

    return create_ast_node_at(AST_FOR, NULL, parts, parse_statement(tokens, token_index),
        for_location);
}

struct ast_node* parse_for_init(struct token *tokens, int *token_index)
{
    if (tokens[*token_index].type == T_SEMICOLON) {
        (*token_index)++;
        return NULL;
    }

    if (is_type_start(tokens[*token_index].type) ||
        is_typedef_name(&tokens[*token_index])) {
        return parse_declaration(tokens, token_index);
    }

    struct ast_node *init = parse_exp(tokens, token_index);
    if (tokens[*token_index].type != T_SEMICOLON) {
        parse_error_at(&tokens[*token_index], "expected ';' after for initializer, found '%s'",
            tokens[*token_index].value);
    }
    (*token_index)++;

    return init;
}

struct ast_node* parse_optional_exp(struct token *tokens, int *token_index)
{
    if (tokens[*token_index].type == T_SEMICOLON || tokens[*token_index].type == T_CLOSEPAREN) {
        return NULL;
    }

    return parse_exp(tokens, token_index);
}

struct ast_node* parse_factor(struct token *tokens, int *token_index)
{
    struct token *tok = &tokens[*token_index];

    if (tok->type == T_PLUS_PLUS) {
        SourceLocation operator_location = tok->location;
        (*token_index)++;
        struct ast_node *operand = parse_factor(tokens, token_index);
        if (operand->type != AST_IDENTIFIER) {
        }
        return create_ast_node_at(AST_PRE_INCREMENT, NULL, operand, NULL, operator_location);
    } else if (tok->type == T_MINUS_MINUS) {
        SourceLocation operator_location = tok->location;
        (*token_index)++;
        struct ast_node *operand = parse_factor(tokens, token_index);
        if (operand->type != AST_IDENTIFIER) {
        }
        return create_ast_node_at(AST_PRE_DECREMENT, NULL, operand, NULL, operator_location);
    } else if (tok->type == T_SIZEOF) {
        SourceLocation sizeof_location = tok->location;
        (*token_index)++;
        if (tokens[*token_index].type == T_OPENPAREN) {
            int type_index = *token_index + 1;
            const char *type_name;

            /*
             * `sizeof(struct X)` names a type, not an expression. The struct's
             * size is not known until its definition has been collected, so
             * the tag is recorded and semantic analysis resolves it.
             */
            if (tokens[type_index].type == T_STRUCT &&
                tokens[type_index + 1].type == T_IDENTIFIER &&
                tokens[type_index + 2].type == T_CLOSEPAREN) {
                struct ast_node *size = create_ast_node_at(AST_SIZEOF, NULL,
                    NULL, NULL, sizeof_location);

                size->struct_name = strdup(tokens[type_index + 1].value);
                size->data_type = TYPE_UINT;
                *token_index = type_index + 3;
                return size;
            }

            type_name = parse_type_name(tokens, &type_index);
            if (type_name && tokens[type_index].type == T_CLOSEPAREN) {
                *token_index = type_index + 1;
                struct ast_node *size = create_ast_node_at(AST_SIZEOF, (char *)type_name, NULL, NULL,
                    sizeof_location);
                size->data_type = TYPE_UINT;
                return size;
            }
        }

        /*
         * Keep the operand: its resolved type is what determines the size.
         * It is never evaluated, only measured.
         */
        struct ast_node *operand = parse_factor(tokens, token_index);
        struct ast_node *size = create_ast_node_at(AST_SIZEOF, NULL, operand, NULL,
            sizeof_location);
        size->data_type = TYPE_UINT;
        return size;
    } else if (tok->type == T_MINUS) {
        SourceLocation operator_location = tok->location;
        (*token_index)++;
        return create_ast_node_at(AST_NEGATION, NULL, parse_factor(tokens, token_index), NULL,
            operator_location);
    } else if (tok->type == T_LOGICAL_NEGATION) {
        SourceLocation operator_location = tok->location;
        (*token_index)++;
        return create_ast_node_at(AST_LOGICAL_NEGATION, NULL, parse_factor(tokens, token_index), NULL,
            operator_location);
    } else if (tok->type == T_BITWISE_COMPLEMENT) {
        SourceLocation operator_location = tok->location;
        (*token_index)++;
        return create_ast_node_at(AST_BITWISE_COMPLEMENT, NULL, parse_factor(tokens, token_index), NULL,
            operator_location);
    } else if (tok->type == T_AMPERSAND) {
        SourceLocation operator_location = tok->location;
        (*token_index)++;
        return create_ast_node_at(AST_ADDRESS_OF, NULL, parse_factor(tokens, token_index), NULL,
            operator_location);
    } else if (tok->type == T_STAR) {
        SourceLocation operator_location = tok->location;
        (*token_index)++;
        return create_ast_node_at(AST_DEREFERENCE, NULL, parse_factor(tokens, token_index), NULL,
            operator_location);
    }

    if (tok->type == T_INTLIT || tok->type == T_CHARLIT) {
        struct ast_node *lit_node = create_ast_node_at(AST_INTLIT, tok->value, NULL, NULL,
            tok->location);
        (*token_index)++;
        return lit_node;
    }

    if (tok->type == T_STRINGLIT) {
        struct ast_node *str_node = create_ast_node_at(AST_STRINGLIT, tok->value, NULL, NULL,
            tok->location);
        str_node->data_type = TYPE_CHAR;
        str_node->pointer_depth = 1;
        (*token_index)++;
        return str_node;
    }

    /*
     * An enum constant is a compile-time integer, so it becomes a literal here
     * and nothing downstream needs to know enums exist.
     */
    if (tok->type == T_IDENTIFIER && find_enum_constant(tok->value)) {
        const struct enum_constant *constant = find_enum_constant(tok->value);
        char text[32];
        struct ast_node *literal;

        snprintf(text, sizeof(text), "%ld", constant->value);
        literal = create_ast_node_at(AST_INTLIT, text, NULL, NULL, tok->location);
        literal->data_type = TYPE_INT;
        (*token_index)++;
        return literal;
    }

    if (tok->type == T_IDENTIFIER) {
        char *name = tok->value;
        SourceLocation identifier_location = tok->location;
        (*token_index)++;

        if (tokens[*token_index].type == T_OPENPAREN) {
            (*token_index)++;
            struct ast_node *args = parse_arg_list(tokens, token_index);
            if (tokens[*token_index].type != T_CLOSEPAREN) {
                parse_error_at(&tokens[*token_index], "expected ')' after function call, found '%s'",
                    tokens[*token_index].value);
            } else {
                (*token_index)++;
            }
            struct ast_node *call = create_ast_node_at(AST_CALL, name, args, NULL,
                identifier_location);
            if (tokens[*token_index].type == T_PLUS_PLUS || tokens[*token_index].type == T_MINUS_MINUS) {
                parse_error_at(&tokens[*token_index],
                    "postfix increment/decrement requires an identifier");
            }
            return call;
        }

        struct ast_node *id = create_ast_node_at(AST_IDENTIFIER, name, NULL, NULL,
            identifier_location);
        while (tokens[*token_index].type == T_OPENBRACKET ||
               tokens[*token_index].type == T_DOT ||
               tokens[*token_index].type == T_ARROW) {
            if (tokens[*token_index].type == T_ARROW) {
                /* p->field means (*p).field, and is built as exactly that. */
                SourceLocation arrow_location = tokens[*token_index].location;

                (*token_index)++;
                if (tokens[*token_index].type != T_IDENTIFIER) {
                    parse_error_at(&tokens[*token_index],
                        "expected a field name after '->'");
                    break;
                }
                id = create_ast_node_at(AST_FIELD_ACCESS, tokens[*token_index].value,
                    create_ast_node_at(AST_DEREFERENCE, NULL, id, NULL, arrow_location),
                    NULL, arrow_location);
                (*token_index)++;
                continue;
            }
            if (tokens[*token_index].type == T_DOT) {
                SourceLocation dot_location = tokens[*token_index].location;
                (*token_index)++;
                if (tokens[*token_index].type != T_IDENTIFIER) {
                    parse_error_at(&tokens[*token_index], "expected field name, found '%s'",
                        tokens[*token_index].value);
                }
                id = create_ast_node_at(AST_FIELD_ACCESS, tokens[*token_index].value, id, NULL,
                    dot_location);
                (*token_index)++;
                continue;
            }
            SourceLocation bracket_location = tokens[*token_index].location;
            (*token_index)++;
            struct ast_node *index = parse_exp(tokens, token_index);
            if (tokens[*token_index].type != T_CLOSEBRACKET) {
                parse_error_at(&tokens[*token_index], "expected ']', found '%s'",
                    tokens[*token_index].value);
            } else {
                (*token_index)++;
            }
            id = create_ast_node_at(AST_ARRAY_SUBSCRIPT, NULL, id, index, bracket_location);
        }
        if (tokens[*token_index].type == T_PLUS_PLUS) {
            SourceLocation operator_location = tokens[*token_index].location;
            (*token_index)++;
            return create_ast_node_at(AST_POST_INCREMENT, NULL, id, NULL, operator_location);
        } else if (tokens[*token_index].type == T_MINUS_MINUS) {
            SourceLocation operator_location = tokens[*token_index].location;
            (*token_index)++;
            return create_ast_node_at(AST_POST_DECREMENT, NULL, id, NULL, operator_location);
        }

        return id;
    }

    if (tok->type == T_OPENPAREN) {
        SourceLocation paren_location = tok->location;
        (*token_index)++;
        int type_index = *token_index;
        const char *type_name = parse_type_name(tokens, &type_index);
        if (type_name && tokens[type_index].type == T_CLOSEPAREN) {
            *token_index = type_index + 1;
            struct ast_node *cast = create_ast_node_at(AST_CAST, (char *)type_name,
                parse_factor(tokens, token_index), NULL, paren_location);
            cast->data_type = type_from_name(type_name);
            return cast;
        }

        struct ast_node *inner_exp = parse_exp(tokens, token_index);
        tok = &tokens[*token_index];
        if (tok->type != T_CLOSEPAREN) {
            parse_error_at(tok, "expected closing parenthesis, found '%s'", tok->value);
        } else {
            (*token_index)++;
        }
        return inner_exp;
    }

    parse_error_at(tok, "unexpected token '%s' in expression parsing", tok->value);
    return NULL;
}

struct ast_node* parse_arg_list(struct token *tokens, int *token_index)
{
    if (tokens[*token_index].type == T_CLOSEPAREN) {
        return NULL;
    }

    struct ast_node *arg = parse_assignment(tokens, token_index);
    struct ast_node *rest = NULL;

    if (tokens[*token_index].type == T_COMMA) {
        (*token_index)++;
        rest = parse_arg_list(tokens, token_index);
    }

    return create_ast_node(AST_ARG_LIST, NULL, arg, rest);
}

struct ast_node* parse_term(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_factor(tokens, token_index);

    while (1) {
        struct token *tok = &tokens[*token_index];

        if (tok->type == T_STAR) {
            (*token_index)++;
            left = create_ast_node(AST_MUL, NULL, left, parse_factor(tokens, token_index));
        } else if (tok->type == T_SLASH) {
            (*token_index)++;
            left = create_ast_node(AST_DIV, NULL, left, parse_factor(tokens, token_index));
        } else if (tok->type == T_PERCENT) {
            (*token_index)++;
            left = create_ast_node(AST_MOD, NULL, left, parse_factor(tokens, token_index));
        } else {
            break;
        }
    }

    return left;
}

struct ast_node* parse_exp(struct token *tokens, int *token_index)
{
    return parse_comma(tokens, token_index);
}

struct ast_node* parse_comma(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_assignment(tokens, token_index);

    while (tokens[*token_index].type == T_COMMA) {
        (*token_index)++;
        left = create_ast_node(AST_COMMA, NULL, left, parse_assignment(tokens, token_index));
    }

    return left;
}

struct ast_node* parse_assignment(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_conditional(tokens, token_index);
    TokenType op = tokens[*token_index].type;

    if (op == T_ASSIGN || op == T_PLUS_ASSIGN || op == T_MINUS_ASSIGN ||
        op == T_STAR_ASSIGN || op == T_SLASH_ASSIGN || op == T_PERCENT_ASSIGN ||
        op == T_AMPERSAND_ASSIGN || op == T_PIPE_ASSIGN || op == T_CARET_ASSIGN ||
        op == T_SHIFT_LEFT_ASSIGN || op == T_SHIFT_RIGHT_ASSIGN) {
        if (left->type != AST_IDENTIFIER &&
            left->type != AST_DEREFERENCE &&
            left->type != AST_FIELD_ACCESS &&
            left->type != AST_ARRAY_SUBSCRIPT) {
            parse_error_at(&tokens[*token_index], "left side of assignment must be an identifier");
        }

        SourceLocation operator_location = tokens[*token_index].location;
        (*token_index)++;
        struct ast_node *right = parse_assignment(tokens, token_index);

        if (op == T_ASSIGN) {
            return create_ast_node_at(AST_ASSIGN, NULL, left, right, operator_location);
        }
        if (!is_repeatable(left)) {
            parse_error_at(&tokens[*token_index],
                "compound assignment target must not have side effects");
        }

        ASTNodeType binop = AST_ADD;
        if (op == T_MINUS_ASSIGN) binop = AST_SUB;
        else if (op == T_STAR_ASSIGN) binop = AST_MUL;
        else if (op == T_SLASH_ASSIGN) binop = AST_DIV;
        else if (op == T_PERCENT_ASSIGN) binop = AST_MOD;
        else if (op == T_AMPERSAND_ASSIGN) binop = AST_BITWISE_AND;
        else if (op == T_PIPE_ASSIGN) binop = AST_BITWISE_OR;
        else if (op == T_CARET_ASSIGN) binop = AST_BITWISE_XOR;
        else if (op == T_SHIFT_LEFT_ASSIGN) binop = AST_SHIFT_LEFT;
        else if (op == T_SHIFT_RIGHT_ASSIGN) binop = AST_SHIFT_RIGHT;

        return create_ast_node(
            AST_ASSIGN,
            NULL,
            left,
            create_ast_node_at(binop, NULL,
                clone_expression(left),
                right,
                operator_location)
        );
    }

    return left;
}

struct ast_node* parse_conditional(struct token *tokens, int *token_index)
{
    struct ast_node *cond = parse_logical_or(tokens, token_index);

    if (tokens[*token_index].type == T_QUESTION) {
        (*token_index)++;
        struct ast_node *then_exp = parse_exp(tokens, token_index);

        if (tokens[*token_index].type != T_COLON) {
            parse_error_at(&tokens[*token_index],
                "expected ':' in conditional expression, found '%s'",
                tokens[*token_index].value);
        } else {
            (*token_index)++;
        }

        struct ast_node *else_exp = parse_conditional(tokens, token_index);
        return create_ast_node(
            AST_CONDITIONAL,
            NULL,
            cond,
            create_ast_node(AST_CONDITIONAL_BRANCHES, NULL, then_exp, else_exp)
        );
    }

    return cond;
}

struct ast_node* parse_logical_or(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_logical_and(tokens, token_index);

    while (tokens[*token_index].type == T_LOGICAL_OR) {
        (*token_index)++;
        left = create_ast_node(AST_LOGICAL_OR, NULL, left, parse_logical_and(tokens, token_index));
    }

    return left;
}

struct ast_node* parse_logical_and(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_bitwise_or(tokens, token_index);

    while (tokens[*token_index].type == T_LOGICAL_AND) {
        (*token_index)++;
        left = create_ast_node(AST_LOGICAL_AND, NULL, left, parse_bitwise_or(tokens, token_index));
    }

    return left;
}

struct ast_node* parse_bitwise_or(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_bitwise_xor(tokens, token_index);

    while (tokens[*token_index].type == T_PIPE) {
        (*token_index)++;
        left = create_ast_node(AST_BITWISE_OR, NULL, left, parse_bitwise_xor(tokens, token_index));
    }

    return left;
}

struct ast_node* parse_bitwise_xor(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_bitwise_and(tokens, token_index);

    while (tokens[*token_index].type == T_CARET) {
        (*token_index)++;
        left = create_ast_node(AST_BITWISE_XOR, NULL, left, parse_bitwise_and(tokens, token_index));
    }

    return left;
}

struct ast_node* parse_bitwise_and(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_equality(tokens, token_index);

    while (tokens[*token_index].type == T_AMPERSAND) {
        (*token_index)++;
        left = create_ast_node(AST_BITWISE_AND, NULL, left, parse_equality(tokens, token_index));
    }

    return left;
}

struct ast_node* parse_equality(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_relational(tokens, token_index);

    while (1) {
        struct token *tok = &tokens[*token_index];

        if (tok->type == T_EQUAL) {
            (*token_index)++;
            left = create_ast_node(AST_EQUAL, NULL, left, parse_relational(tokens, token_index));
        } else if (tok->type == T_NOT_EQUAL) {
            (*token_index)++;
            left = create_ast_node(AST_NOT_EQUAL, NULL, left, parse_relational(tokens, token_index));
        } else {
            break;
        }
    }

    return left;
}

struct ast_node* parse_relational(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_shift(tokens, token_index);

    while (1) {
        struct token *tok = &tokens[*token_index];

        if (tok->type == T_LESS) {
            (*token_index)++;
            left = create_ast_node(AST_LESS, NULL, left, parse_shift(tokens, token_index));
        } else if (tok->type == T_LESS_EQUAL) {
            (*token_index)++;
            left = create_ast_node(AST_LESS_EQUAL, NULL, left, parse_shift(tokens, token_index));
        } else if (tok->type == T_GREATER) {
            (*token_index)++;
            left = create_ast_node(AST_GREATER, NULL, left, parse_shift(tokens, token_index));
        } else if (tok->type == T_GREATER_EQUAL) {
            (*token_index)++;
            left = create_ast_node(AST_GREATER_EQUAL, NULL, left, parse_shift(tokens, token_index));
        } else {
            break;
        }
    }

    return left;
}

struct ast_node* parse_shift(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_additive(tokens, token_index);

    while (1) {
        struct token *tok = &tokens[*token_index];

        if (tok->type == T_SHIFT_LEFT) {
            (*token_index)++;
            left = create_ast_node(AST_SHIFT_LEFT, NULL, left, parse_additive(tokens, token_index));
        } else if (tok->type == T_SHIFT_RIGHT) {
            (*token_index)++;
            left = create_ast_node(AST_SHIFT_RIGHT, NULL, left, parse_additive(tokens, token_index));
        } else {
            break;
        }
    }

    return left;
}

struct ast_node* parse_additive(struct token *tokens, int *token_index)
{
    struct ast_node *left = parse_term(tokens, token_index);

    while (1) {
        struct token *tok = &tokens[*token_index];

        if (tok->type == T_PLUS) {
            (*token_index)++;
            left = create_ast_node(AST_ADD, NULL, left, parse_term(tokens, token_index));
        } else if (tok->type == T_MINUS) {
            (*token_index)++;
            left = create_ast_node(AST_SUB, NULL, left, parse_term(tokens, token_index));
        } else {
            break;
        }
    }

    return left;
}

struct ast_node* create_ast_node(ASTNodeType type, char *value, struct ast_node *left, struct ast_node *right)
{
    SourceLocation location = {0, 0, NULL};

    if (left) {
        location = left->location;
    } else if (right) {
        location = right->location;
    }
    return create_ast_node_at(type, value, left, right, location);
}

struct ast_node* create_ast_node_at(ASTNodeType type, char *value, struct ast_node *left, struct ast_node *right, SourceLocation location)
{
    /*
     * calloc, not malloc: fields the parser does not set (the resolved type and
     * symbol, both filled in later by semantic analysis) must start NULL rather
     * than holding whatever was on the heap.
     */
    struct ast_node *node = calloc(1, sizeof(struct ast_node));
    if (!node) {
        perror("Error allocating AST node");
        exit(EXIT_FAILURE);
    }

    node->type = type;
    node->data_type = TYPE_INVALID;
    node->pointer_depth = 0;
    node->array_length = 0;
    node->string_label = 0;
    node->struct_name = NULL;
    node->ty = NULL;
    node->sym = NULL;
    node->location = location;
    node->value = value ? strdup(value) : NULL;
    node->left = left;
    node->right = right;
    return node;
}

void free_ast_node(struct ast_node *node)
{
    if (node) {
        free_ast_node(node->left);
        free_ast_node(node->right);
        free(node->value);
        free(node->struct_name);
        free(node);
    }
}
