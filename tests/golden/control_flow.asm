.globl choose
choose:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    %edi, -4(%rbp)
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $5, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    setg    %al
    cmpl    $0, %eax
    je      .L1
    movl    -4(%rbp), %eax
    jmp     .L0
    jmp     .L2
.L1:
    movl    $5, %eax
    jmp     .L0
.L2:
    movl    $0, %eax
.L0:
    leave
    ret
.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    $0, %eax
    movl    %eax, -4(%rbp)
    movl    $0, %eax
    movl    %eax, -8(%rbp)
.L4:
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $5, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    setl    %al
    cmpl    $0, %eax
    je      .L5
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $1, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -4(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $3, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    sete    %al
    cmpl    $0, %eax
    je      .L6
    jmp     .L4
    jmp     .L7
.L6:
.L7:
    movl    -8(%rbp), %eax
    pushq   %rax
    movl    -4(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    jmp     .L4
.L5:
    movl    $0, %eax
    movl    %eax, -12(%rbp)
.L8:
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $5, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    setl    %al
    cmpl    $0, %eax
    je      .L10
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $4, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    sete    %al
    cmpl    $0, %eax
    je      .L11
    jmp     .L10
    jmp     .L12
.L11:
.L12:
    movl    -8(%rbp), %eax
    pushq   %rax
    movl    $1, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
.L9:
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
    jmp     .L8
.L10:
    movl    -8(%rbp), %eax
    pushq   %rax
    popq    %rdi
    movl    $0, %eax
    call    choose
    jmp     .L3
    movl    $0, %eax
.L3:
    leave
    ret
