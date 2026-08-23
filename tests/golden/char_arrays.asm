.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $32, %rsp
    movb    $0, -4(%rbp)
    movb    $0, -3(%rbp)
    movb    $0, -2(%rbp)
    movb    $0, -1(%rbp)
    movq    $0, -16(%rbp)
    movl    $0, -20(%rbp)
    movl    $65, %eax
    movsbl  %al, %eax
    pushq   %rax
    leaq    -4(%rbp), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $1, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movb    %dl, (%rax)
    movl    %edx, %eax
    movl    $66, %eax
    movsbl  %al, %eax
    pushq   %rax
    leaq    -4(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    cltq
    imulq   $1, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movb    %dl, (%rax)
    movl    %edx, %eax
    movl    $67, %eax
    movsbl  %al, %eax
    pushq   %rax
    leaq    -4(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $1, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movb    %dl, (%rax)
    movl    %edx, %eax
    movl    $68, %eax
    movsbl  %al, %eax
    pushq   %rax
    leaq    -4(%rbp), %rax
    pushq   %rax
    movl    $3, %eax
    cltq
    imulq   $1, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movb    %dl, (%rax)
    movl    %edx, %eax
    leaq    -4(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $1, %rax
    popq    %rdx
    addq    %rdx, %rax
    movsbl  (%rax), %eax
    pushq   %rax
    leaq    -20(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -20(%rbp), %eax
    pushq   %rax
    leaq    -4(%rbp), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $1, %rax
    popq    %rdx
    addq    %rdx, %rax
    movsbl  (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -4(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    cltq
    imulq   $1, %rax
    popq    %rdx
    addq    %rdx, %rax
    movsbl  (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -4(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $1, %rax
    popq    %rdx
    addq    %rdx, %rax
    movsbl  (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -4(%rbp), %rax
    pushq   %rax
    movl    $3, %eax
    cltq
    imulq   $1, %rax
    popq    %rdx
    addq    %rdx, %rax
    movsbl  (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -20(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    leaq    -4(%rbp), %rax
    pushq   %rax
    leaq    -16(%rbp), %rax
    popq    %rdx
    movq    %rdx, (%rax)
    movq    %rdx, %rax
    movq    -16(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    popq    %rdx
    cltq
    imulq   $1, %rax
    addq    %rdx, %rax
    pushq   %rax
    leaq    -16(%rbp), %rax
    popq    %rdx
    movq    %rdx, (%rax)
    movq    %rdx, %rax
    movl    -20(%rbp), %eax
    pushq   %rax
    movq    -16(%rbp), %rax
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
    movl    $4, %eax
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
