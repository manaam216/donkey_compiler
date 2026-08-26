.globl make_pair
make_pair:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    %edi, -4(%rbp)
    movl    %esi, -8(%rbp)
    movq    $0, -16(%rbp)
    movl    -4(%rbp), %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    addq    $4, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -8(%rbp), %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    addq    $0, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    leaq    -16(%rbp), %rax
    movq    (%rax), %rax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.globl sum_pair
sum_pair:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movq    %rdi, -8(%rbp)
    leaq    -8(%rbp), %rax
    addq    $4, %rax
    movl    (%rax), %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    addq    $0, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
.globl sum_quad
sum_quad:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movq    %rdi, -16(%rbp)
    movq    %rsi, -8(%rbp)
    leaq    -16(%rbp), %rax
    addq    $12, %rax
    movl    (%rax), %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    addq    $8, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    addq    $4, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    addq    $0, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L2
    movl    $0, %eax
.L2:
    leave
    ret
.globl mixed
mixed:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $32, %rsp
    movl    %edi, -4(%rbp)
    movq    %rsi, -20(%rbp)
    movq    %rdx, -12(%rbp)
    movl    %ecx, -24(%rbp)
    movl    -4(%rbp), %eax
    pushq   %rax
    leaq    -20(%rbp), %rax
    addq    $12, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -20(%rbp), %rax
    addq    $0, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -24(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L3
    movl    $0, %eax
.L3:
    leave
    ret
.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $32, %rsp
    movl    $0, -16(%rbp)
    movq    $0, -24(%rbp)
    movl    $0, %eax
    movl    %eax, -28(%rbp)
    movl    $1, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    addq    $12, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $2, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    addq    $8, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $3, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    addq    $4, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $4, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    addq    $0, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $20, %eax
    pushq   %rax
    movl    $10, %eax
    pushq   %rax
    popq    %rdi
    popq    %rsi
    movl    $0, %eax
    call    make_pair
    movq    %rax, %rcx
    leaq    -24(%rbp), %rax
    movq    %rcx, (%rax)
    movl    -28(%rbp), %eax
    pushq   %rax
    leaq    -24(%rbp), %rax
    addq    $4, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -24(%rbp), %rax
    addq    $0, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -28(%rbp), %eax
    pushq   %rax
    movl    $6, %eax
    pushq   %rax
    movl    $5, %eax
    pushq   %rax
    popq    %rdi
    popq    %rsi
    movl    $0, %eax
    call    make_pair
    pushq   %rax
    popq    %rdi
    movl    $0, %eax
    call    sum_pair
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -28(%rbp), %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    movq    8(%rax), %rdx
    pushq   %rdx
    movq    0(%rax), %rdx
    pushq   %rdx
    popq    %rdi
    popq    %rsi
    movl    $0, %eax
    call    sum_quad
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -28(%rbp), %eax
    pushq   %rax
    movl    $200, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    movq    8(%rax), %rdx
    pushq   %rdx
    movq    0(%rax), %rdx
    pushq   %rdx
    movl    $100, %eax
    pushq   %rax
    popq    %rdi
    popq    %rsi
    popq    %rdx
    popq    %rcx
    movl    $0, %eax
    call    mixed
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -28(%rbp), %eax
    jmp     .L4
    movl    $0, %eax
.L4:
    leave
    ret
.section .note.GNU-stack,"",@progbits
