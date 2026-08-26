.globl sum
sum:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movq    %rdi, -8(%rbp)
    leaq    -8(%rbp), %rax
    addq    $0, %rax
    movl    (%rax), %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    addq    $4, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $48, %rsp
    movq    $0, -8(%rbp)
    movl    $0, %eax
    movl    %eax, -12(%rbp)
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $0, -20(%rbp)
    movl    $0, -16(%rbp)
    movl    $3, %eax
    movl    %eax, -20(%rbp)
    movl    $4, %eax
    movl    %eax, -16(%rbp)
    leaq    -20(%rbp), %rax
    movq    0(%rax), %rdx
    pushq   %rdx
    popq    %rdi
    movl    $0, %eax
    call    sum
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $0, -28(%rbp)
    movl    $0, -24(%rbp)
    movl    $5, %eax
    movl    %eax, -24(%rbp)
    leaq    -28(%rbp), %rax
    movq    0(%rax), %rdx
    pushq   %rdx
    popq    %rdi
    movl    $0, %eax
    call    sum
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $0, -36(%rbp)
    movl    $0, -32(%rbp)
    movl    $10, %eax
    movl    %eax, -36(%rbp)
    movl    $20, %eax
    movl    %eax, -32(%rbp)
    leaq    -36(%rbp), %rax
    pushq   %rax
    leaq    -8(%rbp), %rax
    popq    %rdx
    movq    0(%rdx), %rcx
    movq    %rcx, 0(%rax)
    movl    -12(%rbp), %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    addq    $0, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    addq    $4, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
.section .note.GNU-stack,"",@progbits
