#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "decl.h"
#include "type.h"
#include "symbol.h"

struct global_symbol {
    const char *name;
    struct Symbol *sym;
    int is_function;
    CType type;
    int pointer_depth;
    int array_length;
    const char *struct_name;
    int parameter_count;
    CType parameter_types[64];
    int parameter_pointer_depths[64];
};

struct local_symbol {
    const char *name;
    CType type;
    int pointer_depth;
    int array_length;
    const char *struct_name;
    int depth;
    struct Symbol *sym;
};

struct struct_field {
    const char *name;
    CType type;
    int pointer_depth;
    int offset;
};

struct struct_symbol {
    const char *name;
    int field_count;
    struct struct_field fields[64];
    struct Type *ty;            /* resolved layout: offsets, size, alignment */
};

/*
 * All analysis state lives in one context that the caller owns, rather than in
 * file-scope arrays with fixed capacities. This removes the MAX_SYMBOLS ceiling,
 * makes the pass re-runnable in-process (needed for unit tests), and is a step
 * toward compiling several translation units.
 */
struct sema_ctx {
    struct global_symbol *globals;
    int global_count;
    int global_capacity;

    struct local_symbol *locals;
    int local_count;
    int local_capacity;

    struct struct_symbol *structs;
    int struct_count;
    int struct_capacity;

    int scope_depth;
    int loop_depth;
    int error_count;

    /*
     * Stack layout for the function being analysed. frame_offset is the bytes
     * used by the enclosing scopes; leaving a scope rewinds it so disjoint
     * blocks reuse the same slots. frame_max is the high-water mark, which is
     * what the function actually needs to reserve.
     */
    int frame_offset;
    int frame_max;

    const char *current_function;
    CType current_return_type;
    int current_return_pointer_depth;
    const char *source_path;
};

static void semantic_error_at(struct sema_ctx *ctx, struct ast_node *node,
    const char *format, ...);

/*
 * Grow *items to hold at least one more element. Allocation failure is fatal:
 * there is no useful way to continue analysis without a symbol table.
 */
static void ensure_capacity(void **items, int count, int *capacity,
    size_t item_size)
{
    void *grown;
    int next;

    if (count < *capacity) {
        return;
    }

    next = *capacity ? *capacity * 2 : 64;
    grown = realloc(*items, (size_t)next * item_size);
    if (!grown) {
        fprintf(stderr, "Out of memory while growing the symbol table\n");
        exit(EXIT_FAILURE);
    }

    *items = grown;
    *capacity = next;
}

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

static void semantic_format_type(CType type, int pointer_depth, int array_length,
    char *buffer, size_t size)
{
    snprintf(buffer, size, "%s", semantic_type_name(type));
    while (pointer_depth-- > 0) {
        strncat(buffer, "*", size - strlen(buffer) - 1);
    }
    if (array_length > 0) {
        char suffix[32];
        snprintf(suffix, sizeof(suffix), "[%d]", array_length);
        strncat(buffer, suffix, size - strlen(buffer) - 1);
    }
}

static int semantic_type_matches(CType left_type, int left_pointer_depth,
    CType right_type, int right_pointer_depth)
{
    return left_type == right_type && left_pointer_depth == right_pointer_depth;
}

static int semantic_is_integer(CType type, int pointer_depth, int array_length)
{
    return type != TYPE_INVALID && pointer_depth == 0 && array_length == 0;
}

static int semantic_effective_pointer_depth(struct ast_node *node)
{
    if (!node) {
        return 0;
    }
    return node->pointer_depth + (node->array_length > 0 ? 1 : 0);
}

