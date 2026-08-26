/*
 * Statement forms added in Phase 9: switch with fallthrough and a default,
 * do-while, goto with a label, and the empty statement.
 */
int classify(int n)
{
    int result = 0;

    switch (n) {
        case 0:
            result = 100;
            break;
        case 1:                 /* falls through to the next case */
        case 2:
            result = 200;
            break;
        case 9:
            result = 900;
            break;
        default:
            result = -1;
    }
    return result;
}

int main()
{
    int total = 0;
    int i = 0;

    /* The body runs before the condition is first tested. */
    do {
        total = total + classify(i);
        i = i + 1;
    } while (i < 4);

    ;                           /* a statement that does nothing */

    if (total > 0) goto done;
    total = -999;               /* unreachable while total is positive */
done:
    return total + classify(9);
}
