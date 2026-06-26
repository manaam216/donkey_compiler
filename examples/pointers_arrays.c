int main()
{
    int x = 3;
    int *p;
    int a[4];

    p = &x;
    *p = *p + 4;
    a[0] = x;
    a[1] = *p + 5;
    return a[0] + a[1];
}
