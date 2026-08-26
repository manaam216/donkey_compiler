/*
 * Struct pointers with ->, whole-struct assignment, multi-dimensional arrays,
 * and increment applied to something other than a plain variable.
 */
struct Point {
    int x;
    int y;
};

int main()
{
    struct Point a;
    struct Point b;
    struct Point *p;
    int grid[2][3];
    int counters[3];
    int i, j, total = 0;

    a.x = 3;
    a.y = 4;

    b = a;                      /* whole-struct assignment */
    p = &b;
    p->x = p->x + 10;           /* field access through a pointer */

    for (i = 0; i < 2; i = i + 1) {
        for (j = 0; j < 3; j = j + 1) {
            grid[i][j] = i * 10 + j;
        }
    }

    counters[0] = 0;
    counters[1] = 0;
    counters[2] = 0;
    counters[1]++;              /* increment through a subscript */
    counters[1]++;
    counters[2] += 5;           /* compound assignment through a subscript */

    for (i = 0; i < 2; i = i + 1) {
        for (j = 0; j < 3; j = j + 1) {
            total = total + grid[i][j];
        }
    }

    return p->x + p->y + total + counters[1] + counters[2]
        + sizeof(grid) + sizeof(struct Point);
}
