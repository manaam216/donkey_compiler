.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    $0, -4(%rbp)
    movl    $0, -8(%rbp)
    movl    $1, %eax
    pushq   %rax
    leaq    -4(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $0, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $0, -12(%rbp)
    movl    $20, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -8(%rbp), %eax
    pushq   %rax
    movl    -12(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -8(%rbp), %eax
    pushq   %rax
    movl    -4(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $0, -12(%rbp)
    movl    $300, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -8(%rbp), %eax
    pushq   %rax
    movl    -12(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -8(%rbp), %eax
    pushq   %rax
    movl    -4(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
