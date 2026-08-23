/*
 * Parser recovery: a missing semicolon in one function must not hide the
 * problem in the next. Both are reported.
 */
int first()
{
    return 1
}

int second()
{
    int x = 2
    return x;
}
