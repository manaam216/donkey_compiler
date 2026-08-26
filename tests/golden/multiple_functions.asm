.globl helper
helper:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    %edi, -4(%rbp)
    movl    %esi, -8(%rbp)
    movl    $2, %eax
    movl    %eax, -12(%rbp)
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    -8(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -12(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.globl unused
unused:
    pushq   %rbp
    movq    %rsp, %rbp
    movl    $99, %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    $7, %eax
    movl    %eax, -4(%rbp)
    movl    $4, %eax
    pushq   %rax
    movl    -4(%rbp), %eax
    pushq   %rax
    popq    %rdi
    popq    %rsi
    movl    $0, %eax
    call    helper
    pushq   %rax
    movl    $3, %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L2
    movl    $0, %eax
.L2:
    leave
    ret
.section .note.GNU-stack,"",@progbits
