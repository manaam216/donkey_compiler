/*
 * The -> operator, and ++/-- applied to things other than plain variables.
 * Both need an lvalue address rather than a named symbol.
 */
struct Point { int x; int y; };

int main()
{
    struct Point p;
    struct Point *q;
    int a[3];
    int total;

    a[0] = 10;
    a[1] = 20;
    a[2] = 30;

    p.x = 3;
    p.y = 4;
    q = &p;

    q->x = q->x + 10;           /* read and write through a pointer */
    a[1]++;                     /* increment an array element */
    ++a[2];
    p.x++;                      /* increment a struct field */

    total = q->x + q->y + a[0] + a[1] + a[2] + p.x;
    return total;
}
