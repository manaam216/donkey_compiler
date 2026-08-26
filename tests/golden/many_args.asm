.globl nine
nine:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $32, %rsp
    movl    %edi, -4(%rbp)
    movl    %esi, -8(%rbp)
    movl    %edx, -12(%rbp)
    movl    %ecx, -16(%rbp)
    movl    %r8d, -20(%rbp)
    movl    %r9d, -24(%rbp)
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    -8(%rbp), %eax
    pushq   %rax
    movl    $2, %eax
    popq    %rdx
    imull   %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $3, %eax
    popq    %rdx
    imull   %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -16(%rbp), %eax
    pushq   %rax
    movl    $4, %eax
    popq    %rdx
    imull   %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -20(%rbp), %eax
    pushq   %rax
    movl    $5, %eax
    popq    %rdx
    imull   %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -24(%rbp), %eax
    pushq   %rax
    movl    $6, %eax
    popq    %rdx
    imull   %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    16(%rbp), %eax
    pushq   %rax
    movl    $7, %eax
    popq    %rdx
    imull   %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    24(%rbp), %eax
    pushq   %rax
    movl    $8, %eax
    popq    %rdx
    imull   %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    32(%rbp), %eax
    pushq   %rax
    movl    $9, %eax
    popq    %rdx
    imull   %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.globl six
six:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $32, %rsp
    movl    %edi, -4(%rbp)
    movl    %esi, -8(%rbp)
    movl    %edx, -12(%rbp)
    movl    %ecx, -16(%rbp)
    movl    %r8d, -20(%rbp)
    movl    %r9d, -24(%rbp)
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    -8(%rbp), %eax
    pushq   %rax
    movl    $2, %eax
    popq    %rdx
    imull   %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $3, %eax
    popq    %rdx
    imull   %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -16(%rbp), %eax
    pushq   %rax
    movl    $4, %eax
    popq    %rdx
    imull   %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -20(%rbp), %eax
    pushq   %rax
    movl    $5, %eax
    popq    %rdx
    imull   %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -24(%rbp), %eax
    pushq   %rax
    movl    $6, %eax
    popq    %rdx
    imull   %edx, %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $8, %rsp
    movl    $9, %eax
    pushq   %rax
    movl    $8, %eax
    pushq   %rax
    movl    $7, %eax
    pushq   %rax
    movl    $6, %eax
    pushq   %rax
    movl    $5, %eax
    pushq   %rax
    movl    $4, %eax
    pushq   %rax
    movl    $3, %eax
    pushq   %rax
    movl    $2, %eax
    pushq   %rax
    movl    $1, %eax
    pushq   %rax
    popq    %rdi
    popq    %rsi
    popq    %rdx
    popq    %rcx
    popq    %r8
    popq    %r9
    movl    $0, %eax
    call    nine
    addq    $32, %rsp
    pushq   %rax
    movl    $6, %eax
    pushq   %rax
    movl    $5, %eax
    pushq   %rax
    movl    $4, %eax
    pushq   %rax
    movl    $3, %eax
    pushq   %rax
    movl    $2, %eax
    pushq   %rax
    movl    $1, %eax
    pushq   %rax
    popq    %rdi
    popq    %rsi
    popq    %rdx
    popq    %rcx
    popq    %r8
    popq    %r9
    movl    $0, %eax
    call    six
    popq    %rdx
    addl    %edx, %eax
    jmp     .L2
    movl    $0, %eax
.L2:
    leave
    ret
.section .note.GNU-stack,"",@progbits
