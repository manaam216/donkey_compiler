.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $32, %rsp
    movq    $0, -8(%rbp)
    movq    $0, -16(%rbp)
    movl    $0, -28(%rbp)
    movl    $0, -24(%rbp)
    movl    $0, -20(%rbp)
    movl    $0, -32(%rbp)
    movl    $10, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $20, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $30, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $3, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    addq    $4, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $4, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    addq    $0, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    leaq    -8(%rbp), %rax
    pushq   %rax
    leaq    -16(%rbp), %rax
    popq    %rdx
    movq    %rdx, (%rax)
    movq    %rdx, %rax
    movq    -16(%rbp), %rax
    addq    $4, %rax
    movl    (%rax), %eax
    pushq   %rax
    movl    $10, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movq    -16(%rbp), %rax
    addq    $4, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    leaq    -28(%rbp), %rax
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
    leaq    -28(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    pushq   %rax
    movl    (%rax), %eax
    addl    $1, %eax
    movq    %rax, %rdx
    popq    %rax
    movl    %edx, (%rax)
    movq    %rdx, %rax
    leaq    -8(%rbp), %rax
    addq    $4, %rax
    pushq   %rax
    movl    (%rax), %eax
    pushq   %rax
    addl    $1, %eax
    movq    %rax, %rdx
    popq    %rcx
    popq    %rax
    movl    %edx, (%rax)
    movq    %rcx, %rax
    movq    -16(%rbp), %rax
    addq    $4, %rax
    movl    (%rax), %eax
    pushq   %rax
    movq    -16(%rbp), %rax
    addq    $0, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
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
    leaq    -28(%rbp), %rax
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
    leaq    -8(%rbp), %rax
    addq    $4, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -32(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -32(%rbp), %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.section .note.GNU-stack,"",@progbits
