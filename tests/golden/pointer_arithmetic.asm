.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $32, %rsp
    movl    $0, -16(%rbp)
    movl    $0, -12(%rbp)
    movl    $0, -8(%rbp)
    movl    $0, -4(%rbp)
    movq    $0, -24(%rbp)
    movl    $2, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $4, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $6, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    leaq    -16(%rbp), %rax
    pushq   %rax
    leaq    -24(%rbp), %rax
    popq    %rdx
    movq    %rdx, (%rax)
    movq    %rdx, %rax
    movq    -24(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    popq    %rdx
    cltq
    imulq   $4, %rax
    addq    %rdx, %rax
    movl    (%rax), %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
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
    movq    -24(%rbp), %rax
    pushq   %rax
    movl    $3, %eax
    popq    %rdx
    cltq
    imulq   $4, %rax
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movq    -24(%rbp), %rax
    pushq   %rax
    addq    $4, %rax
    movq    %rax, -24(%rbp)
    popq    %rax
    movq    -24(%rbp), %rax
    movl    (%rax), %eax
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
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.section .note.GNU-stack,"",@progbits
