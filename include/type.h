#ifndef DONKEY_TYPE_H
#define DONKEY_TYPE_H

#include <stddef.h>

/*
 * The compiler's type representation.
 *
 * This replaces the three loose fields (data_type, pointer_depth,
 * array_length) that used to be smeared across every AST node. Those could not
 * express a pointee type, so every size decision in the code generator was a
 * hardcoded 4 -- which meant `char a[10]` indexed by 4 and occupied 40 bytes.
 * A Type knows its own size and alignment, so layout and scaling are computed
 * rather than assumed.
 *
 * Sizes below are for the x86-64 System V target (LP64): long and pointers are
 * both 8 bytes. Retargeting changes these constants and nothing else.
 */

typedef enum {
    TY_VOID,
    TY_CHAR,
    TY_SHORT,
    TY_INT,
    TY_LONG,
    TY_PTR,
    TY_ARRAY,
    TY_STRUCT
} TypeKind;

struct Member {
    char *name;
    struct Type *ty;
    int offset;
    struct Member *next;
};

struct Type {
    TypeKind kind;
    int size;               /* sizeof this type, in bytes */
    int align;              /* required alignment, in bytes */
    int is_unsigned;

    struct Type *base;      /* TY_PTR: pointee; TY_ARRAY: element */
    int array_length;       /* TY_ARRAY */

    char *name;             /* TY_STRUCT: tag */
    struct Member *members; /* TY_STRUCT */
    int is_complete;        /* TY_STRUCT: fields have been laid out */
};

/* Basic types. These are shared singletons; never free or mutate them. */
extern struct Type *ty_void;
extern struct Type *ty_char;
extern struct Type *ty_uchar;
extern struct Type *ty_short;
extern struct Type *ty_ushort;
extern struct Type *ty_int;
extern struct Type *ty_uint;
extern struct Type *ty_long;
extern struct Type *ty_ulong;

struct Type *ty_pointer_to(struct Type *base);
struct Type *ty_array_of(struct Type *base, int length);
struct Type *ty_struct(const char *name);

/*
 * Assign offsets to a struct's members, inserting padding so each lands on its
 * own alignment boundary, and set the struct's overall size and alignment.
 */
void ty_layout_struct(struct Type *type);
void ty_add_member(struct Type *type, const char *name, struct Type *member_type);
struct Member *ty_find_member(struct Type *type, const char *name);

int ty_is_integer(struct Type *type);
int ty_is_pointer_like(struct Type *type);   /* pointer or array */

/*
 * The type a value of this type decays to when used in an expression: arrays
 * become pointers to their element type, everything else is unchanged.
 */
struct Type *ty_decay(struct Type *type);

/* Element size used to scale pointer arithmetic and subscripting. */
int ty_element_size(struct Type *type);

const char *ty_name(struct Type *type);
void ty_format(struct Type *type, char *buffer, size_t size);

/* Look up a basic type by the spelling the parser and semantic pass use. */
struct Type *ty_from_name(const char *name);

/* Release every type allocated by the constructors above. */
void ty_cleanup(void);

#endif
