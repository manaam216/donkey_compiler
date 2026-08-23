.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    movl    $1, %eax
    pushq   %rax
    movl    $2, %eax
    pushq   %rax
    movl    $3, %eax
    pushq   %rax
    movl    $4, %eax
    popq    %rdx
    addl    %edx, %eax
    popq    %rdx
    imull   %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    $0, %eax
    cmpl    $0, %eax
    movl    $0, %eax
    sete    %al
    popq    %rdx
    subl    %eax, %edx
    movl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
