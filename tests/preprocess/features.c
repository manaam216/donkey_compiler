/*
 * Every preprocessor feature Donkey implements, in one file. Its output is
 * compared token for token against the system cpp, and the program is also
 * compiled and run.
 *
 * config.h arrives twice -- directly and through extra.h -- so #pragma once
 * is exercised too.
 */
#include "include/extra.h"
#include "include/config.h"

#define ONE 1
#define SUM (1 + 2)
#define TWICE(x) ((x) + (x))
#define JOIN(a, b) a##b
#define APPLY(f, x) f(x)
#define SELF SELF

#if defined(ONE) && SUM > 2
#define PICKED 100
#elif SUM == 2
#define PICKED 200
#else
#define PICKED 300
#endif

#ifndef ABSENT
#define PRESENT 7
#endif

/* SUM deliberately does not use ONE: a macro expands where it is used, so
   undefining ONE first would leave an undefined name behind. */
#undef ONE
#ifdef ONE
#define AFTER_UNDEF 1
#else
#define AFTER_UNDEF 2
#endif

int JOIN(my, func)(int n)
{
    return n + CONFIG_TOTAL;
}

int main()
{
    int a = SUM;
    int b = TWICE(3);
    int c = APPLY(TWICE, 4);
    int d = PICKED + PRESENT + AFTER_UNDEF;
    int e = myfunc(1);

    return a + b + c + d + e;
}
