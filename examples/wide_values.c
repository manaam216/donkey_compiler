/*
 * Regression coverage for values that process exit codes cannot represent.
 * Exit codes are truncated to 8 unsigned bits, so the result below appears
 * as 118 to any exit-code assertion, and any miscompilation shifting it by a
 * multiple of 256 would be invisible. The stdout harness sees the real value.
 */
int main()
{
    int large;
    int negative;

    large = 100000;
    negative = 0 - 42;

    return large + negative;
}
