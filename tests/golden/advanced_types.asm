.data
.LC0:
    .byte   72, 105, 0
.text
.globl first
first:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movq    %rdi, -8(%rbp)
    movq    -8(%rbp), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    movl    (%rax), %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $64, %rsp
    movl    $7, %eax
    movl    %eax, -16(%rbp)
    movl    $9, %eax
    movl    %eax, -12(%rbp)
    movl    $11, %eax
    movl    %eax, -8(%rbp)
    movl    $13, %eax
    movl    %eax, -4(%rbp)
    leaq    -16(%rbp), %rax
    movq    %rax, -24(%rbp)
    leaq    -16(%rbp), %rax
    pushq   %rax
    movl    $3, %eax
    popq    %rdx
    cltq
    imulq   $4, %rax
    addq    %rdx, %rax
    movq    %rax, -32(%rbp)
    movl    $65, %eax
    movsbl  %al, %eax
    movl    %eax, -36(%rbp)
    movl    $10, %eax
    movsbl  %al, %eax
    movl    %eax, -40(%rbp)
    leaq    .LC0(%rip), %rax
    movq    %rax, -48(%rbp)
    movq    $0, -56(%rbp)
    leaq    -16(%rbp), %rax
    pushq   %rax
    popq    %rdi
    movl    $0, %eax
    call    first
    pushq   %rax
    leaq    -56(%rbp), %rax
    addq    $4, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movq    -32(%rbp), %rax
    pushq   %rax
    movq    -24(%rbp), %rax
    popq    %rdx
    subq    %rax, %rdx
    movq    %rdx, %rax
    cqto
    movq    $4, %rcx
    idivq   %rcx
    pushq   %rax
    leaq    -56(%rbp), %rax
    addq    $0, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    leaq    -56(%rbp), %rax
    addq    $4, %rax
    movl    (%rax), %eax
    pushq   %rax
    leaq    -56(%rbp), %rax
    addq    $0, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movsbl  -36(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movsbl  -40(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movq    -48(%rbp), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $1, %rax
    popq    %rdx
    addq    %rdx, %rax
    movsbl  (%rax), %eax
    movsbl  %al, %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
