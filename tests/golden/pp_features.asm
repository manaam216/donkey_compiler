.globl myfunc
myfunc:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    %edi, -4(%rbp)
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $10, %eax
    pushq   %rax
    movl    $5, %eax
    popq    %rdx
    addl    %edx, %eax
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
    subq    $32, %rsp
    movl    $1, %eax
    pushq   %rax
    movl    $2, %eax
    popq    %rdx
    addl    %edx, %eax
    movl    %eax, -4(%rbp)
    movl    $3, %eax
    pushq   %rax
    movl    $3, %eax
    popq    %rdx
    addl    %edx, %eax
    movl    %eax, -8(%rbp)
    movl    $4, %eax
    pushq   %rax
    movl    $4, %eax
    popq    %rdx
    addl    %edx, %eax
    movl    %eax, -12(%rbp)
    movl    $100, %eax
    pushq   %rax
    movl    $7, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    $2, %eax
    popq    %rdx
    addl    %edx, %eax
    movl    %eax, -16(%rbp)
    movl    $1, %eax
    pushq   %rax
    popq    %rdi
    movl    $0, %eax
    call    myfunc
    movl    %eax, -20(%rbp)
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    -8(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -12(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -16(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -20(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
