int table[4] = {3, 5};
int zeros[2];

int main()
{
    int local[3] = {1, 2, 3};
    int *p;

    p = table;
    table[2] = local[1] + 7;
    return table[0] + table[1] + *(p + 2) + table[3] + zeros[1] + local[2];
}
