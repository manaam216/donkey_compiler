#ifndef DONKEY_MEM_H
#define DONKEY_MEM_H

#include <stddef.h>

/*
 * Allocation that cannot fail, and the growable array built on it.
 *
 * A compiler that has run out of memory has nothing useful left to do: it
 * cannot report the problem it was in the middle of finding, and every caller
 * would only propagate the failure. So allocation aborts rather than returning
 * NULL, and callers are spared a check they could not act on. `what` names the
 * thing being allocated so the message says which table ran out.
 */
void *xmalloc(size_t size, const char *what);
void *xcalloc(size_t count, size_t size, const char *what);
void *xrealloc(void *pointer, size_t size, const char *what);
char *xstrdup(const char *text, const char *what);

/*
 * Make room for at least one more element in a count/capacity array, doubling
 * when it is full. Three passes had grown their own copy of this, differing
 * only in the starting capacity and the wording of the failure.
 *
 * *items and *capacity are updated in place; item_size is sizeof one element.
 */
void grow_array(void **items, int count, int *capacity, size_t item_size,
    int initial_capacity, const char *what);

#endif
