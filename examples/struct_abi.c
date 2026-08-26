/*
 * Passing and returning structs by value.
 *
 * System V splits a struct into eightbytes and classifies each. With no
 * floating fields every one is INTEGER class, so a struct of up to sixteen
 * bytes travels in one or two registers -- and comes back in %rax and %rdx the
 * same way. Larger structs are MEMORY class and are still refused.
 */
struct Pair {
    int x;
    int y;
};

struct Quad {
    int a;
    int b;
    int c;
    int d;
};

struct Pair make_pair(int x, int y)
{
    struct Pair p;

    p.x = x;
    p.y = y;
    return p;
}

int sum_pair(struct Pair p)
{
    return p.x + p.y;
}

int sum_quad(struct Quad q)
{
    return q.a + q.b + q.c + q.d;
}

/* Integer and struct arguments draw from the same register sequence. */
int mixed(int lead, struct Quad q, int tail)
{
    return lead + q.a + q.d + tail;
}

int main()
{
    struct Quad q;
    struct Pair returned;
    int total = 0;

    q.a = 1;
    q.b = 2;
    q.c = 3;
    q.d = 4;

    returned = make_pair(10, 20);            /* returned in %rax */
    total = total + returned.x + returned.y; /* 30 */

    total = total + sum_pair(make_pair(5, 6));   /* returned, then passed: 11 */
    total = total + sum_quad(q);                 /* two registers: 10 */
    total = total + mixed(100, q, 200);          /* 305 */

    return total;
}
