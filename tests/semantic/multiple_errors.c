/* Error recovery: semantic analysis reports all four, not just the first. */
int main()
{
    int a;
    a = undeclared_one;
    a = undeclared_two;
    b = 3;
    return a + undeclared_three;
}
