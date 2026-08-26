.globl classify
classify:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    %edi, -4(%rbp)
    movl    $0, %eax
    movl    %eax, -8(%rbp)
    movl    -4(%rbp), %eax
    cmpl    $0, %eax
    je      .L2
    cmpl    $1, %eax
    je      .L3
    cmpl    $2, %eax
    je      .L4
    cmpl    $9, %eax
    je      .L5
    jmp     .L6
.L2:
    movl    $100, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    jmp     .L1
.L3:
.L4:
    movl    $200, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    jmp     .L1
.L5:
    movl    $900, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    jmp     .L1
.L6:
    movl    $1, %eax
    negl    %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
.L1:
    movl    -8(%rbp), %eax
    jmp     .L0
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
.L9:
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    -8(%rbp), %eax
    pushq   %rax
    popq    %rdi
    movl    $0, %eax
    call    classify
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -4(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
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
.L10:
    movl    -8(%rbp), %eax
    pushq   %rax
    movl    $4, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    setl    %al
    cmpl    $0, %eax
    jne     .L9
.L11:
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $0, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    setg    %al
    cmpl    $0, %eax
    je      .L12
    jmp     .L8
    jmp     .L13
.L12:
.L13:
    movl    $999, %eax
    negl    %eax
    pushq   %rax
    leaq    -4(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
.L8:
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $9, %eax
    pushq   %rax
    popq    %rdi
    movl    $0, %eax
    call    classify
    popq    %rdx
    addl    %edx, %eax
    jmp     .L7
    movl    $0, %eax
.L7:
    leave
    ret
.section .note.GNU-stack,"",@progbits
