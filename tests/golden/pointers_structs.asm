.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $80, %rsp
    movq    $0, -8(%rbp)
    movq    $0, -16(%rbp)
    movq    $0, -24(%rbp)
    movl    $0, -48(%rbp)
    movl    $0, -36(%rbp)
    movl    $0, -60(%rbp)
    movl    $0, -56(%rbp)
    movl    $0, -52(%rbp)
    movl    $0, -64(%rbp)
    movl    $0, -68(%rbp)
    movl    $0, %eax
    movl    %eax, -72(%rbp)
    movl    $3, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    addq    $0, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $4, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    addq    $4, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    leaq    -8(%rbp), %rax
    pushq   %rax
    leaq    -16(%rbp), %rax
    popq    %rdx
    movq    0(%rdx), %rcx
    movq    %rcx, 0(%rax)
    leaq    -16(%rbp), %rax
    pushq   %rax
    leaq    -24(%rbp), %rax
    popq    %rdx
    movq    %rdx, (%rax)
    movq    %rdx, %rax
    movq    -24(%rbp), %rax
    addq    $0, %rax
    movl    (%rax), %eax
    pushq   %rax
    movl    $10, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movq    -24(%rbp), %rax
    addq    $0, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $0, %eax
    pushq   %rax
    leaq    -64(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
.L1:
    movl    -64(%rbp), %eax
    pushq   %rax
    movl    $2, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    setl    %al
    cmpl    $0, %eax
    je      .L3
    movl    $0, %eax
    pushq   %rax
    leaq    -68(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
.L4:
    movl    -68(%rbp), %eax
    pushq   %rax
    movl    $3, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    setl    %al
    cmpl    $0, %eax
    je      .L6
    movl    -64(%rbp), %eax
    pushq   %rax
    movl    $10, %eax
    popq    %rdx
    imull   %edx, %eax
    pushq   %rax
    movl    -68(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -48(%rbp), %rax
    pushq   %rax
    movl    -64(%rbp), %eax
    cltq
    imulq   $12, %rax
    popq    %rdx
    addq    %rdx, %rax
    pushq   %rax
    movl    -68(%rbp), %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
.L5:
    movl    -68(%rbp), %eax
    pushq   %rax
    movl    $1, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -68(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    jmp     .L4
.L6:
.L2:
    movl    -64(%rbp), %eax
    pushq   %rax
    movl    $1, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -64(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    jmp     .L1
.L3:
    movl    $0, %eax
    pushq   %rax
    leaq    -60(%rbp), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $0, %eax
    pushq   %rax
    leaq    -60(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $0, %eax
    pushq   %rax
    leaq    -60(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    leaq    -60(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    pushq   %rax
    movl    (%rax), %eax
    pushq   %rax
    addl    $1, %eax
    movq    %rax, %rdx
    popq    %rcx
    popq    %rax
    movl    %edx, (%rax)
    movq    %rcx, %rax
    leaq    -60(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    pushq   %rax
    movl    (%rax), %eax
    pushq   %rax
    addl    $1, %eax
    movq    %rax, %rdx
    popq    %rcx
    popq    %rax
    movl    %edx, (%rax)
    movq    %rcx, %rax
    leaq    -60(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    movl    (%rax), %eax
    pushq   %rax
    movl    $5, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -60(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $0, %eax
    pushq   %rax
    leaq    -64(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
.L7:
    movl    -64(%rbp), %eax
    pushq   %rax
    movl    $2, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    setl    %al
    cmpl    $0, %eax
    je      .L9
    movl    $0, %eax
    pushq   %rax
    leaq    -68(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
.L10:
    movl    -68(%rbp), %eax
    pushq   %rax
    movl    $3, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    setl    %al
    cmpl    $0, %eax
    je      .L12
    movl    -72(%rbp), %eax
    pushq   %rax
    leaq    -48(%rbp), %rax
    pushq   %rax
    movl    -64(%rbp), %eax
    cltq
    imulq   $12, %rax
    popq    %rdx
    addq    %rdx, %rax
    pushq   %rax
    movl    -68(%rbp), %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -72(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
.L11:
    movl    -68(%rbp), %eax
    pushq   %rax
    movl    $1, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -68(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    jmp     .L10
.L12:
.L8:
    movl    -64(%rbp), %eax
    pushq   %rax
    movl    $1, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -64(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    jmp     .L7
.L9:
    movq    -24(%rbp), %rax
    addq    $0, %rax
    movl    (%rax), %eax
    pushq   %rax
    movq    -24(%rbp), %rax
    addq    $4, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -72(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -60(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -60(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    $24, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    $8, %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.section .note.GNU-stack,"",@progbits
