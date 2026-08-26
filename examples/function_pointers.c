/*
 * Function pointers: TYPE (*name)(params), taking a function's address by
 * naming it, and calling through the pointer.
 */
int add(int a, int b) { return a + b; }
int multiply(int a, int b) { return a * b; }

int apply(int (*operation)(int, int), int a, int b)
{
    return operation(a, b);
}

int main()
{
    int (*op)(int, int);
    int total = 0;

    op = add;
    total = total + op(2, 3);           /* 5 */

    op = multiply;
    total = total + op(4, 5);           /* 20 */

    total = total + apply(add, 10, 1);  /* 11, passed as an argument */

    return total;
}
