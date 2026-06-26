int main()
{
    int values[4];
    int *p;

    values[0] = 2;
    values[1] = 4;
    values[2] = 6;
    p = values;
    *(p + 3) = *(p + 1) + values[2];
    p++;
    return *p + *(p + 2);
}
