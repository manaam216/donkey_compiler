.data
    .align  4
.globl table
table:
    .long   3
    .long   5
    .long   0
    .long   0
    .align  4
.globl zeros
zeros:
    .long   0
    .long   0
.text
.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $32, %rsp
    movl    $1, %eax
    movl    %eax, -12(%rbp)
    movl    $2, %eax
    movl    %eax, -8(%rbp)
    movl    $3, %eax
    movl    %eax, -4(%rbp)
    movq    $0, -24(%rbp)
    leaq    table(%rip), %rax
    pushq   %rax
    leaq    -24(%rbp), %rax
    popq    %rdx
    movq    %rdx, (%rax)
    movq    %rdx, %rax
    leaq    -12(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    movl    (%rax), %eax
    pushq   %rax
    movl    $7, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    table(%rip), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    leaq    table(%rip), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    movl    (%rax), %eax
    pushq   %rax
    leaq    table(%rip), %rax
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
    movq    -24(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    popq    %rdx
    cltq
    imulq   $4, %rax
    addq    %rdx, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    table(%rip), %rax
    pushq   %rax
    movl    $3, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    zeros(%rip), %rax
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
    leaq    -12(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.section .note.GNU-stack,"",@progbits
