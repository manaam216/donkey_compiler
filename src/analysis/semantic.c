#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "decl.h"
#include "type.h"
#include "symbol.h"
#include "diag.h"
#include "support/mem.h"
#include "sema_internal.h"

const char *semantic_type_name(CType type)
{
    switch (type) {
        case TYPE_VOID: return "void";
        case TYPE_FLOAT: return "float";
        case TYPE_DOUBLE: return "double";
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

void semantic_format_type(CType type, int pointer_depth, int array_length,
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

int semantic_type_matches(CType left_type, int left_pointer_depth,
    CType right_type, int right_pointer_depth)
{
    return left_type == right_type && left_pointer_depth == right_pointer_depth;
}

int semantic_is_integer(CType type, int pointer_depth, int array_length)
{
    return type != TYPE_INVALID && pointer_depth == 0 && array_length == 0;
}

int semantic_effective_pointer_depth(struct ast_node *node)
{
    if (!node) {
        return 0;
    }
    return node->pointer_depth + (node->array_length > 0 ? 1 : 0);
}

int sema_find_struct(struct sema_ctx *ctx, const char *name)
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

int sema_find_struct_field(struct sema_ctx *ctx, int struct_index, const char *name)
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

CType semantic_type_from_name(const char *name)
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

void semantic_error_at(struct sema_ctx *ctx, struct ast_node *node, const char *format, ...)
{
    SourceLocation location;
    va_list args;
    char message[512];

    location.line = 0;
    location.column = 0;
    if (node) {
        location = node->location;
    }

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    diag_set_function(ctx->current_function);
    diag_at(DIAG_ERROR, location, "%s", message);
    ctx->error_count++;
}

int sema_count_list(struct ast_node *node, ASTNodeType list_type)
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

int semantic_analyze(struct ast_node *ast, const char *source_path)
{
    struct sema_ctx ctx_storage;
    struct sema_ctx *ctx = &ctx_storage;
    int ok;

    memset(ctx, 0, sizeof(*ctx));
    ctx->source_path = source_path;

    sema_collect_top_level(ctx, ast);
    sema_analyze_top_level(ctx, ast);
    if (ctx->error_count == 0) sema_check_top_level_types(ctx, ast);

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
