.data
    .align  1
.globl global_byte
global_byte:
    .byte   44
    .align  1
.globl global_signed_byte
global_signed_byte:
    .byte   -1
.text
.globl narrow_return
narrow_return:
    pushq   %rbp
    movq    %rsp, %rbp
    movl    $300, %eax
    movzbl  %al, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.globl narrow_argument
narrow_argument:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    %edi, -4(%rbp)
    movzbl  -4(%rbp), %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $32, %rsp
    movl    $1, %eax
    negl    %eax
    movl    %eax, -4(%rbp)
    movl    $255, %eax
    movzbl  %al, %eax
    movl    %eax, -8(%rbp)
    movl    $1, %eax
    negl    %eax
    cltq
    movq    %rax, -16(%rbp)
    movl    $0, %eax
    movl    %eax, -20(%rbp)
    movzbl  -8(%rbp), %eax
    pushq   %rax
    addl    $1, %eax
    movzbl  %al, %eax
    movl    %eax, -8(%rbp)
    popq    %rax
    movl    $0, %eax
    call    narrow_return
    pushq   %rax
    movl    $300, %eax
    movzbl  %al, %eax
    pushq   %rax
    popq    %rdi
    movl    $0, %eax
    call    narrow_argument
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $1, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    seta    %al
    pushq   %rax
    movl    $10, %eax
    popq    %rdx
    imull   %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $31, %eax
    popq    %rdx
    movl    %eax, %ecx
    movl    %edx, %eax
    shrl    %cl, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $2, %eax
    popq    %rdx
    pushq   %rax
    movl    %edx, %eax
    popq    %rcx
    xorl    %edx, %edx
    divl    %ecx
    pushq   %rax
    movl    $100, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    seta    %al
    pushq   %rax
    movl    $20, %eax
    popq    %rdx
    imull   %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movzbl  global_byte(%rip), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movsbl  global_signed_byte(%rip), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movzbl  -8(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movq    -16(%rbp), %rax
    pushq   %rax
    movl    -20(%rbp), %eax
    cltq
    popq    %rdx
    cmpq    %rax, %rdx
    movl    $0, %eax
    setl    %al
    popq    %rdx
    addl    %edx, %eax
    jmp     .L2
    movl    $0, %eax
.L2:
    leave
    ret
.section .note.GNU-stack,"",@progbits
