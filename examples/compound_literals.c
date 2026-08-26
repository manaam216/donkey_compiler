/*
 * Compound literals: an unnamed object written where it appears.
 *
 * `(struct Point){3, 4}` has the same storage duration as a local, so the
 * compiler gives it a frame slot even though no declaration asked for one.
 * The initializer accepts designators like any other.
 */
struct Point {
    int x;
    int y;
};

int sum(struct Point p)
{
    return p.x + p.y;
}

int main()
{
    struct Point a;
    int total = 0;

    total = total + sum((struct Point){3, 4});       /* passed by value: 7 */
    total = total + sum((struct Point){.y = 5});     /* designated; x is 0 */

    a = (struct Point){10, 20};                      /* assigned from one */
    total = total + a.x + a.y;                       /* 30 */

    return total;
}
