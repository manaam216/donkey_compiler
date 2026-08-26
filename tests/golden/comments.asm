.globl helper
helper:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    %edi, -4(%rbp)
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $3, %eax
    popq    %rdx
    addl    %edx, %eax
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
    movl    $4, %eax
    movl    %eax, -4(%rbp)
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $5, %eax
    pushq   %rax
    popq    %rdi
    movl    $0, %eax
    call    helper
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -4(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -4(%rbp), %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
.section .note.GNU-stack,"",@progbits
