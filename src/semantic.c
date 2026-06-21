#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "defs.h"
#include "decl.h"

#define MAX_SYMBOLS 256

struct global_symbol {
    const char *name;
    int is_function;
    CType type;
    int parameter_count;
    CType parameter_types[64];
};

struct local_symbol {
    const char *name;
    CType type;
    int depth;
};

static struct global_symbol globals[MAX_SYMBOLS];
static struct local_symbol locals[MAX_SYMBOLS];
static int global_count;
static int local_count;
static int scope_depth;
static int loop_depth;
static int error_count;
static const char *current_function;
static CType current_return_type;

static const char *semantic_type_name(CType type)
{
    switch (type) {
        case TYPE_CHAR: return "char";
        case TYPE_UCHAR: return "uchar";
        case TYPE_SHORT: return "short";
        case TYPE_USHORT: return "ushort";
        case TYPE_UINT: return "uint";
        case TYPE_LONG: return "long";
        case TYPE_ULONG: return "ulong";
        default: return "int";
    }
}

static CType semantic_type_from_name(const char *name)
{
    if (!name) return TYPE_INVALID;
    if (strcmp(name, "char") == 0) return TYPE_CHAR;
    if (strcmp(name, "uchar") == 0) return TYPE_UCHAR;
    if (strcmp(name, "short") == 0) return TYPE_SHORT;
    if (strcmp(name, "ushort") == 0) return TYPE_USHORT;
    if (strcmp(name, "uint") == 0) return TYPE_UINT;
    if (strcmp(name, "long") == 0) return TYPE_LONG;
    if (strcmp(name, "ulong") == 0) return TYPE_ULONG;
    return TYPE_INT;
}

static void semantic_error(const char *format, ...)
{
    va_list args;

    fprintf(stderr, "Semantic error");
    if (current_function) {
        fprintf(stderr, " in function '%s'", current_function);
    }
    fprintf(stderr, ": ");

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    error_count++;
}

static int count_list(struct ast_node *node, ASTNodeType list_type)
{
    int count = 0;

    while (node) {
        if (node->type != list_type) {
            return count + 1;
        }
        count++;
        node = node->right;
    }
    return count;
}

