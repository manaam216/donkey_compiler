/*
 * A struct of eight bytes or fewer is passed in one register and works. A
 * larger one needs two registers or a stack copy, which is not implemented, so
 * it is refused rather than quietly miscompiled.
 */
struct Large { int a; int b; int c; int d; };

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
