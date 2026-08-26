/*
 * Passing a struct by value. System V classifies a struct by size; with no
 * floating-point types every field is INTEGER class, so one of eight bytes or
 * fewer travels in a single register.
 */
struct Point {
    int x;
    int y;
};

struct Small {
    int only;
};

int sum(struct Point p)
{
    return p.x + p.y;
}

int scaled(struct Point p, int factor)
{
    return (p.x + p.y) * factor;
}

int just_one(struct Small s)
{
    return s.only;
}

int main()
{
    struct Point p;
    struct Small s;

    p.x = 3;
    p.y = 4;
    s.only = 100;

    return sum(p)                /* 7 */
        + scaled(p, 2)           /* 14 */
        + just_one(s);           /* 100 */
}