static int find_global(const char *name)
{
    int i;

    for (i = 0; i < global_count; i++) {
        if (strcmp(globals[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_local(const char *name)
{
    int i;

    for (i = local_count - 1; i >= 0; i--) {
        if (strcmp(locals[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void add_global(struct ast_node *node)
{
    const char *name = node->value;
    int is_function = node->type == AST_FUNCTION;
    int existing = find_global(name);
    struct ast_node *param;

    if (existing >= 0) {
        semantic_error("duplicate top-level declaration of '%s'", name);
        return;
    }
    if (global_count >= MAX_SYMBOLS) {
        semantic_error("too many top-level declarations");
        return;
    }

    globals[global_count].name = name;
    globals[global_count].is_function = is_function;
    globals[global_count].type = node->data_type;
    globals[global_count].parameter_count = 0;
    if (is_function) {
        for (param = node->left; param; param = param->right) {
            if (globals[global_count].parameter_count >= 64) {
                semantic_error("function '%s' has too many parameters", name);
                break;
            }
            globals[global_count].parameter_types[globals[global_count].parameter_count++] =
                param->left->data_type;
        }
    }
    global_count++;
}

static void add_local(const char *name, CType type)
{
    int existing = find_local(name);

    if (existing >= 0) {
        if (locals[existing].depth == scope_depth) {
            semantic_error("duplicate declaration of '%s'", name);
        } else {
            semantic_error("variable shadowing is not supported for '%s'", name);
        }
        return;
    }
    if (local_count >= MAX_SYMBOLS) {
        semantic_error("too many local declarations");
        return;
    }

    locals[local_count].name = name;
    locals[local_count].type = type;
    locals[local_count].depth = scope_depth;
    local_count++;
}

static void enter_scope(void)
{
    scope_depth++;
}

static void leave_scope(void)
{
    while (local_count > 0 && locals[local_count - 1].depth == scope_depth) {
        local_count--;
    }
    scope_depth--;
}

static void analyze_expression(struct ast_node *node);
static void analyze_statement(struct ast_node *node);

static void analyze_expression(struct ast_node *node)
{
    int symbol;
    int actual_count;

    if (!node) {
        return;
    }

    switch (node->type) {
        case AST_INTLIT:
        case AST_SIZEOF:
            return;
        case AST_IDENTIFIER:
            symbol = find_global(node->value);
            if (find_local(node->value) < 0 &&
                (symbol < 0 || globals[symbol].is_function)) {
                semantic_error("use of undeclared variable '%s'", node->value);
            }
            return;
        case AST_CALL:
            symbol = find_global(node->value);
            if (find_local(node->value) >= 0) {
                semantic_error("called object '%s' is not a function", node->value);
            } else if (symbol < 0) {
                semantic_error("call to undeclared function '%s'", node->value);
            } else if (!globals[symbol].is_function) {
                semantic_error("called object '%s' is not a function", node->value);
            } else {
                actual_count = count_list(node->left, AST_ARG_LIST);
                if (actual_count != globals[symbol].parameter_count) {
                    semantic_error("function '%s' expects %d argument(s), but %d provided",
                        node->value, globals[symbol].parameter_count, actual_count);
                }
            }
            for (struct ast_node *arg = node->left; arg; arg = arg->right) {
                analyze_expression(arg->type == AST_ARG_LIST ? arg->left : arg);
                if (arg->type != AST_ARG_LIST) {
                    break;
                }
            }
            return;
        case AST_CAST:
        case AST_NEGATION:
        case AST_BITWISE_COMPLEMENT:
        case AST_LOGICAL_NEGATION:
        case AST_PRE_INCREMENT:
        case AST_PRE_DECREMENT:
        case AST_POST_INCREMENT:
        case AST_POST_DECREMENT:
            analyze_expression(node->left);
            return;
        case AST_CONDITIONAL:
            analyze_expression(node->left);
            analyze_expression(node->right->left);
            analyze_expression(node->right->right);
            return;
        default:
            analyze_expression(node->left);
            analyze_expression(node->right);
            return;
    }
}

static void analyze_block(struct ast_node *node, int creates_scope)
{
    if (creates_scope) {
        enter_scope();
    }
    if (node) {
        analyze_statement(node->left);
    }
    if (creates_scope) {
        leave_scope();
    }
}

static void analyze_statement(struct ast_node *node)
{
    struct ast_node *parts;
    struct ast_node *condition_and_post;

    if (!node) {
        return;
    }

    switch (node->type) {
        case AST_BLOCK:
            analyze_block(node, 1);
            break;
        case AST_STATEMENT_LIST:
            analyze_statement(node->left);
            analyze_statement(node->right);
            break;
        case AST_DECL:
            add_local(node->value, node->data_type);
            analyze_expression(node->left);
            break;
        case AST_EXPR_STMT:
        case AST_RETURN:
            analyze_expression(node->left);
            break;
        case AST_IF:
            analyze_expression(node->left);
            analyze_statement(node->right->left);
            analyze_statement(node->right->right);
            break;
        case AST_WHILE:
            analyze_expression(node->left);
            loop_depth++;
            analyze_statement(node->right);
            loop_depth--;
            break;
        case AST_FOR:
            enter_scope();
            parts = node->left;
            condition_and_post = parts->right;
            if (parts->left && parts->left->type == AST_DECL) {
                analyze_statement(parts->left);
            } else {
                analyze_expression(parts->left);
            }
            analyze_expression(condition_and_post->left);
            analyze_expression(condition_and_post->right);
            loop_depth++;
            analyze_statement(node->right);
            loop_depth--;
            leave_scope();
            break;
        case AST_BREAK:
            if (loop_depth == 0) {
                semantic_error("'break' statement is not inside a loop");
            }
            break;
        case AST_CONTINUE:
            if (loop_depth == 0) {
                semantic_error("'continue' statement is not inside a loop");
            }
            break;
        default:
            analyze_expression(node);
            break;
    }
}

static void collect_top_level(struct ast_node *node)
{
    if (!node) {
        return;
    }
    if (node->type == AST_PROGRAM) {
        collect_top_level(node->left);
    } else if (node->type == AST_FUNCTION_LIST) {
        collect_top_level(node->left);
        collect_top_level(node->right);
    } else if (node->type == AST_FUNCTION) {
        add_global(node);
    } else if (node->type == AST_GLOBAL_DECL) {
        add_global(node);
    }
}

static int is_constant_expression(struct ast_node *node)
{
    if (!node) {
        return 1;
    }

    switch (node->type) {
        case AST_INTLIT:
        case AST_SIZEOF:
            return 1;
        case AST_NEGATION:
        case AST_BITWISE_COMPLEMENT:
        case AST_LOGICAL_NEGATION:
        case AST_CAST:
            return is_constant_expression(node->left);
        case AST_ADD:
        case AST_SUB:
        case AST_MUL:
        case AST_DIV:
        case AST_MOD:
        case AST_SHIFT_LEFT:
        case AST_SHIFT_RIGHT:
        case AST_BITWISE_AND:
        case AST_BITWISE_OR:
        case AST_BITWISE_XOR:
        case AST_LOGICAL_AND:
        case AST_LOGICAL_OR:
        case AST_EQUAL:
        case AST_NOT_EQUAL:
        case AST_LESS:
        case AST_LESS_EQUAL:
        case AST_GREATER:
        case AST_GREATER_EQUAL:
        case AST_COMMA:
            return is_constant_expression(node->left) && is_constant_expression(node->right);
        case AST_CONDITIONAL:
            return is_constant_expression(node->left) &&
                is_constant_expression(node->right->left) &&
                is_constant_expression(node->right->right);
        default:
            return 0;
    }
}

static void analyze_top_level(struct ast_node *node)
{
    struct ast_node *param;

    if (!node) {
        return;
    }
    if (node->type == AST_PROGRAM) {
        analyze_top_level(node->left);
    } else if (node->type == AST_FUNCTION_LIST) {
        analyze_top_level(node->left);
        analyze_top_level(node->right);
    } else if (node->type == AST_GLOBAL_DECL) {
        if (!is_constant_expression(node->left)) {
            semantic_error("initializer for global '%s' is not a constant expression", node->value);
        }
    } else if (node->type == AST_FUNCTION) {
        current_function = node->value;
        local_count = 0;
        scope_depth = 1;
        loop_depth = 0;

        for (param = node->left; param; param = param->right) {
            if (param->type == AST_PARAM_LIST) {
                add_local(param->left->value, param->left->data_type);
            }
        }
        analyze_block(node->right, 0);
        current_function = NULL;
    }
}

static CType integer_promotion(CType type)
{
    if (type == TYPE_CHAR || type == TYPE_UCHAR ||
        type == TYPE_SHORT || type == TYPE_USHORT) {
        return TYPE_INT;
    }
    return type;
}

static CType usual_arithmetic_type(CType left, CType right)
{
    left = integer_promotion(left);
    right = integer_promotion(right);
    if (left == right) return left;
    if (left == TYPE_ULONG || right == TYPE_ULONG) return TYPE_ULONG;
    if ((left == TYPE_LONG && right == TYPE_UINT) ||
        (left == TYPE_UINT && right == TYPE_LONG)) return TYPE_ULONG;
    if (left == TYPE_UINT || right == TYPE_UINT) return TYPE_UINT;
    if (left == TYPE_LONG || right == TYPE_LONG) return TYPE_LONG;
    return TYPE_INT;
}

static void insert_conversion(struct ast_node **slot, CType target)
{
    struct ast_node *cast;

    if (!slot || !*slot || target == TYPE_INVALID || (*slot)->data_type == target) {
        return;
    }
    cast = create_ast_node(AST_CAST, (char *)semantic_type_name(target), *slot, NULL);
    cast->data_type = target;
    *slot = cast;
}

static CType check_expression_type(struct ast_node **slot);

static CType check_binary_type(struct ast_node *node)
{
    CType left = check_expression_type(&node->left);
    CType right = check_expression_type(&node->right);
    CType common;

    if (left == TYPE_INVALID || right == TYPE_INVALID) {
        return node->data_type = TYPE_INVALID;
    }
    if (node->type == AST_LOGICAL_AND || node->type == AST_LOGICAL_OR) {
        return node->data_type = TYPE_INT;
    }
    if (node->type == AST_SHIFT_LEFT || node->type == AST_SHIFT_RIGHT) {
        left = integer_promotion(left);
        right = integer_promotion(right);
        insert_conversion(&node->left, left);
        insert_conversion(&node->right, right);
        return node->data_type = left;
    }

    common = usual_arithmetic_type(left, right);
    insert_conversion(&node->left, common);
    insert_conversion(&node->right, common);
    if (node->type == AST_EQUAL || node->type == AST_NOT_EQUAL ||
        node->type == AST_LESS || node->type == AST_LESS_EQUAL ||
        node->type == AST_GREATER || node->type == AST_GREATER_EQUAL) {
        return node->data_type = TYPE_INT;
    }
    return node->data_type = common;
}

static CType check_expression_type(struct ast_node **slot)
{
    struct ast_node *node;
    struct ast_node *argument;
    int local;
    int global;
    int argument_index;
    CType left;
    CType right;
    CType common;

    if (!slot || !*slot) return TYPE_INVALID;
    node = *slot;

    switch (node->type) {
        case AST_INTLIT:
            return node->data_type = TYPE_INT;
        case AST_SIZEOF:
            return node->data_type = TYPE_UINT;
        case AST_IDENTIFIER:
            local = find_local(node->value);
            global = find_global(node->value);
            if (local >= 0) return node->data_type = locals[local].type;
            if (global >= 0 && !globals[global].is_function)
                return node->data_type = globals[global].type;
            return node->data_type = TYPE_INVALID;
        case AST_CALL:
            global = find_global(node->value);
            argument_index = 0;
            for (argument = node->left; argument; argument = argument->right) {
                check_expression_type(&argument->left);
                if (global >= 0 && globals[global].is_function &&
                    argument_index < globals[global].parameter_count) {
                    insert_conversion(&argument->left,
                        globals[global].parameter_types[argument_index]);
                }
                argument_index++;
            }
            if (global >= 0 && globals[global].is_function)
                return node->data_type = globals[global].type;
            return node->data_type = TYPE_INVALID;
        case AST_CAST:
            check_expression_type(&node->left);
            if (node->data_type == TYPE_INVALID)
                node->data_type = semantic_type_from_name(node->value);
            return node->data_type;
        case AST_NEGATION:
        case AST_BITWISE_COMPLEMENT:
            left = integer_promotion(check_expression_type(&node->left));
            insert_conversion(&node->left, left);
            return node->data_type = left;
        case AST_LOGICAL_NEGATION:
            check_expression_type(&node->left);
            return node->data_type = TYPE_INT;
        case AST_PRE_INCREMENT:
        case AST_PRE_DECREMENT:
        case AST_POST_INCREMENT:
        case AST_POST_DECREMENT:
            return node->data_type = check_expression_type(&node->left);
        case AST_ASSIGN:
            left = check_expression_type(&node->left);
            check_expression_type(&node->right);
            insert_conversion(&node->right, left);
            return node->data_type = left;
        case AST_CONDITIONAL:
            check_expression_type(&node->left);
            left = check_expression_type(&node->right->left);
            right = check_expression_type(&node->right->right);
            common = usual_arithmetic_type(left, right);
            insert_conversion(&node->right->left, common);
            insert_conversion(&node->right->right, common);
            return node->data_type = common;
        case AST_COMMA:
            check_expression_type(&node->left);
            return node->data_type = check_expression_type(&node->right);
        default:
            return check_binary_type(node);
    }
}

static void check_statement_types(struct ast_node *node)
{
    struct ast_node *parts;
    struct ast_node *condition_and_post;

    if (!node) return;
    switch (node->type) {
        case AST_BLOCK:
            enter_scope();
            check_statement_types(node->left);
            leave_scope();
            break;
        case AST_STATEMENT_LIST:
            check_statement_types(node->left);
            check_statement_types(node->right);
            break;
        case AST_DECL:
            add_local(node->value, node->data_type);
            if (node->left) {
                check_expression_type(&node->left);
                insert_conversion(&node->left, node->data_type);
            }
            break;
        case AST_EXPR_STMT:
            check_expression_type(&node->left);
            break;
        case AST_RETURN:
            check_expression_type(&node->left);
            insert_conversion(&node->left, current_return_type);
            break;
        case AST_IF:
            check_expression_type(&node->left);
            check_statement_types(node->right->left);
            check_statement_types(node->right->right);
            break;
        case AST_WHILE:
            check_expression_type(&node->left);
            check_statement_types(node->right);
            break;
        case AST_FOR:
            enter_scope();
            parts = node->left;
            condition_and_post = parts->right;
            if (parts->left && parts->left->type == AST_DECL)
                check_statement_types(parts->left);
            else
                check_expression_type(&parts->left);
            check_expression_type(&condition_and_post->left);
            check_expression_type(&condition_and_post->right);
            check_statement_types(node->right);
            leave_scope();
            break;
        default:
            break;
    }
}

static void check_top_level_types(struct ast_node *node)
{
    struct ast_node *param;

    if (!node) return;
    if (node->type == AST_PROGRAM) {
        check_top_level_types(node->left);
    } else if (node->type == AST_FUNCTION_LIST) {
        check_top_level_types(node->left);
        check_top_level_types(node->right);
    } else if (node->type == AST_GLOBAL_DECL) {
        if (node->left) {
            check_expression_type(&node->left);
            insert_conversion(&node->left, node->data_type);
        }
    } else if (node->type == AST_FUNCTION) {
        current_return_type = node->data_type;
        local_count = 0;
        scope_depth = 1;
        for (param = node->left; param; param = param->right)
            add_local(param->left->value, param->left->data_type);
        if (node->right) check_statement_types(node->right->left);
    }
}

int semantic_analyze(struct ast_node *ast)
{
    global_count = 0;
    local_count = 0;
    scope_depth = 0;
    loop_depth = 0;
    error_count = 0;
    current_function = NULL;

    collect_top_level(ast);
    analyze_top_level(ast);
    if (error_count == 0) check_top_level_types(ast);
    return error_count == 0;
}
