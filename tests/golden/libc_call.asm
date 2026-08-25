.data
.LC0:
    .byte   100, 111, 110, 107, 101, 121, 32, 99, 97, 108, 108, 105, 110, 103, 32, 108, 105, 98, 99, 0
.LC1:
    .byte   115, 113, 117, 97, 114, 101, 40, 37, 100, 41, 32, 61, 32, 37, 100, 10, 0
.LC2:
    .byte   118, 97, 108, 117, 101, 115, 91, 37, 100, 93, 32, 61, 32, 37, 100, 10, 0
.LC3:
    .byte   116, 111, 116, 97, 108, 32, 61, 32, 37, 100, 10, 0
.text
.globl square
square:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    %edi, -4(%rbp)
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    -4(%rbp), %eax
    popq    %rdx
    imull   %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $32, %rsp
    movl    $0, -12(%rbp)
    movl    $0, -8(%rbp)
    movl    $0, -4(%rbp)
    movl    $0, -16(%rbp)
    movl    $0, %eax
    movl    %eax, -20(%rbp)
    movl    $2, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $3, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $4, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    leaq    .LC0(%rip), %rax
    pushq   %rax
    popq    %rdi
    movl    $0, %eax
    call    puts
    movl    $5, %eax
    pushq   %rax
    popq    %rdi
    movl    $0, %eax
    call    square
    pushq   %rax
    movl    $5, %eax
    pushq   %rax
    leaq    .LC1(%rip), %rax
    pushq   %rax
    popq    %rdi
    popq    %rsi
    popq    %rdx
    movl    $0, %eax
    call    printf
    movl    $0, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
.L2:
    movl    -16(%rbp), %eax
    pushq   %rax
    movl    $3, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    setl    %al
    cmpl    $0, %eax
    je      .L4
    leaq    -12(%rbp), %rax
    pushq   %rax
    movl    -16(%rbp), %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    movl    (%rax), %eax
    pushq   %rax
    movl    -16(%rbp), %eax
    pushq   %rax
    leaq    .LC2(%rip), %rax
    pushq   %rax
    popq    %rdi
    popq    %rsi
    popq    %rdx
    movl    $0, %eax
    call    printf
    movl    -20(%rbp), %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    pushq   %rax
    movl    -16(%rbp), %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -20(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
.L3:
    movl    -16(%rbp), %eax
    pushq   %rax
    movl    $1, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    jmp     .L2
.L4:
    movl    -20(%rbp), %eax
    pushq   %rax
    leaq    .LC3(%rip), %rax
    pushq   %rax
    popq    %rdi
    popq    %rsi
    movl    $0, %eax
    call    printf
    movl    -20(%rbp), %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
