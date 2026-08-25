.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $48, %rsp
    movq    $0, -24(%rbp)
    movq    $0, -16(%rbp)
    movq    $0, -8(%rbp)
    movw    $0, -32(%rbp)
    movw    $0, -30(%rbp)
    movw    $0, -28(%rbp)
    movw    $0, -26(%rbp)
    movl    $0, -36(%rbp)
    movl    $1, %eax
    pushq   %rax
    leaq    -24(%rbp), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $8, %rax
    popq    %rdx
    addq    %rdx, %rax
    addq    $4, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $2, %eax
    pushq   %rax
    leaq    -24(%rbp), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $8, %rax
    popq    %rdx
    addq    %rdx, %rax
    addq    $0, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $10, %eax
    pushq   %rax
    leaq    -24(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    cltq
    imulq   $8, %rax
    popq    %rdx
    addq    %rdx, %rax
    addq    $4, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $20, %eax
    pushq   %rax
    leaq    -24(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    cltq
    imulq   $8, %rax
    popq    %rdx
    addq    %rdx, %rax
    addq    $0, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $100, %eax
    pushq   %rax
    leaq    -24(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $8, %rax
    popq    %rdx
    addq    %rdx, %rax
    addq    $4, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $200, %eax
    pushq   %rax
    leaq    -24(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $8, %rax
    popq    %rdx
    addq    %rdx, %rax
    addq    $0, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $300, %eax
    movswl  %ax, %eax
    pushq   %rax
    leaq    -32(%rbp), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $2, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movw    %dx, (%rax)
    movl    %edx, %eax
    movl    $400, %eax
    movswl  %ax, %eax
    pushq   %rax
    leaq    -32(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    cltq
    imulq   $2, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movw    %dx, (%rax)
    movl    %edx, %eax
    movl    $500, %eax
    movswl  %ax, %eax
    pushq   %rax
    leaq    -32(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $2, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movw    %dx, (%rax)
    movl    %edx, %eax
    movl    $600, %eax
    movswl  %ax, %eax
    pushq   %rax
    leaq    -32(%rbp), %rax
    pushq   %rax
    movl    $3, %eax
    cltq
    imulq   $2, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movw    %dx, (%rax)
    movl    %edx, %eax
    leaq    -24(%rbp), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $8, %rax
    popq    %rdx
    addq    %rdx, %rax
    addq    $4, %rax
    movl    (%rax), %eax
    pushq   %rax
    leaq    -24(%rbp), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $8, %rax
    popq    %rdx
    addq    %rdx, %rax
    addq    $0, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -24(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    cltq
    imulq   $8, %rax
    popq    %rdx
    addq    %rdx, %rax
    addq    $4, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -24(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    cltq
    imulq   $8, %rax
    popq    %rdx
    addq    %rdx, %rax
    addq    $0, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -24(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $8, %rax
    popq    %rdx
    addq    %rdx, %rax
    addq    $4, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -24(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $8, %rax
    popq    %rdx
    addq    %rdx, %rax
    addq    $0, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -36(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -36(%rbp), %eax
    pushq   %rax
    leaq    -32(%rbp), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $2, %rax
    popq    %rdx
    addq    %rdx, %rax
    movswl  (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -32(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    cltq
    imulq   $2, %rax
    popq    %rdx
    addq    %rdx, %rax
    movswl  (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -32(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $2, %rax
    popq    %rdx
    addq    %rdx, %rax
    movswl  (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -32(%rbp), %rax
    pushq   %rax
    movl    $3, %eax
    cltq
    imulq   $2, %rax
    popq    %rdx
    addq    %rdx, %rax
    movswl  (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -36(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -36(%rbp), %eax
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
