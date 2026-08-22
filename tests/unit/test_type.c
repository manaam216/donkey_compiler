/*
 * Unit tests for the type layer. Everything downstream -- stack frames, struct
 * offsets, pointer scaling, sizeof -- reads its numbers from here, so these
 * sizes are checked directly rather than only through generated assembly.
 */
#include <stdio.h>
#include <string.h>
#include "type.h"

static int failures;

static void check_int(const char *label, int actual, int expected)
{
    if (actual != expected) {
        printf("FAIL %s: expected %d, got %d\n", label, expected, actual);
        failures++;
    }
}

static void check_str(const char *label, const char *actual, const char *expected)
{
    if (strcmp(actual, expected) != 0) {
        printf("FAIL %s: expected '%s', got '%s'\n", label, expected, actual);
        failures++;
    }
}

static void test_basic_sizes(void)
{
    check_int("sizeof(char)", ty_char->size, 1);
    check_int("alignof(char)", ty_char->align, 1);
    check_int("sizeof(short)", ty_short->size, 2);
    check_int("alignof(short)", ty_short->align, 2);
    check_int("sizeof(int)", ty_int->size, 4);
    check_int("sizeof(long)", ty_long->size, 4);
    check_int("unsigned char is unsigned", ty_uchar->is_unsigned, 1);
    check_int("char is signed", ty_char->is_unsigned, 0);
}

static void test_arrays(void)
{
    struct Type *chars = ty_array_of(ty_char, 10);
    struct Type *ints = ty_array_of(ty_int, 10);
    struct Type *shorts = ty_array_of(ty_short, 3);

    /* The bug this whole phase exists to fix: char[10] used to occupy 40. */
    check_int("sizeof(char[10])", chars->size, 10);
    check_int("alignof(char[10])", chars->align, 1);
    check_int("sizeof(int[10])", ints->size, 40);
    check_int("sizeof(short[3])", shorts->size, 6);

    check_int("char[] element size", ty_element_size(chars), 1);
    check_int("int[] element size", ty_element_size(ints), 4);
}

static void test_pointers(void)
{
    struct Type *pchar = ty_pointer_to(ty_char);
    struct Type *ppchar = ty_pointer_to(pchar);

    check_int("sizeof(char*)", pchar->size, 4);
    /* Scaling char* arithmetic by 1, not 4. */
    check_int("char* element size", ty_element_size(pchar), 1);
    check_int("char** element size", ty_element_size(ppchar), 4);
}

static void test_decay(void)
{
    struct Type *chars = ty_array_of(ty_char, 10);
    struct Type *decayed = ty_decay(chars);

    check_int("char[10] decays to pointer", decayed->kind == TY_PTR, 1);
    check_int("decayed element size", ty_element_size(decayed), 1);
    check_int("int does not decay", ty_decay(ty_int) == ty_int, 1);
}

static void test_struct_layout(void)
{
    /* struct { char a; int b; char c; } -- padding after a, tail padding after c. */
    struct Type *s = ty_struct("mixed");
    struct Member *a, *b, *c;

    ty_add_member(s, "a", ty_char);
    ty_add_member(s, "b", ty_int);
    ty_add_member(s, "c", ty_char);
    ty_layout_struct(s);

    a = ty_find_member(s, "a");
    b = ty_find_member(s, "b");
    c = ty_find_member(s, "c");

    check_int("member a offset", a->offset, 0);
    check_int("member b offset", b->offset, 4);   /* was 4 by luck, now by rule */
    check_int("member c offset", c->offset, 8);
    check_int("struct align", s->align, 4);
    check_int("struct size with tail padding", s->size, 12);
}

static void test_packed_struct_layout(void)
{
    /* All-char struct needs no padding at all; the old model gave it 12. */
    struct Type *s = ty_struct("chars");

    ty_add_member(s, "a", ty_char);
    ty_add_member(s, "b", ty_char);
    ty_add_member(s, "c", ty_char);
    ty_layout_struct(s);

    check_int("char struct size", s->size, 3);
    check_int("char struct align", s->align, 1);
    check_int("third member offset", ty_find_member(s, "c")->offset, 2);
}

static void test_struct_arrays(void)
{
    struct Type *s = ty_struct("pair");
    struct Type *arr;

    ty_add_member(s, "x", ty_int);
    ty_add_member(s, "y", ty_int);
    ty_layout_struct(s);

    arr = ty_array_of(s, 4);
    check_int("sizeof(struct pair)", s->size, 8);
    check_int("sizeof(struct pair[4])", arr->size, 32);
    check_int("struct array element size", ty_element_size(arr), 8);
}

static void test_formatting(void)
{
    char buffer[128];

    ty_format(ty_pointer_to(ty_int), buffer, sizeof(buffer));
    check_str("format int*", buffer, "int*");

    ty_format(ty_pointer_to(ty_pointer_to(ty_char)), buffer, sizeof(buffer));
    check_str("format char**", buffer, "char**");

    ty_format(ty_array_of(ty_int, 4), buffer, sizeof(buffer));
    check_str("format int[4]", buffer, "int[4]");

    ty_format(ty_uint, buffer, sizeof(buffer));
    check_str("format uint", buffer, "uint");
}

int main(void)
{
    test_basic_sizes();
    test_arrays();
    test_pointers();
    test_decay();
    test_struct_layout();
    test_packed_struct_layout();
    test_struct_arrays();
    test_formatting();

    ty_cleanup();

    if (failures) {
        printf("%d type check(s) failed.\n", failures);
        return 1;
    }
    printf("All type checks passed.\n");
    return 0;
}
