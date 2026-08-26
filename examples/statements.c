/*
 * Statement forms added in Phase 9: switch, do/while, goto and labels, the
 * empty statement, and a bare return from a void function.
 */
void nothing(void)
{
    return;             /* a void function returns no value */
}

int classify(int n)
{
    int result;

    switch (n) {
        case 1:
            result = 10;
            break;
        case 2:
        case 3:
            result = 20;    /* two labels reaching one body */
            break;
        default:
            result = 30;
            break;
    }
    return result;
}

int count_down(int from)
{
    int total = 0;

    do {
        total = total + from;
        from = from - 1;
    } while (from > 0);

    return total;
}

int find_first_negative(int values[4])
{
    int i = 0;

    while (i < 4) {
        if (values[i] < 0) {
            goto found;
        }
        i = i + 1;
    }
    return -1;

found:
    return i;
}

int main()
{
    int values[4];
    int total = 0;

    ;                   /* an empty statement is allowed */
    nothing();

    values[0] = 5;
    values[1] = 6;
    values[2] = 0 - 7;
    values[3] = 8;

    total = classify(1) + classify(3) + classify(9);   /* 10 + 20 + 30 */
    total = total + count_down(4);                     /* 4+3+2+1 = 10 */
    total = total + find_first_negative(values);       /* 2 */

    return total;
}
