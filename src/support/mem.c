#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "support/mem.h"

static void out_of_memory(const char *what)
{
    fprintf(stderr, "out of memory allocating %s\n", what ? what : "memory");
    exit(EXIT_FAILURE);
}

void *xmalloc(size_t size, const char *what)
{
    void *block = malloc(size);

    if (!block) {
        out_of_memory(what);
    }
    return block;
}

void *xcalloc(size_t count, size_t size, const char *what)
{
    void *block = calloc(count, size);

    if (!block) {
        out_of_memory(what);
    }
    return block;
}

void *xrealloc(void *pointer, size_t size, const char *what)
{
    void *block = realloc(pointer, size);

    if (!block) {
        out_of_memory(what);
    }
    return block;
}

char *xstrdup(const char *text, const char *what)
{
    char *copy;
    size_t length;

    if (!text) {
        return NULL;
    }
    length = strlen(text) + 1;
    copy = xmalloc(length, what);
    memcpy(copy, text, length);
    return copy;
}

void grow_array(void **items, int count, int *capacity, size_t item_size,
    int initial_capacity, const char *what)
{
    int next;

    if (count < *capacity) {
        return;
    }

    next = *capacity ? *capacity * 2 : initial_capacity;
    *items = xrealloc(*items, (size_t)next * item_size, what);
    *capacity = next;
}
