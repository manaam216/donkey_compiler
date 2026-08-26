/*
 * float and double: literals, arithmetic, comparisons, conversions in both
 * directions, and the System V calling convention, which passes floating
 * arguments in SSE registers counted separately from the integer ones.
 */
double half(double x)
{
    return x / 2.0;
}

double weighted(double a, double b, double weight)
{
    return a * weight + b * (1.0 - weight);
}

/* Integer and floating parameters draw from separate register sequences. */
double mixed(int count, double value, int scale, double offset)
{
    return value * count + offset * scale;
}

int main()
{
    double a = 10.0;
    double b = 4.0;
    float f = 2.5f;
    int i = 7;
    int result = 0;

    result = result + (int)(a + b);          /* 14 */
    result = result + (int)(a - b);          /* 6 */
    result = result + (int)(a * b);          /* 40 */
    result = result + (int)(a / b);          /* 2 */
    result = result + (int)(f * 2.0);        /* float widens to double: 5 */
    result = result + (int)(a + i);          /* int converts to double: 17 */

    result = result + (a > b);               /* 1 */
    result = result + (a < b);               /* 0 */
    result = result + (a == 10.0);           /* 1 */
    result = result + (a >= 10.0);           /* 1 */

    result = result + (int)3.99;             /* truncates toward zero: 3 */
    result = result + (int)half(9.0);        /* 4 */
    result = result + (int)(weighted(10.0, 20.0, 0.25) * 10.0);   /* 175 */
    result = result + (int)mixed(3, 2.0, 4, 1.5);                 /* 12 */

    return result;
}
