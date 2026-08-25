.data
    .align  4
.globl shared
shared:
    .long   5
    .align  4
.globl zero
zero:
    .long   0
.text
.globl helper
helper:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    %edi, -4(%rbp)
    movl    shared(%rip), %eax
    pushq   %rax
    movl    -4(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    shared(%rip), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    shared(%rip), %eax
    pushq   %rax
    movl    $1, %eax
    popq    %rdx
    subl    %eax, %edx
    movl    %edx, %eax
    pushq   %rax
    leaq    zero(%rip), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    zero(%rip), %eax
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
    movl    $2, %eax
    movl    %eax, -4(%rbp)
    movl    $4, %eax
    pushq   %rax
    popq    %rdi
    movl    $0, %eax
    call    helper
    movl    %eax, -8(%rbp)
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $3, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -4(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -8(%rbp), %eax
    pushq   %rax
    movl    -4(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    zero(%rip), %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
.section .note.GNU-stack,"",@progbits
