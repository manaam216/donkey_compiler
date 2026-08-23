.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    $1, %eax
    movl    %eax, -4(%rbp)
    movl    -4(%rbp), %eax
    cmpl    $0, %eax
    jne     .L1
    movl    $10, %eax
    pushq   %rax
    movl    $0, %eax
    popq    %rdx
    pushq   %rax
    movl    %edx, %eax
    popq    %rcx
    cdq
    idivl   %ecx
    cmpl    $0, %eax
    jne     .L1
    movl    $0, %eax
    jmp     .L2
.L1:
    movl    $1, %eax
.L2:
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
