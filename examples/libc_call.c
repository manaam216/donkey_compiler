/*
 * Calling the C library. This is what prototypes and varargs are for: printf
 * is declared here exactly as a header would declare it, and called with a
 * varying number of arguments.
 */
int printf(const char *format, ...);
int puts(const char *s);

int square(int n)
{
    return n * n;
}

int main()
{
    int values[3];
    int i;
    int total = 0;

    values[0] = 2;
    values[1] = 3;
    values[2] = 4;

    puts("donkey calling libc");
    printf("square(%d) = %d\n", 5, square(5));

    for (i = 0; i < 3; i = i + 1) {
        printf("values[%d] = %d\n", i, values[i]);
        total = total + values[i];
    }

    printf("total = %d\n", total);
    return total;
}
