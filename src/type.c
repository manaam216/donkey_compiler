#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "type.h"

/*
 * Basic types are singletons: there is exactly one `int` type object, so
 * comparisons can be done by pointer where convenient.
 */
static struct Type basic_void   = { TY_VOID,  1, 1, 0, NULL, 0, NULL, NULL, 1 };
static struct Type basic_char   = { TY_CHAR,  1, 1, 0, NULL, 0, NULL, NULL, 1 };
static struct Type basic_uchar  = { TY_CHAR,  1, 1, 1, NULL, 0, NULL, NULL, 1 };
static struct Type basic_short  = { TY_SHORT, 2, 2, 0, NULL, 0, NULL, NULL, 1 };
static struct Type basic_ushort = { TY_SHORT, 2, 2, 1, NULL, 0, NULL, NULL, 1 };
static struct Type basic_int    = { TY_INT,   4, 4, 0, NULL, 0, NULL, NULL, 1 };
static struct Type basic_uint   = { TY_INT,   4, 4, 1, NULL, 0, NULL, NULL, 1 };
static struct Type basic_long   = { TY_LONG,  8, 8, 0, NULL, 0, NULL, NULL, 1 };
static struct Type basic_ulong  = { TY_LONG,  8, 8, 1, NULL, 0, NULL, NULL, 1 };

struct Type *ty_void   = &basic_void;
struct Type *ty_char   = &basic_char;
struct Type *ty_uchar  = &basic_uchar;
struct Type *ty_short  = &basic_short;
struct Type *ty_ushort = &basic_ushort;
struct Type *ty_int    = &basic_int;
struct Type *ty_uint   = &basic_uint;
struct Type *ty_long   = &basic_long;
struct Type *ty_ulong  = &basic_ulong;

/* Pointer size and alignment for the x86-64 target. */
#define POINTER_SIZE 8

/*
 * Derived types are tracked so ty_cleanup can release them. The compiler
 * builds a modest number of them and holds them for the whole run, so a plain
 * list is enough; there is no need to intern or reference-count.
 */
struct type_entry {
    struct Type *type;
    struct type_entry *next;
};

static struct type_entry *allocated_types;

static struct Type *alloc_type(TypeKind kind)
{
    struct Type *type = calloc(1, sizeof(*type));
    struct type_entry *entry = malloc(sizeof(*entry));

    if (!type || !entry) {
        fprintf(stderr, "Out of memory while allocating a type\n");
        exit(EXIT_FAILURE);
    }

    type->kind = kind;
    type->is_complete = 1;

    entry->type = type;
    entry->next = allocated_types;
    allocated_types = entry;

    return type;
}

struct Type *ty_pointer_to(struct Type *base)
{
    struct Type *type = alloc_type(TY_PTR);

    type->size = POINTER_SIZE;
    type->align = POINTER_SIZE;
    type->base = base;
    return type;
}

struct Type *ty_array_of(struct Type *base, int length)
{
    struct Type *type = alloc_type(TY_ARRAY);

    type->base = base;
    type->array_length = length;
    type->align = base ? base->align : 1;
    type->size = base ? base->size * length : 0;
    return type;
}

struct Type *ty_struct(const char *name)
{
    struct Type *type = alloc_type(TY_STRUCT);

    type->name = name ? strdup(name) : NULL;
    type->align = 1;
    type->size = 0;
    type->is_complete = 0;
    return type;
}

struct Type *ty_func(struct Type *return_type)
{
    struct Type *type = alloc_type(TY_FUNC);

    /*
     * A function has no size of its own; what gets stored is a pointer to it.
     * Giving it a size of one keeps arithmetic on such a pointer harmless.
     */
    type->size = 1;
    type->align = 1;
    type->base = return_type;
    return type;
}

void ty_add_member(struct Type *type, const char *name, struct Type *member_type)
{
    struct Member *member;
    struct Member **tail;

    if (!type || type->kind != TY_STRUCT) {
        return;
    }

    member = calloc(1, sizeof(*member));
    if (!member) {
        fprintf(stderr, "Out of memory while allocating a struct member\n");
        exit(EXIT_FAILURE);
    }

    member->name = name ? strdup(name) : NULL;
    member->ty = member_type;

    for (tail = &type->members; *tail; tail = &(*tail)->next) {
        /* append, so members keep declaration order */
    }
    *tail = member;
}

static int align_to(int offset, int align)
{
    if (align <= 1) {
        return offset;
    }
    return (offset + align - 1) / align * align;
}

