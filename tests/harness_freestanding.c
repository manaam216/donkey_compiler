/*
 * Freestanding version of tests/harness.c, for verifying x86-64 output on a
 * machine with no Linux libc to link against.
 *
 * It does the same job -- call the compiled program and print its full 32-bit
 * result -- but talks to the kernel directly, so it can be linked with nothing
 * but ld.lld. tests/harness.c is the normal path wherever a libc is available.
 *
 * Build:
 *   clang --target=x86_64-unknown-linux-gnu -ffreestanding -nostdlib -c ...
 */

extern int donkey_main(void);

static long sys_write(long fd, const char *buf, long count)
{
    long result;

    __asm__ volatile ("syscall"
        : "=a" (result)
        : "a" (1L), "D" (fd), "S" (buf), "d" (count)
        : "rcx", "r11", "memory");
    return result;
}

__attribute__((noreturn))
static void sys_exit(long status)
{
    __asm__ volatile ("syscall" : : "a" (60L), "D" (status));
    __builtin_unreachable();
}

void _start(void)
{
    char buffer[16];
    int index = (int)sizeof(buffer);
    int value = donkey_main();
    unsigned int magnitude;
    int negative = value < 0;

    /* Negate in unsigned space so the most negative int does not overflow. */
    magnitude = negative ? 0u - (unsigned int)value : (unsigned int)value;

    buffer[--index] = '\n';
    if (magnitude == 0) {
        buffer[--index] = '0';
    }
    while (magnitude > 0) {
        buffer[--index] = (char)('0' + (magnitude % 10));
        magnitude /= 10;
    }
    if (negative) {
        buffer[--index] = '-';
    }

    sys_write(1, buffer + index, (long)sizeof(buffer) - index);
    sys_exit(0);
}
