/*
 * Test harness for Donkey-compiled examples.
 *
 * Donkey cannot call printf: it has no preprocessor, and semantic analysis
 * rejects calls to undeclared functions. So examples cannot report their own
 * results. Instead the build renames the example's `main` symbol to
 * `donkey_main`, and this harness -- compiled by the host C compiler -- calls
 * it and prints the full 32-bit result.
 *
 * This exists because process exit codes are truncated to 8 unsigned bits.
 * Asserting on them silently accepts wrong answers: a function returning
 * 100000 exits 160, and one returning -42 exits 214. Printing the value
 * instead makes the whole int range observable.
 */
#include <stdio.h>

extern int donkey_main(void);

int main(void)
{
    printf("%d\n", donkey_main());
    return 0;
}
