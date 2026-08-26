/*
 * Declaration forms added in Phase 8: prototypes, void, storage classes and
 * qualifiers, several declarators per declaration, enum, and typedef.
 */
typedef int Integer;
typedef unsigned char Byte;
typedef int *IntPtr;

enum Colour { RED, GREEN, BLUE };
enum Status { OK = 10, FAILED = 20, UNKNOWN };

/* Declared before it is defined, which is what a prototype is for. */
static int twice(Integer n);

const int scale = 3;

int twice(Integer n)
{
    return n * 2;
}

int main()
{
    int a = 1, b = 2, c;           /* several declarators, some initialised */
    Integer counted = 5;
    Byte small = 200;
    IntPtr pointer;
    enum Colour colour = GREEN;

    c = a + b;
    pointer = &c;

    return twice(counted)          /* 10 */
        + small                    /* 200 */
        + *pointer                 /* 3 */
        + colour * 100             /* 100 */
        + FAILED + UNKNOWN         /* 41 */
        + BLUE                     /* 2 */
        + scale;                   /* 3 */
}
