.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    $2, %eax
    movl    %eax, -4(%rbp)
    movl    $5, %eax
    movl    %eax, -8(%rbp)
    movl    $0, %eax
    movl    %eax, -12(%rbp)
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $3, %eax
    popq    %rdx
    movl    %eax, %ecx
    movl    %edx, %eax
    sall    %cl, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    -8(%rbp), %eax
    pushq   %rax
    movl    $1, %eax
    popq    %rdx
    movl    %eax, %ecx
    movl    %edx, %eax
    sarl    %cl, %eax
    popq    %rdx
    subl    %eax, %edx
    movl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $2, %eax
    popq    %rdx
    imull   %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $7, %eax
    popq    %rdx
    pushq   %rax
    movl    %edx, %eax
    popq    %rcx
    cdq
    idivl   %ecx
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $5, %eax
    popq    %rdx
    pushq   %rax
    movl    %edx, %eax
    popq    %rcx
    cdq
    idivl   %ecx
    movl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $8, %eax
    popq    %rdx
    orl     %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $15, %eax
    popq    %rdx
    andl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $3, %eax
    popq    %rdx
    xorl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $4, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $4, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $2, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    -4(%rbp), %eax
    pushq   %rax
    addl    $1, %eax
    movl    %eax, -4(%rbp)
    popq    %rax
    pushq   %rax
    movl    -4(%rbp), %eax
    addl    $1, %eax
    movl    %eax, -4(%rbp)
    popq    %rdx
    addl    %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    -8(%rbp), %eax
    pushq   %rax
    subl    $1, %eax
    movl    %eax, -8(%rbp)
    popq    %rax
    pushq   %rax
    movl    -8(%rbp), %eax
    subl    $1, %eax
    movl    %eax, -8(%rbp)
    popq    %rdx
    addl    %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    -8(%rbp), %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    setg    %al
    cmpl    $0, %eax
    je      .L1
    movl    $10, %eax
    jmp     .L2
.L1:
    movl    $20, %eax
.L2:
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $1, %eax
    pushq   %rax
    leaq    -4(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $2, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    -8(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