static int find_struct(struct sema_ctx *ctx, const char *name)
{
    int i;

    if (!name) return -1;
    for (i = 0; i < ctx->struct_count; i++) {
        if (strcmp(ctx->structs[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_struct_field(struct sema_ctx *ctx, int struct_index, const char *name)
{
    int i;

    if (struct_index < 0) return -1;
    for (i = 0; i < ctx->structs[struct_index].field_count; i++) {
        if (strcmp(ctx->structs[struct_index].fields[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/*
 * Turn the parser's syntactic record (base type name, pointer stars, array
 * length, struct tag) into a resolved type. Structs must already be collected,
 * which collect_top_level guarantees by visiting definitions first.
 */
static struct Type *base_type_for(struct sema_ctx *ctx, struct ast_node *node)
{
    if (node->struct_name) {
        int index = find_struct(ctx, node->struct_name);
        if (index >= 0 && ctx->structs[index].ty) {
            return ctx->structs[index].ty;
        }
        return ty_int;
    }
    return ty_from_name(semantic_type_name(node->data_type));
}

static struct Type *resolve_type(struct sema_ctx *ctx, struct ast_node *node)
{
    struct Type *type = base_type_for(ctx, node);
    int i;

    for (i = 0; i < node->pointer_depth; i++) {
        type = ty_pointer_to(type);
    }
    if (node->array_length > 0) {
        type = ty_array_of(type, node->array_length);
    }
    return type;
}

static void add_struct(struct sema_ctx *ctx, struct ast_node *node)
{
    struct ast_node *field;
    struct Type *struct_type;
    int index;

    if (find_struct(ctx, node->value) >= 0) {
        semantic_error_at(ctx, node, "duplicate struct definition '%s'", node->value);
        return;
    }
    ensure_capacity((void **)&ctx->structs, ctx->struct_count,
        &ctx->struct_capacity, sizeof(*ctx->structs));
    struct_type = ty_struct(node->value);
    node->ty = struct_type;
    ctx->structs[ctx->struct_count].name = node->value;
    ctx->structs[ctx->struct_count].field_count = 0;
    ctx->structs[ctx->struct_count].ty = struct_type;
    for (field = node->left; field; field = field->right) {
        struct ast_node *decl = field->left;
        int existing = find_struct_field(ctx, ctx->struct_count, decl->value);
        index = ctx->structs[ctx->struct_count].field_count;
        if (existing >= 0) {
            semantic_error_at(ctx, decl, "duplicate field '%s'", decl->value);
            continue;
        }
        if (index >= 64) {
            semantic_error_at(ctx, decl, "too many fields in struct '%s'", node->value);
            break;
        }
        ctx->structs[ctx->struct_count].fields[index].name = decl->value;
        ctx->structs[ctx->struct_count].fields[index].type = decl->data_type;
        ctx->structs[ctx->struct_count].fields[index].pointer_depth = decl->pointer_depth;
        ctx->structs[ctx->struct_count].field_count++;
        ty_add_member(struct_type, decl->value, resolve_type(ctx, decl));
    }

    /* Real offsets, padding, size, and alignment -- not field_index * 4. */
    ty_layout_struct(struct_type);
    for (index = 0; index < ctx->structs[ctx->struct_count].field_count; index++) {
        struct Member *member = ty_find_member(struct_type,
            ctx->structs[ctx->struct_count].fields[index].name);
        ctx->structs[ctx->struct_count].fields[index].offset = member ? member->offset : 0;
    }

    ctx->struct_count++;
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

static void semantic_error_at(struct sema_ctx *ctx, struct ast_node *node, const char *format, ...)
{
    va_list args;

    fprintf(stderr, "Semantic error");
    if (node && node->location.line > 0) {
        fprintf(stderr, " at %s:%d:%d", ctx->source_path,
            node->location.line, node->location.column);
    }
    if (ctx->current_function) {
        fprintf(stderr, " in function '%s'", ctx->current_function);
    }
    fprintf(stderr, ": ");

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    ctx->error_count++;
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

static struct ast_node *initializer_items(struct ast_node *node)
{
    if (!node || node->type != AST_INITIALIZER_LIST) {
        return NULL;
    }
    if (!node->left || node->left->type == AST_INITIALIZER_LIST) {
        return node->left;
    }
    return node;
}

static int find_global(struct sema_ctx *ctx, const char *name)
{
    int i;

    for (i = 0; i < ctx->global_count; i++) {
        if (strcmp(ctx->globals[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_local(struct sema_ctx *ctx, const char *name)
{
    int i;

    for (i = ctx->local_count - 1; i >= 0; i--) {
        if (strcmp(ctx->locals[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void add_global(struct sema_ctx *ctx, struct ast_node *node)
{
    const char *name = node->value;
    int is_function = node->type == AST_FUNCTION;
    int existing = find_global(ctx, name);
    struct ast_node *param;

    if (existing >= 0) {
        semantic_error_at(ctx, node, "duplicate top-level declaration of '%s'", name);
        return;
    }
    ensure_capacity((void **)&ctx->globals, ctx->global_count,
        &ctx->global_capacity, sizeof(*ctx->globals));
    if (node->struct_name && find_struct(ctx, node->struct_name) < 0) {
        semantic_error_at(ctx, node, "unknown struct type '%s'", node->struct_name);
        return;
    }

    node->ty = resolve_type(ctx, node);
    if (!node->sym) {
        node->sym = sym_new(name, is_function ? SYM_FUNCTION : SYM_GLOBAL, node->ty);
    }
    ctx->globals[ctx->global_count].sym = node->sym;
    ctx->globals[ctx->global_count].name = name;
    ctx->globals[ctx->global_count].is_function = is_function;
    ctx->globals[ctx->global_count].type = node->data_type;
    ctx->globals[ctx->global_count].pointer_depth = node->pointer_depth;
    ctx->globals[ctx->global_count].array_length = node->array_length;
    ctx->globals[ctx->global_count].struct_name = node->struct_name;
    ctx->globals[ctx->global_count].parameter_count = 0;
    if (is_function) {
        for (param = node->left; param; param = param->right) {
            if (ctx->globals[ctx->global_count].parameter_count >= 64) {
                semantic_error_at(ctx, node, "function '%s' has too many parameters", name);
                break;
            }
            param->left->ty = resolve_type(ctx, param->left);
            ctx->globals[ctx->global_count].parameter_types[ctx->globals[ctx->global_count].parameter_count++] =
                param->left->data_type;
            ctx->globals[ctx->global_count].parameter_pointer_depths[ctx->globals[ctx->global_count].parameter_count - 1] =
                param->left->pointer_depth;
        }
    }
    ctx->global_count++;
}

static void add_local(struct sema_ctx *ctx, struct ast_node *node, CType type)
{
    const char *name = node->value;
    int existing = find_local(ctx, name);

    /*
     * Redeclaring a name in the same scope is an error; redeclaring it in an
     * inner scope shadows the outer one. Shadowing works because each
     * declaration now gets its own Symbol, so the two variables have distinct
     * storage even though they share a name. find_local scans innermost-first,
     * so references resolve to the nearest declaration.
     */
    if (existing >= 0 && ctx->locals[existing].depth == ctx->scope_depth) {
        semantic_error_at(ctx, node, "duplicate declaration of '%s'", name);
        return;
    }
    ensure_capacity((void **)&ctx->locals, ctx->local_count,
        &ctx->local_capacity, sizeof(*ctx->locals));
    if (node->struct_name && find_struct(ctx, node->struct_name) < 0) {
        semantic_error_at(ctx, node, "unknown struct type '%s'", node->struct_name);
        return;
    }

    node->ty = resolve_type(ctx, node);
    if (!node->sym) {
        struct Type *resolved = resolve_type(ctx, node);
        int size = resolved->size > 0 ? resolved->size : 4;

        int align = resolved->align > 0 ? resolved->align : 4;

        if (align < 4) {
            align = 4;          /* never pack scalars tighter than a word */
        }

        node->sym = sym_new(name, SYM_LOCAL, resolved);
        /*
         * The frame grows downward from %rbp, so round the running total up to
         * the type's alignment before claiming the slot. An 8-byte pointer or
         * long must land on an 8-byte boundary.
         */
        ctx->frame_offset += size;
        ctx->frame_offset = (ctx->frame_offset + align - 1) / align * align;
        node->sym->offset = -ctx->frame_offset;
        if (ctx->frame_offset > ctx->frame_max) {
            ctx->frame_max = ctx->frame_offset;
        }
    }

    ctx->locals[ctx->local_count].sym = node->sym;
    ctx->locals[ctx->local_count].name = name;
    ctx->locals[ctx->local_count].type = type;
    ctx->locals[ctx->local_count].pointer_depth = node->pointer_depth;
    ctx->locals[ctx->local_count].array_length = node->array_length;
    ctx->locals[ctx->local_count].struct_name = node->struct_name;
    ctx->locals[ctx->local_count].depth = ctx->scope_depth;
    ctx->local_count++;
}

/*
 * Parameters are already on the stack when the function is entered, above the
 * saved frame pointer and return address, so they get positive offsets and do
 * not consume frame space.
 */
static void add_parameter(struct sema_ctx *ctx, struct ast_node *node, int index)
{
    if (!node->sym) {
        struct Type *resolved = resolve_type(ctx, node);

        node->sym = sym_new(node->value, SYM_PARAM, resolved);
        node->sym->param_index = index;

        if (index < 6) {
            /*
             * Passed in a register: give it a frame slot for the prologue to
             * spill into, so it can be addressed like any other local.
             */
            int size = resolved->size > 0 ? resolved->size : 8;
            int align = resolved->align > 4 ? resolved->align : 4;

            ctx->frame_offset += size;
            ctx->frame_offset = (ctx->frame_offset + align - 1) / align * align;
            node->sym->offset = -ctx->frame_offset;
            if (ctx->frame_offset > ctx->frame_max) {
                ctx->frame_max = ctx->frame_offset;
            }
        } else {
            /* Already on the stack, above the saved %rbp and return address. */
            node->sym->offset = 16 + ((index - 6) * 8);
        }
    }
    add_local(ctx, node, node->data_type);
}

static void enter_scope(struct sema_ctx *ctx)
{
    ctx->scope_depth++;
}


static void leave_scope(struct sema_ctx *ctx)
{
    int i;

    while (ctx->local_count > 0 && ctx->locals[ctx->local_count - 1].depth == ctx->scope_depth) {
        ctx->local_count--;
    }

    /*
     * Rewind the frame to what the still-live locals occupy, so two disjoint
     * blocks reuse the same stack slots instead of each claiming their own.
     * frame_max already recorded the deepest point reached.
     */
    ctx->frame_offset = 0;
    for (i = 0; i < ctx->local_count; i++) {
        struct Symbol *sym = ctx->locals[i].sym;

        if (sym && sym->kind == SYM_LOCAL && -sym->offset > ctx->frame_offset) {
            ctx->frame_offset = -sym->offset;
        }
    }

    ctx->scope_depth--;
}

static void analyze_expression(struct sema_ctx *ctx, struct ast_node *node);
static void analyze_statement(struct sema_ctx *ctx, struct ast_node *node);

static void analyze_expression(struct sema_ctx *ctx, struct ast_node *node)
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
        case AST_INITIALIZER_LIST:
            for (struct ast_node *item = initializer_items(node); item; item = item->right) {
                analyze_expression(ctx, item->left);
            }
            return;
        case AST_IDENTIFIER:
            symbol = find_global(ctx, node->value);
            if (find_local(ctx, node->value) < 0 &&
                (symbol < 0 || ctx->globals[symbol].is_function)) {
                semantic_error_at(ctx, node, "use of undeclared variable '%s'", node->value);
            }
            return;
        case AST_CALL:
            symbol = find_global(ctx, node->value);
            if (find_local(ctx, node->value) >= 0) {
                semantic_error_at(ctx, node, "called object '%s' is not a function", node->value);
            } else if (symbol < 0) {
                semantic_error_at(ctx, node, "call to undeclared function '%s'", node->value);
            } else if (!ctx->globals[symbol].is_function) {
                semantic_error_at(ctx, node, "called object '%s' is not a function", node->value);
            } else {
                actual_count = count_list(node->left, AST_ARG_LIST);
                if (actual_count != ctx->globals[symbol].parameter_count) {
                    semantic_error_at(ctx, node, "function '%s' expects %d argument(s), but %d provided",
                        node->value, ctx->globals[symbol].parameter_count, actual_count);
                }
            }
            for (struct ast_node *arg = node->left; arg; arg = arg->right) {
                analyze_expression(ctx, arg->type == AST_ARG_LIST ? arg->left : arg);
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
            analyze_expression(ctx, node->left);
            return;
        case AST_CONDITIONAL:
            analyze_expression(ctx, node->left);
            analyze_expression(ctx, node->right->left);
            analyze_expression(ctx, node->right->right);
            return;
        default:
            analyze_expression(ctx, node->left);
            analyze_expression(ctx, node->right);
            return;
    }
}

static void analyze_block(struct sema_ctx *ctx, struct ast_node *node, int creates_scope)
{
    if (creates_scope) {
        enter_scope(ctx);
    }
    if (node) {
        analyze_statement(ctx, node->left);
    }
    if (creates_scope) {
        leave_scope(ctx);
    }
}

static void analyze_statement(struct sema_ctx *ctx, struct ast_node *node)
{
    struct ast_node *parts;
    struct ast_node *condition_and_post;

    if (!node) {
        return;
    }

    switch (node->type) {
        case AST_BLOCK:
            analyze_block(ctx, node, 1);
            break;
        case AST_STATEMENT_LIST:
            analyze_statement(ctx, node->left);
            analyze_statement(ctx, node->right);
            break;
        case AST_DECL:
            add_local(ctx, node, node->data_type);
            analyze_expression(ctx, node->left);
            break;
        case AST_EXPR_STMT:
        case AST_RETURN:
            analyze_expression(ctx, node->left);
            break;
        case AST_IF:
            analyze_expression(ctx, node->left);
            analyze_statement(ctx, node->right->left);
            analyze_statement(ctx, node->right->right);
            break;
        case AST_WHILE:
            analyze_expression(ctx, node->left);
            ctx->loop_depth++;
            analyze_statement(ctx, node->right);
            ctx->loop_depth--;
            break;
        case AST_FOR:
            enter_scope(ctx);
            parts = node->left;
            condition_and_post = parts->right;
            if (parts->left && parts->left->type == AST_DECL) {
                analyze_statement(ctx, parts->left);
            } else {
                analyze_expression(ctx, parts->left);
            }
            analyze_expression(ctx, condition_and_post->left);
            analyze_expression(ctx, condition_and_post->right);
            ctx->loop_depth++;
            analyze_statement(ctx, node->right);
            ctx->loop_depth--;
            leave_scope(ctx);
            break;
        case AST_BREAK:
            if (ctx->loop_depth == 0) {
                semantic_error_at(ctx, node, "'break' statement is not inside a loop");
            }
            break;
        case AST_CONTINUE:
            if (ctx->loop_depth == 0) {
                semantic_error_at(ctx, node, "'continue' statement is not inside a loop");
            }
            break;
        default:
            analyze_expression(ctx, node);
            break;
    }
}

static void collect_top_level(struct sema_ctx *ctx, struct ast_node *node)
{
    if (!node) {
        return;
    }
    if (node->type == AST_PROGRAM) {
        collect_top_level(ctx, node->left);
    } else if (node->type == AST_FUNCTION_LIST) {
        collect_top_level(ctx, node->left);
        collect_top_level(ctx, node->right);
    } else if (node->type == AST_FUNCTION) {
        add_global(ctx, node);
    } else if (node->type == AST_STRUCT_DEF) {
        add_struct(ctx, node);
    } else if (node->type == AST_GLOBAL_DECL) {
        add_global(ctx, node);
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
        case AST_INITIALIZER_LIST:
            for (struct ast_node *item = initializer_items(node); item; item = item->right) {
                if (!is_constant_expression(item->left)) {
                    return 0;
                }
            }
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

static void analyze_top_level(struct sema_ctx *ctx, struct ast_node *node)
{
    struct ast_node *param;

    if (!node) {
        return;
    }
    if (node->type == AST_PROGRAM) {
        analyze_top_level(ctx, node->left);
    } else if (node->type == AST_FUNCTION_LIST) {
        analyze_top_level(ctx, node->left);
        analyze_top_level(ctx, node->right);
    } else if (node->type == AST_GLOBAL_DECL) {
        if (!is_constant_expression(node->left)) {
            semantic_error_at(ctx, node, "initializer for global '%s' is not a constant expression", node->value);
        }
    } else if (node->type == AST_STRUCT_DEF) {
        return;
    } else if (node->type == AST_FUNCTION) {
        int param_index = 0;

        ctx->current_function = node->value;
        ctx->local_count = 0;
        ctx->scope_depth = 1;
        ctx->loop_depth = 0;
        ctx->frame_offset = 0;
        ctx->frame_max = 0;

        for (param = node->left; param; param = param->right) {
            if (param->type == AST_PARAM_LIST) {
                add_parameter(ctx, param->left, param_index++);
            }
        }
        analyze_block(ctx, node->right, 0);

        /* The frame the code generator must reserve for this function. */
        if (node->sym) {
            node->sym->frame_size = ctx->frame_max;
        }
        ctx->current_function = NULL;
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
    /*
     * On LP64 a long is wider than an unsigned int and can represent every one
     * of its values, so the unsigned operand converts to long rather than both
     * becoming unsigned. (Where long and int are the same width -- ILP32 --
     * the result would be unsigned long instead.)
     */
    if ((left == TYPE_LONG && right == TYPE_UINT) ||
        (left == TYPE_UINT && right == TYPE_LONG)) return TYPE_LONG;
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

static CType check_expression_type(struct sema_ctx *ctx, struct ast_node **slot);
static CType check_expression_type_inner(struct sema_ctx *ctx, struct ast_node **slot);
static void check_initializer_list_types(struct sema_ctx *ctx, struct ast_node *declaration);

static CType check_binary_type(struct sema_ctx *ctx, struct ast_node *node)
{
    CType left = check_expression_type(ctx, &node->left);
    CType right = check_expression_type(ctx, &node->right);
    CType common;
    int left_pointer_depth = semantic_effective_pointer_depth(node->left);
    int right_pointer_depth = semantic_effective_pointer_depth(node->right);

    if (left == TYPE_INVALID || right == TYPE_INVALID) {
        return node->data_type = TYPE_INVALID;
    }
    if (left_pointer_depth > 0 || right_pointer_depth > 0) {
        if (node->type == AST_ADD &&
            left_pointer_depth > 0 &&
            semantic_is_integer(right, right_pointer_depth, node->right->array_length)) {
            node->pointer_depth = left_pointer_depth;
            node->array_length = 0;
            return node->data_type = left;
        }
        if (node->type == AST_ADD &&
            right_pointer_depth > 0 &&
            semantic_is_integer(left, left_pointer_depth, node->left->array_length)) {
            node->pointer_depth = right_pointer_depth;
            node->array_length = 0;
            return node->data_type = right;
        }
        if (node->type == AST_SUB &&
            left_pointer_depth > 0 &&
            right_pointer_depth > 0) {
            if (left != right || left_pointer_depth != right_pointer_depth) {
                semantic_error_at(ctx, node, "cannot subtract incompatible pointer types");
                return node->data_type = TYPE_INVALID;
            }
            node->pointer_depth = 0;
            node->array_length = 0;
            return node->data_type = TYPE_INT;
        }
        if (node->type == AST_SUB &&
            left_pointer_depth > 0 &&
            semantic_is_integer(right, right_pointer_depth, node->right->array_length)) {
            node->pointer_depth = left_pointer_depth;
            node->array_length = 0;
            return node->data_type = left;
        }
        semantic_error_at(ctx, node, "invalid operands to pointer arithmetic");
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

static CType check_expression_type_inner(struct sema_ctx *ctx, struct ast_node **slot)
{
    struct ast_node *node;
    struct ast_node *argument;
    int local;
    int global;
    int argument_index;
    CType left;
    CType right;
    CType common;
    char left_name[64];
    char right_name[64];

    if (!slot || !*slot) return TYPE_INVALID;
    node = *slot;

    switch (node->type) {
        case AST_INTLIT:
            return node->data_type = TYPE_INT;
        case AST_STRINGLIT:
            node->pointer_depth = 1;
            return node->data_type = TYPE_CHAR;
        case AST_SIZEOF:
            /* Type the operand so its size is known; it is never evaluated. */
            if (node->left) {
                check_expression_type(ctx, &node->left);
            }
            return node->data_type = TYPE_UINT;
        case AST_INITIALIZER_LIST:
            semantic_error_at(ctx, node, "initializer list is not valid in this expression");
            return node->data_type = TYPE_INVALID;
        case AST_IDENTIFIER:
            local = find_local(ctx, node->value);
            global = find_global(ctx, node->value);
            if (local >= 0) {
                node->sym = ctx->locals[local].sym;
                node->data_type = ctx->locals[local].type;
                node->pointer_depth = ctx->locals[local].pointer_depth;
                node->array_length = ctx->locals[local].array_length;
                node->struct_name = ctx->locals[local].struct_name ? strdup(ctx->locals[local].struct_name) : NULL;
                return node->data_type;
            }
            if (global >= 0 && !ctx->globals[global].is_function) {
                node->sym = ctx->globals[global].sym;
                node->pointer_depth = ctx->globals[global].pointer_depth;
                node->array_length = ctx->globals[global].array_length;
                node->struct_name = ctx->globals[global].struct_name ? strdup(ctx->globals[global].struct_name) : NULL;
                return node->data_type = ctx->globals[global].type;
            }
            return node->data_type = TYPE_INVALID;
        case AST_FIELD_ACCESS: {
            int struct_index;
            int field_index;
            check_expression_type(ctx, &node->left);
            if (!node->left->struct_name || node->left->pointer_depth > 0) {
                semantic_error_at(ctx, node, "field access requires a struct value");
                return node->data_type = TYPE_INVALID;
            }
            struct_index = find_struct(ctx, node->left->struct_name);
            field_index = find_struct_field(ctx, struct_index, node->value);
            if (field_index < 0) {
                semantic_error_at(ctx, node, "struct '%s' has no field '%s'",
                    node->left->struct_name, node->value);
                return node->data_type = TYPE_INVALID;
            }
            node->data_type = ctx->structs[struct_index].fields[field_index].type;
            node->pointer_depth = ctx->structs[struct_index].fields[field_index].pointer_depth;
            node->array_length = 0;
            return node->data_type;
        }
        case AST_CALL:
            global = find_global(ctx, node->value);
            argument_index = 0;
            for (argument = node->left; argument; argument = argument->right) {
                check_expression_type(ctx, &argument->left);
                if (global >= 0 && ctx->globals[global].is_function &&
                    argument_index < ctx->globals[global].parameter_count) {
                    if (semantic_effective_pointer_depth(argument->left) !=
                        ctx->globals[global].parameter_pointer_depths[argument_index]) {
                        semantic_format_type(ctx->globals[global].parameter_types[argument_index],
                            ctx->globals[global].parameter_pointer_depths[argument_index], 0,
                            left_name, sizeof(left_name));
                        semantic_format_type(argument->left->data_type,
                            semantic_effective_pointer_depth(argument->left), 0,
                            right_name, sizeof(right_name));
                        semantic_error_at(ctx, argument->left, "cannot pass %s as %s", right_name, left_name);
                    } else if (semantic_effective_pointer_depth(argument->left) == 0) {
                        insert_conversion(&argument->left,
                            ctx->globals[global].parameter_types[argument_index]);
                    }
                }
                argument_index++;
            }
            if (global >= 0 && ctx->globals[global].is_function) {
                node->pointer_depth = ctx->globals[global].pointer_depth;
                return node->data_type = ctx->globals[global].type;
            }
            return node->data_type = TYPE_INVALID;
        case AST_ADDRESS_OF:
            check_expression_type(ctx, &node->left);
            if (node->left->type != AST_IDENTIFIER &&
                node->left->type != AST_DEREFERENCE &&
                node->left->type != AST_FIELD_ACCESS &&
                node->left->type != AST_ARRAY_SUBSCRIPT) {
                semantic_error_at(ctx, node, "operand of '&' must be an lvalue");
                return node->data_type = TYPE_INVALID;
            }
            node->data_type = node->left->data_type;
            node->pointer_depth = node->left->pointer_depth + 1;
            node->array_length = 0;
            return node->data_type;
        case AST_DEREFERENCE:
            check_expression_type(ctx, &node->left);
            if (node->left->pointer_depth <= 0) {
                semantic_error_at(ctx, node, "cannot dereference non-pointer expression");
                return node->data_type = TYPE_INVALID;
            }
            node->data_type = node->left->data_type;
            node->pointer_depth = node->left->pointer_depth - 1;
            node->array_length = 0;
            return node->data_type;
        case AST_ARRAY_SUBSCRIPT:
            check_expression_type(ctx, &node->left);
            check_expression_type(ctx, &node->right);
            if (!semantic_is_integer(node->right->data_type, node->right->pointer_depth,
                    node->right->array_length)) {
                semantic_error_at(ctx, node->right, "array subscript must be an integer");
            }
            if (node->left->array_length <= 0 && node->left->pointer_depth <= 0) {
                semantic_error_at(ctx, node, "subscripted expression is not an array or pointer");
                return node->data_type = TYPE_INVALID;
            }
            node->data_type = node->left->data_type;
            node->pointer_depth = node->left->array_length > 0 ?
                node->left->pointer_depth : node->left->pointer_depth - 1;
            node->array_length = 0;
            /*
             * Carry the struct tag through the subscript, so an element of a
             * struct array is still a struct and its fields stay accessible.
             */
            node->struct_name = node->left->struct_name ?
                strdup(node->left->struct_name) : NULL;
            return node->data_type;
        case AST_CAST:
            check_expression_type(ctx, &node->left);
            if (node->data_type == TYPE_INVALID)
                node->data_type = semantic_type_from_name(node->value);
            return node->data_type;
        case AST_NEGATION:
        case AST_BITWISE_COMPLEMENT:
            left = integer_promotion(check_expression_type(ctx, &node->left));
            insert_conversion(&node->left, left);
            return node->data_type = left;
        case AST_LOGICAL_NEGATION:
            check_expression_type(ctx, &node->left);
            return node->data_type = TYPE_INT;
        case AST_PRE_INCREMENT:
        case AST_PRE_DECREMENT:
        case AST_POST_INCREMENT:
        case AST_POST_DECREMENT:
            node->data_type = check_expression_type(ctx, &node->left);
            node->pointer_depth = semantic_effective_pointer_depth(node->left);
            return node->data_type;
        case AST_ASSIGN:
            left = check_expression_type(ctx, &node->left);
            check_expression_type(ctx, &node->right);
            if (node->left->array_length > 0) {
                semantic_error_at(ctx, node->left, "cannot assign to array '%s'", node->left->value);
            } else if (!semantic_type_matches(left, semantic_effective_pointer_depth(node->left),
                    node->right->data_type, semantic_effective_pointer_depth(node->right))) {
                if (semantic_effective_pointer_depth(node->left) > 0 ||
                    semantic_effective_pointer_depth(node->right) > 0) {
                    semantic_format_type(left, node->left->pointer_depth, 0,
                        left_name, sizeof(left_name));
                    semantic_format_type(node->right->data_type,
                        semantic_effective_pointer_depth(node->right), 0,
                        right_name, sizeof(right_name));
                    semantic_error_at(ctx, node, "cannot assign %s to %s", right_name, left_name);
                } else {
                    insert_conversion(&node->right, left);
                }
            }
            node->pointer_depth = semantic_effective_pointer_depth(node->left);
            return node->data_type = left;
        case AST_CONDITIONAL:
            check_expression_type(ctx, &node->left);
            left = check_expression_type(ctx, &node->right->left);
            right = check_expression_type(ctx, &node->right->right);
            common = usual_arithmetic_type(left, right);
            insert_conversion(&node->right->left, common);
            insert_conversion(&node->right->right, common);
            return node->data_type = common;
        case AST_COMMA:
            check_expression_type(ctx, &node->left);
            return node->data_type = check_expression_type(ctx, &node->right);
        default:
            return check_binary_type(ctx, node);
    }
}

/*
 * Every expression node carries a resolved type, derived from the fields the
 * checker above computes. The code generator reads these for element sizes and
 * struct offsets instead of assuming 4 bytes.
 */
static CType check_expression_type(struct sema_ctx *ctx, struct ast_node **slot)
{
    CType result = check_expression_type_inner(ctx, slot);

    if (slot && *slot) {
        (*slot)->ty = resolve_type(ctx, *slot);
    }
    return result;
}

static void check_initializer_list_types(struct sema_ctx *ctx, struct ast_node *declaration)
{
    int index = 0;
    struct ast_node *item;

    if (!declaration->left) {
        return;
    }
    if (declaration->left->type != AST_INITIALIZER_LIST) {
        semantic_error_at(ctx, declaration->left, "array initializer must be brace-enclosed");
        return;
    }

    for (item = initializer_items(declaration->left); item; item = item->right) {
        if (index >= declaration->array_length) {
            semantic_error_at(ctx, item->left ? item->left : item,
                "too many initializers for array '%s'", declaration->value);
            return;
        }
        check_expression_type(ctx, &item->left);
        if (item->left) {
            insert_conversion(&item->left, declaration->data_type);
        }
        index++;
    }
}

static void check_statement_types(struct sema_ctx *ctx, struct ast_node *node)
{
    struct ast_node *parts;
    struct ast_node *condition_and_post;

    if (!node) return;
    switch (node->type) {
        case AST_BLOCK:
            enter_scope(ctx);
            check_statement_types(ctx, node->left);
            leave_scope(ctx);
            break;
        case AST_STATEMENT_LIST:
            check_statement_types(ctx, node->left);
            check_statement_types(ctx, node->right);
            break;
        case AST_DECL:
            add_local(ctx, node, node->data_type);
            if (node->left) {
                if (node->array_length > 0) {
                    check_initializer_list_types(ctx, node);
                } else if (node->left->type == AST_INITIALIZER_LIST) {
                    semantic_error_at(ctx, node->left, "initializer list is only valid for arrays");
                } else {
                    check_expression_type(ctx, &node->left);
                    if (node->pointer_depth > 0 || semantic_effective_pointer_depth(node->left) > 0) {
                    if (!semantic_type_matches(node->data_type, node->pointer_depth,
                            node->left->data_type, semantic_effective_pointer_depth(node->left))) {
                        char left_name[64];
                        char right_name[64];
                        semantic_format_type(node->data_type, node->pointer_depth, 0,
                            left_name, sizeof(left_name));
                        semantic_format_type(node->left->data_type,
                            semantic_effective_pointer_depth(node->left), 0,
                            right_name, sizeof(right_name));
                        semantic_error_at(ctx, node, "cannot initialize %s with %s", left_name, right_name);
                    }
                    } else {
                        insert_conversion(&node->left, node->data_type);
                    }
                }
            }
            break;
        case AST_EXPR_STMT:
            check_expression_type(ctx, &node->left);
            break;
        case AST_RETURN:
            check_expression_type(ctx, &node->left);
            if (ctx->current_return_pointer_depth > 0 || semantic_effective_pointer_depth(node->left) > 0) {
                if (!semantic_type_matches(ctx->current_return_type, ctx->current_return_pointer_depth,
                        node->left->data_type, semantic_effective_pointer_depth(node->left))) {
                    char left_name[64];
                    char right_name[64];
                    semantic_format_type(ctx->current_return_type, ctx->current_return_pointer_depth, 0,
                        left_name, sizeof(left_name));
                    semantic_format_type(node->left->data_type,
                        semantic_effective_pointer_depth(node->left), 0,
                        right_name, sizeof(right_name));
                    semantic_error_at(ctx, node, "cannot return %s from function returning %s",
                        right_name, left_name);
                }
            } else {
                insert_conversion(&node->left, ctx->current_return_type);
            }
            break;
        case AST_IF:
            check_expression_type(ctx, &node->left);
            check_statement_types(ctx, node->right->left);
            check_statement_types(ctx, node->right->right);
            break;
        case AST_WHILE:
            check_expression_type(ctx, &node->left);
            check_statement_types(ctx, node->right);
            break;
        case AST_FOR:
            enter_scope(ctx);
            parts = node->left;
            condition_and_post = parts->right;
            if (parts->left && parts->left->type == AST_DECL)
                check_statement_types(ctx, parts->left);
            else
                check_expression_type(ctx, &parts->left);
            check_expression_type(ctx, &condition_and_post->left);
            check_expression_type(ctx, &condition_and_post->right);
            check_statement_types(ctx, node->right);
            leave_scope(ctx);
            break;
        default:
            break;
    }
}

static void check_top_level_types(struct sema_ctx *ctx, struct ast_node *node)
{
    struct ast_node *param;

    if (!node) return;
    if (node->type == AST_PROGRAM) {
        check_top_level_types(ctx, node->left);
    } else if (node->type == AST_FUNCTION_LIST) {
        check_top_level_types(ctx, node->left);
        check_top_level_types(ctx, node->right);
    } else if (node->type == AST_GLOBAL_DECL) {
        if (node->left) {
            if (node->array_length > 0) {
                check_initializer_list_types(ctx, node);
            } else if (node->left->type == AST_INITIALIZER_LIST) {
                semantic_error_at(ctx, node->left, "initializer list is only valid for arrays");
            } else {
                check_expression_type(ctx, &node->left);
                insert_conversion(&node->left, node->data_type);
            }
        }
    } else if (node->type == AST_STRUCT_DEF) {
        return;
    } else if (node->type == AST_FUNCTION) {
        ctx->current_return_type = node->data_type;
        ctx->current_return_pointer_depth = node->pointer_depth;
        ctx->local_count = 0;
        ctx->scope_depth = 1;
        ctx->frame_offset = 0;
        {
            int param_index = 0;
            for (param = node->left; param; param = param->right)
                add_parameter(ctx, param->left, param_index++);
        }
        if (node->right) check_statement_types(ctx, node->right->left);
    }
}

int semantic_analyze(struct ast_node *ast, const char *source_path)
{
    struct sema_ctx ctx_storage;
    struct sema_ctx *ctx = &ctx_storage;
    int ok;

    memset(ctx, 0, sizeof(*ctx));
    ctx->source_path = source_path;

    collect_top_level(ctx, ast);
    analyze_top_level(ctx, ast);
    if (ctx->error_count == 0) check_top_level_types(ctx, ast);

    ok = ctx->error_count == 0;

    /*
     * The tables hold borrowed pointers into the AST, so only the arrays
     * themselves are owned here.
     */
    free(ctx->globals);
    free(ctx->locals);
    free(ctx->structs);

    return ok;
}
