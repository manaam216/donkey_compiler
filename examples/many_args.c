/*
 * Calling convention coverage. System V passes the first six integer or
 * pointer arguments in registers (rdi, rsi, rdx, rcx, r8, r9) and the rest on
 * the stack, which must stay 16-byte aligned at the call.
 *
 * Nine arguments exercises both paths and an odd number of stack arguments,
 * which is the case that needs explicit padding.
 */
int nine(int a, int b, int c, int d, int e, int f, int g, int h, int i)
{
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8 + i * 9;
}

int six(int a, int b, int c, int d, int e, int f)
{
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6;
}

int main()
{
    return nine(1, 2, 3, 4, 5, 6, 7, 8, 9) + six(1, 2, 3, 4, 5, 6);
}
