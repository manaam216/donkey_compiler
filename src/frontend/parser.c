/*
 * Parser: diagnostics and recovery, the typedef and enum tables, type names,
 * declarators, initializers, and AST node allocation -- the pieces every level
 * of the grammar needs.
 */
#include "defs.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decl.h"
#include "diag.h"
#include "parser_internal.h"

const char *parser_source_path;

/*
 * Report a syntax error and keep going. The caller is expected to put the
 * parser back on a token it can resume from -- see synchronize() -- so a single
 * missing semicolon does not hide everything after it.
 */
void parse_error_at(struct token *token, const char *format, ...)
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
void synchronize(struct token *tokens, int *token_index)
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
int is_declaration_prefix(TokenType type)
{
    return type == T_EXTERN || type == T_STATIC ||
        type == T_CONST || type == T_VOLATILE;
}
void skip_declaration_prefixes(struct token *tokens, int *token_index)
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
void add_typedef(const char *name, const char *type_name, int pointer_depth)
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
const struct typedef_name *find_typedef(const char *name)
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


static struct enum_constant *enum_constants;
static int enum_constant_count;
static int enum_constant_capacity;
void add_enum_constant(const char *name, long value)
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
const struct enum_constant *find_enum_constant(const char *name)
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
int parse_enum_specifier(struct token *tokens, int *token_index)
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
const char* parse_type_name(struct token *tokens, int *token_index)
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
    if (base_type == T_FLOAT && !has_sign) {
        (*token_index)++;
        return "float";
    }
    if (base_type == T_DOUBLE && !has_sign) {
        (*token_index)++;
        return "double";
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
CType type_from_name(const char *name)
{
    if (!name) return TYPE_INT;
    if (strcmp(name, "void") == 0) return TYPE_VOID;
    if (strcmp(name, "float") == 0) return TYPE_FLOAT;
    if (strcmp(name, "double") == 0) return TYPE_DOUBLE;
    if (strcmp(name, "char") == 0) return TYPE_CHAR;
    if (strcmp(name, "uchar") == 0) return TYPE_UCHAR;
    if (strcmp(name, "short") == 0) return TYPE_SHORT;
    if (strcmp(name, "ushort") == 0) return TYPE_USHORT;
    if (strcmp(name, "uint") == 0) return TYPE_UINT;
    if (strcmp(name, "long") == 0) return TYPE_LONG;
    if (strcmp(name, "ulong") == 0) return TYPE_ULONG;
    return TYPE_INT;
}
int parse_pointer_stars(struct token *tokens, int *token_index)
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
int parse_array_length(struct token *tokens, int *token_index)
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
int parse_array_dims(struct token *tokens, int *token_index, int *dims)
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

/*
 * Whether an expression can be evaluated twice safely, which the rewrite above
 * requires. A call could do anything, so a target containing one is refused
 * rather than silently run twice.
 */

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
char *parse_function_pointer_declarator(struct token *tokens,
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

/*
 * The general C declarator grammar.
 *
 * A declarator derives its type from the base inside-out, and parentheses
 * change the nesting: `int *a[10]` is an array of pointers, `int (*a)[10]` a
 * pointer to an array. The two differ only in where the parentheses sit, so a
 * left-to-right reading cannot tell them apart.
 *
 * The trick for handling the parenthesised form is to parse the inner
 * declarator twice. On seeing '(' the inner text is skipped so the suffix
 * after the closing parenthesis can be applied first -- that suffix binds to
 * whatever the inner declarator names -- and only then is the inner text
 * parsed for real. Each step is appended to a list in the order it applies.
 */

void derive(struct ast_node *node, int kind, int length,
    struct token *tokens, int *token_index)
{
    if (node->derivation_count >= DONKEY_MAX_DERIVATIONS) {
        parse_error_at(&tokens[*token_index],
            "declarator nests more than %d levels deep", DONKEY_MAX_DERIVATIONS);
        return;
    }
    node->derivations[node->derivation_count].kind = kind;
    node->derivations[node->derivation_count].length = length;
    node->derivation_count++;
}

/* Step over a parenthesised run, leaving *token_index just past its close. */
void skip_parenthesised(struct token *tokens, int *token_index)
{
    int depth = 0;

    do {
        if (tokens[*token_index].type == T_OPENPAREN) {
            depth++;
        } else if (tokens[*token_index].type == T_CLOSEPAREN) {
            depth--;
        }
        (*token_index)++;
    } while (depth > 0 && tokens[*token_index - 1].type != T_EOF);
}

/* The [] and () that follow a declarator, applied nearest-first. */
void parse_declarator_suffix(struct ast_node *node, struct token *tokens,
    int *token_index)
{
    for (;;) {
        if (tokens[*token_index].type == T_OPENBRACKET) {
            int length = parse_array_length(tokens, token_index);

            derive(node, DERIVE_ARRAY, length, tokens, token_index);
        } else if (tokens[*token_index].type == T_OPENPAREN) {
            skip_parenthesised(tokens, token_index);
            derive(node, DERIVE_FUNCTION, 0, tokens, token_index);
        } else {
            return;
        }
    }
}
int parse_declarator(struct ast_node *node, struct token *tokens,
    int *token_index, struct declarator_result *result)
{
    int stars = 0;
    int i;

    while (tokens[*token_index].type == T_STAR) {
        stars++;
        (*token_index)++;
        skip_declaration_prefixes(tokens, token_index);
    }
    for (i = 0; i < stars; i++) {
        derive(node, DERIVE_POINTER, 0, tokens, token_index);
    }

    if (tokens[*token_index].type == T_OPENPAREN) {
        int inner = *token_index + 1;
        int after;

        skip_parenthesised(tokens, token_index);
        after = *token_index;

        /* The suffix binds to what the inner declarator names, so it goes first. */
        parse_declarator_suffix(node, tokens, token_index);

        {
            int saved = *token_index;
            int inner_index = inner;

            *token_index = inner_index;
            if (!parse_declarator(node, tokens, token_index, result)) {
                return 0;
            }
            *token_index = saved;
        }
        (void)after;
        return 1;
    }

    if (tokens[*token_index].type != T_IDENTIFIER) {
        return 0;
    }
    result->name = tokens[*token_index].value;
    result->location = tokens[*token_index].location;
    (*token_index)++;

    parse_declarator_suffix(node, tokens, token_index);
    return 1;
}

/*
 * Postfix operators -- [], ., and -> -- applied to whatever precedes them.
 *
 * These used to be handled only after an identifier, which meant a
 * parenthesised expression could not carry them: neither `(*p)[1]` for a
 * pointer to an array nor `(struct P){1, 2}.x` for a compound literal would
 * parse. They bind to any primary expression, so that is where they belong.
 */

struct ast_node* parse_initializer_list(struct token *tokens, int *token_index)
{
    int designator_index = -1;
    char *designator_field = NULL;

    if (tokens[*token_index].type == T_CLOSEBRACE) {
        return NULL;
    }

    /*
     * A designator says where the element goes: `[2] =` for an array element,
     * `.field =` for a struct member. Without one an element follows the
     * previous, which is what makes a plain list work.
     */
    if (tokens[*token_index].type == T_OPENBRACKET) {
        (*token_index)++;
        if (tokens[*token_index].type == T_INTLIT) {
            designator_index = atoi(tokens[*token_index].value);
            (*token_index)++;
        } else {
            parse_error_at(&tokens[*token_index],
                "array designator must be a constant index");
        }
        if (tokens[*token_index].type == T_CLOSEBRACKET) {
            (*token_index)++;
        } else {
            parse_error_at(&tokens[*token_index], "expected ']' after the index");
        }
        if (tokens[*token_index].type == T_ASSIGN) {
            (*token_index)++;
        } else {
            parse_error_at(&tokens[*token_index], "expected '=' after the designator");
        }
    } else if (tokens[*token_index].type == T_DOT) {
        (*token_index)++;
        if (tokens[*token_index].type == T_IDENTIFIER) {
            designator_field = tokens[*token_index].value;
            (*token_index)++;
        } else {
            parse_error_at(&tokens[*token_index],
                "expected a field name after '.'");
        }
        if (tokens[*token_index].type == T_ASSIGN) {
            (*token_index)++;
        } else {
            parse_error_at(&tokens[*token_index], "expected '=' after the designator");
        }
    }

    struct ast_node *initializer = parse_initializer(tokens, token_index);
    struct ast_node *rest = NULL;

    if (initializer) {
        initializer->designator_index = designator_index;
        initializer->designator_field =
            designator_field ? strdup(designator_field) : NULL;
    }
    if (tokens[*token_index].type == T_COMMA) {
        (*token_index)++;
        rest = parse_initializer_list(tokens, token_index);
    }

    return create_ast_node(AST_INITIALIZER_LIST, NULL, initializer, rest);
}
struct ast_node* parse_initializer(struct token *tokens, int *token_index)
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
int is_typedef_name(struct token *token)
{
    return token->type == T_IDENTIFIER && find_typedef(token->value) != NULL;
}
int is_type_start(TokenType type)
{
    return type == T_CHAR || type == T_SHORT || type == T_INT ||
        type == T_LONG || type == T_SIGNED || type == T_UNSIGNED ||
        type == T_STRUCT || type == T_UNION || type == T_ENUM ||
        type == T_VOID || type == T_FLOAT || type == T_DOUBLE ||
        is_declaration_prefix(type);
}
const char *parse_struct_name(struct token *tokens, int *token_index)
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

/*
 * do { body } while (condition);
 *
 * The body always runs once, so the condition is tested at the bottom. It is
 * stored the same way as a while loop -- condition on the left, body on the
 * right -- and the node type tells the code generator which order to emit.
 */

/*
 * `case <constant>:` and `default:`. Both introduce a labelled point inside a
 * switch body; the statement they label is parsed as their child, so a run of
 * cases falling into one another nests naturally.
 */

/*
 * One declaration may introduce several names: `int a, b = 2;`. The base type
 * is parsed once and each declarator after it produces its own AST_DECL, with
 * the results chained into a statement list so every later pass -- which
 * already walks statement lists -- sees them without changes.
 */
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
    node->designator_index = -1;
    node->designator_field = NULL;
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
        free(node->designator_field);
        free(node);
    }
}
