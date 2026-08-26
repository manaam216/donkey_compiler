.globl add
add:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    %edi, -4(%rbp)
    movl    %esi, -8(%rbp)
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    -8(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.globl multiply
multiply:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    %edi, -4(%rbp)
    movl    %esi, -8(%rbp)
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    -8(%rbp), %eax
    popq    %rdx
    imull   %edx, %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
.globl apply
apply:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movq    %rdi, -8(%rbp)
    movl    %esi, -12(%rbp)
    movl    %edx, -16(%rbp)
    movl    -16(%rbp), %eax
    pushq   %rax
    movl    -12(%rbp), %eax
    pushq   %rax
    popq    %rdi
    popq    %rsi
    movq    -8(%rbp), %r10
    movl    $0, %eax
    call    *%r10
    jmp     .L2
    movl    $0, %eax
.L2:
    leave
    ret
.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movq    $0, -8(%rbp)
    movl    $0, %eax
    movl    %eax, -12(%rbp)
    leaq    add(%rip), %rax
    pushq   %rax
    leaq    -8(%rbp), %rax
    popq    %rdx
    movq    %rdx, (%rax)
    movq    %rdx, %rax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $3, %eax
    pushq   %rax
    movl    $2, %eax
    pushq   %rax
    popq    %rdi
    popq    %rsi
    movq    -8(%rbp), %r10
    movl    $0, %eax
    call    *%r10
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    leaq    multiply(%rip), %rax
    pushq   %rax
    leaq    -8(%rbp), %rax
    popq    %rdx
    movq    %rdx, (%rax)
    movq    %rdx, %rax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $5, %eax
    pushq   %rax
    movl    $4, %eax
    pushq   %rax
    popq    %rdi
    popq    %rsi
    movq    -8(%rbp), %r10
    movl    $0, %eax
    call    *%r10
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $1, %eax
    pushq   %rax
    movl    $10, %eax
    pushq   %rax
    leaq    add(%rip), %rax
    pushq   %rax
    popq    %rdi
    popq    %rsi
    popq    %rdx
    movl    $0, %eax
    call    apply
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -12(%rbp), %eax
    jmp     .L3
    movl    $0, %eax
.L3:
    leave
    ret
.section .note.GNU-stack,"",@progbits
