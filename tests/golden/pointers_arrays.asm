.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $32, %rsp
    movl    $3, %eax
    movl    %eax, -4(%rbp)
    movq    $0, -16(%rbp)
    movl    $0, -32(%rbp)
    movl    $0, -28(%rbp)
    movl    $0, -24(%rbp)
    movl    $0, -20(%rbp)
    leaq    -4(%rbp), %rax
    pushq   %rax
    leaq    -16(%rbp), %rax
    popq    %rdx
    movq    %rdx, (%rax)
    movq    %rdx, %rax
    movq    -16(%rbp), %rax
    movl    (%rax), %eax
    pushq   %rax
    movl    $4, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movq    -16(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -4(%rbp), %eax
    pushq   %rax
    leaq    -32(%rbp), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movq    -16(%rbp), %rax
    movl    (%rax), %eax
    pushq   %rax
    movl    $5, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -32(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    leaq    -32(%rbp), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    movl    (%rax), %eax
    pushq   %rax
    leaq    -32(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
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