void ty_layout_struct(struct Type *type)
{
    struct Member *member;
    int offset = 0;
    int max_align = 1;

    if (!type || type->kind != TY_STRUCT) {
        return;
    }

    for (member = type->members; member; member = member->next) {
        int member_align = member->ty ? member->ty->align : 1;
        int member_size = member->ty ? member->ty->size : 0;

        offset = align_to(offset, member_align);
        member->offset = offset;
        offset += member_size;

        if (member_align > max_align) {
            max_align = member_align;
        }
    }

    /* Tail padding, so an array of this struct keeps every element aligned. */
    type->align = max_align;
    type->size = align_to(offset, max_align);
    type->is_complete = 1;
}

struct Member *ty_find_member(struct Type *type, const char *name)
{
    struct Member *member;

    if (!type || type->kind != TY_STRUCT || !name) {
        return NULL;
    }

    for (member = type->members; member; member = member->next) {
        if (member->name && strcmp(member->name, name) == 0) {
            return member;
        }
    }
    return NULL;
}

int ty_is_integer(struct Type *type)
{
    if (!type) {
        return 0;
    }
    return type->kind == TY_CHAR || type->kind == TY_SHORT ||
           type->kind == TY_INT || type->kind == TY_LONG;
}

int ty_is_pointer_like(struct Type *type)
{
    return type && (type->kind == TY_PTR || type->kind == TY_ARRAY);
}

struct Type *ty_decay(struct Type *type)
{
    if (type && type->kind == TY_ARRAY) {
        return ty_pointer_to(type->base);
    }
    return type;
}

int ty_element_size(struct Type *type)
{
    if (!type || !type->base) {
        return 1;
    }
    return type->base->size > 0 ? type->base->size : 1;
}

const char *ty_name(struct Type *type)
{
    if (!type) {
        return "int";
    }

    switch (type->kind) {
        case TY_VOID:   return "void";
        case TY_CHAR:   return type->is_unsigned ? "uchar" : "char";
        case TY_SHORT:  return type->is_unsigned ? "ushort" : "short";
        case TY_INT:    return type->is_unsigned ? "uint" : "int";
        case TY_LONG:   return type->is_unsigned ? "ulong" : "long";
        case TY_STRUCT: return type->name ? type->name : "struct";
        case TY_FUNC:   return "function";
        default:        return "int";
    }
}

void ty_format(struct Type *type, char *buffer, size_t size)
{
    if (!buffer || size == 0) {
        return;
    }

    if (!type) {
        snprintf(buffer, size, "int");
        return;
    }

    switch (type->kind) {
        case TY_PTR: {
            char inner[96];
            ty_format(type->base, inner, sizeof(inner));
            snprintf(buffer, size, "%s*", inner);
            break;
        }
        case TY_ARRAY: {
            char inner[96];
            ty_format(type->base, inner, sizeof(inner));
            snprintf(buffer, size, "%s[%d]", inner, type->array_length);
            break;
        }
        case TY_STRUCT:
            snprintf(buffer, size, "struct %s", type->name ? type->name : "?");
            break;
        case TY_FUNC: {
            char inner[96];

            ty_format(type->base, inner, sizeof(inner));
            snprintf(buffer, size, "%s()", inner);
            break;
        }
        default:
            snprintf(buffer, size, "%s", ty_name(type));
            break;
    }
}

struct Type *ty_from_name(const char *name)
{
    if (!name) return ty_int;
    if (strcmp(name, "void") == 0)   return ty_void;
    if (strcmp(name, "char") == 0)   return ty_char;
    if (strcmp(name, "uchar") == 0)  return ty_uchar;
    if (strcmp(name, "short") == 0)  return ty_short;
    if (strcmp(name, "ushort") == 0) return ty_ushort;
    if (strcmp(name, "uint") == 0)   return ty_uint;
    if (strcmp(name, "long") == 0)   return ty_long;
    if (strcmp(name, "ulong") == 0)  return ty_ulong;
    return ty_int;
}

void ty_cleanup(void)
{
    struct type_entry *entry = allocated_types;

    while (entry) {
        struct type_entry *next = entry->next;
        struct Member *member = entry->type->members;

        while (member) {
            struct Member *member_next = member->next;
            free(member->name);
            free(member);
            member = member_next;
        }

        free(entry->type->name);
        free(entry->type);
        free(entry);
        entry = next;
    }

    allocated_types = NULL;
}
