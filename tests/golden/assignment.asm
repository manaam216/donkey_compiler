.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    $3, %eax
    movl    %eax, -4(%rbp)
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $2, %eax
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
    imull   %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.section .note.GNU-stack,"",@progbits
