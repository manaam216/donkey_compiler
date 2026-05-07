int helper(int base, int extra)
{
    int x = 2;
    return base + extra + x;
}

int unused()
{
    return 99;
}

int main()
{
    int y = 7;
    return helper(y, 4) + 3;
}
