/*
 * Brace initialisation: for arrays and structs, positional and designated.
 *
 * A designator says where its value goes -- `[2] =` for an array element,
 * `.field =` for a struct member -- and the values after it continue from
 * there. Anything no initialiser covers is zero.
 */
struct Point {
    int x;
    int y;
    int z;
};

int main()
{
    int positional[3] = {1, 2, 3};
    int sparse[6] = {[1] = 10, [4] = 40};
    int leading[4] = {7, [2] = 9};          /* then 9 lands at index 2 */
    struct Point a = {1, 2, 3};
    struct Point b = {.z = 30, .x = 10};    /* out of order; y stays zero */
    struct Point partial = {5};             /* y and z are zero */
    int total = 0;
    int i;

    for (i = 0; i < 3; i = i + 1) {
        total = total + positional[i];      /* 6 */
    }
    for (i = 0; i < 6; i = i + 1) {
        total = total + sparse[i];          /* 50, the gaps being zero */
    }
    for (i = 0; i < 4; i = i + 1) {
        total = total + leading[i];         /* 16 */
    }

    total = total + a.x + a.y + a.z;        /* 6 */
    total = total + b.x + b.y + b.z;        /* 40 */
    total = total + partial.x + partial.y + partial.z;  /* 5 */

    return total;
}
