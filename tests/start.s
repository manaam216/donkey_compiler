/*
 * Process entry point for tests that link against libc without crt1.o.
 *
 * This has to be assembly rather than C. System V says a function is entered
 * with %rsp ≡ 8 (mod 16), because the call pushed a return address. A C
 * _start compiles to `pushq %rbp; call main`, which leaves main entered
 * 16-byte aligned instead -- so every call main makes is misaligned by 8.
 * Non-SSE functions survive that; a variadic one like printf executes movaps
 * against the stack and faults.
 */
    .text
    .globl _start
_start:
    xorl    %ebp, %ebp          /* deepest frame, per the ABI */
    andq    $-16, %rsp          /* the kernel aligns it; make sure */
    call    main
    movl    %eax, %edi          /* exit with main's result */
    /*
     * libc's exit, not the raw syscall: it runs the atexit handlers that flush
     * stdio. Exiting straight to the kernel discards whatever printf had
     * buffered, which loses output whenever stdout is not a terminal.
     */
    call    exit
    hlt                         /* exit does not return */
