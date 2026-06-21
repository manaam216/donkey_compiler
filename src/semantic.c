#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "defs.h"
#include "decl.h"

#define MAX_SYMBOLS 256

struct global_symbol {
    const char *name;
    int is_function;
    int parameter_count;
};

struct local_symbol {
    const char *name;
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

static void add_global(const char *name, int is_function, int parameter_count)
{
    int existing = find_global(name);

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
    globals[global_count].parameter_count = parameter_count;
    global_count++;
}

static void add_local(const char *name)
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
            add_local(node->value);
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
        add_global(node->value, 1, count_list(node->left, AST_PARAM_LIST));
    } else if (node->type == AST_GLOBAL_DECL) {
        add_global(node->value, 0, 0);
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
                add_local(param->left->value);
            }
        }
        analyze_block(node->right, 0);
        current_function = NULL;
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
    return error_count == 0;
}
