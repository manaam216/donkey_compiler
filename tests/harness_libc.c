/*
 * Harness for programs that call the C library.
 *
 * Same job as tests/harness.c -- run the compiled program and print its result
 * -- but it declares printf itself instead of including <stdio.h>, so it can be
 * cross-compiled for a target whose headers are not installed locally. The
 * program under test provides donkey_main and may print as much as it likes;
 * the trailing line is its return value.
 */

extern int printf(const char *format, ...);
extern int donkey_main(void);

int main(void)
{
    printf("%d\n", donkey_main());
    return 0;
}
