.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $32, %rsp
    movl    $0, -12(%rbp)
    movl    $0, -24(%rbp)
    movl    $1, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    addq    $8, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $2, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    addq    $4, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $3, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    addq    $0, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $0, %eax
    pushq   %rax
    leaq    -24(%rbp), %rax
    addq    $8, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $0, %eax
    pushq   %rax
    leaq    -24(%rbp), %rax
    addq    $4, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $0, %eax
    pushq   %rax
    leaq    -24(%rbp), %rax
    addq    $0, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    leaq    -12(%rbp), %rax
    pushq   %rax
    leaq    -24(%rbp), %rax
    popq    %rdx
    movq    0(%rdx), %rcx
    movq    %rcx, 0(%rax)
    movl    8(%rdx), %ecx
    movl    %ecx, 8(%rax)
    leaq    -24(%rbp), %rax
    addq    $8, %rax
    movl    (%rax), %eax
    pushq   %rax
    movl    $100, %eax
    popq    %rdx
    imull   %edx, %eax
    pushq   %rax
    leaq    -24(%rbp), %rax
    addq    $4, %rax
    movl    (%rax), %eax
    pushq   %rax
    movl    $10, %eax
    popq    %rdx
    imull   %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -24(%rbp), %rax
    addq    $0, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.section .note.GNU-stack,"",@progbits
