.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    $260, %eax
    movl    %eax, -4(%rbp)
    movl    $65535, %eax
    movl    %eax, -8(%rbp)
    movl    $0, %eax
    movl    %eax, -12(%rbp)
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    -4(%rbp), %eax
    movsbl  %al, %eax
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
    movsbl  %al, %eax
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
    movzbl  %al, %eax
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
    movswl  %ax, %eax
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
    movswl  %ax, %eax
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
    movzwl  %ax, %eax
    pushq   %rax
    movl    $7, %eax
    popq    %rdx
    pushq   %rax
    movl    %edx, %eax
    popq    %rcx
    cdq
    idivl   %ecx
    movl    %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $3, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    cltq
    pushq   %rax
    movl    $2, %eax
    cltq
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
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    movl    %eax, %eax
    pushq   %rax
    movl    $1, %eax
    movl    %eax, %eax
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
    movl    $8, %eax
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
