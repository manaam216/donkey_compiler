.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    movl    $8, %eax
    pushq   %rax
    movl    $2, %eax
    popq    %rdx
    pushq   %rax
    movl    %edx, %eax
    popq    %rcx
    cdq
    idivl   %ecx
    negl    %eax
    pushq   %rax
    movl    $1, %eax
    notl    %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    $12, %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
