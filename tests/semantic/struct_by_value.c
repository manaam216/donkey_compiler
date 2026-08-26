/* Passing a struct by value is refused rather than quietly miscompiled. */
struct P { int x; int y; };

int take(struct P p)
{
    return p.x + p.y;
}

int main()
{
    struct P a;
    a.x = 1;
    a.y = 2;
    return take(a);
}
