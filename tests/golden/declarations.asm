.data
    .align  4
.globl scale
scale:
    .long   3
.text
.globl twice
twice:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    %edi, -4(%rbp)
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $2, %eax
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
    subq    $48, %rsp
    movl    $1, %eax
    movl    %eax, -4(%rbp)
    movl    $2, %eax
    movl    %eax, -8(%rbp)
    movl    $0, -12(%rbp)
    movl    $5, %eax
    movl    %eax, -16(%rbp)
    movl    $200, %eax
    movzbl  %al, %eax
    movl    %eax, -20(%rbp)
    movq    $0, -32(%rbp)
    movl    $1, %eax
    movl    %eax, -36(%rbp)
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    -8(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    leaq    -12(%rbp), %rax
    pushq   %rax
    leaq    -32(%rbp), %rax
    popq    %rdx
    movq    %rdx, (%rax)
    movq    %rdx, %rax
    movl    -16(%rbp), %eax
    pushq   %rax
    popq    %rdi
    movl    $0, %eax
    call    twice
    pushq   %rax
    movzbl  -20(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movq    -32(%rbp), %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -36(%rbp), %eax
    pushq   %rax
    movl    $100, %eax
    popq    %rdx
    imull   %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    $20, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    $21, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    $2, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    scale(%rip), %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
.section .note.GNU-stack,"",@progbits
