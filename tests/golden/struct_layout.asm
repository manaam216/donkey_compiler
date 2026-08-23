.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $32, %rsp
    movl    $0, -12(%rbp)
    movl    $0, -16(%rbp)
    movl    $0, -20(%rbp)
    movl    $3, %eax
    movsbl  %al, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    addq    $8, %rax
    popq    %rdx
    movb    %dl, (%rax)
    movl    %edx, %eax
    movl    $1000, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    addq    $4, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $7, %eax
    movsbl  %al, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    addq    $0, %rax
    popq    %rdx
    movb    %dl, (%rax)
    movl    %edx, %eax
    movl    $1, %eax
    movsbl  %al, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    addq    $2, %rax
    popq    %rdx
    movb    %dl, (%rax)
    movl    %edx, %eax
    movl    $2, %eax
    movsbl  %al, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    addq    $1, %rax
    popq    %rdx
    movb    %dl, (%rax)
    movl    %edx, %eax
    movl    $3, %eax
    movsbl  %al, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    addq    $0, %rax
    popq    %rdx
    movb    %dl, (%rax)
    movl    %edx, %eax
    leaq    -12(%rbp), %rax
    addq    $8, %rax
    movsbl  (%rax), %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    addq    $4, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    addq    $0, %rax
    movsbl  (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -20(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -20(%rbp), %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    addq    $2, %rax
    movsbl  (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    addq    $1, %rax
    movsbl  (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    addq    $0, %rax
    movsbl  (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -20(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -20(%rbp), %eax
    pushq   %rax
    movl    $12, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    $3, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -20(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -20(%rbp), %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
