/*
 * System V splits a struct into eightbytes. With no floating fields each is
 * INTEGER class, so up to sixteen bytes travels in one or two registers and
 * works. Beyond that the struct is MEMORY class and has to be copied onto the
 * stack, which is not implemented -- so it is refused rather than quietly
 * miscompiled.
 */
struct Large { int a; int b; int c; int d; int e; int f; };

int take(struct Large s)
{
    return s.a;
}

int main()
{
    struct Large value;

    value.a = 1;
    return take(value);
}
