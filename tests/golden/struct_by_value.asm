.globl sum
sum:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movq    %rdi, -8(%rbp)
    leaq    -8(%rbp), %rax
    addq    $0, %rax
    movl    (%rax), %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    addq    $4, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.globl scaled
scaled:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movq    %rdi, -8(%rbp)
    movl    %esi, -12(%rbp)
    leaq    -8(%rbp), %rax
    addq    $0, %rax
    movl    (%rax), %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    addq    $4, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    -12(%rbp), %eax
    popq    %rdx
    imull   %edx, %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
.globl just_one
just_one:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    %edi, -4(%rbp)
    leaq    -4(%rbp), %rax
    addq    $0, %rax
    movl    (%rax), %eax
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
    movl    $0, -12(%rbp)
    movl    $3, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    addq    $0, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $4, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    addq    $4, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $100, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    addq    $0, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    leaq    -8(%rbp), %rax
    movq    0(%rax), %rdx
    pushq   %rdx
    popq    %rdi
    movl    $0, %eax
    call    sum
    pushq   %rax
    movl    $2, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    movq    0(%rax), %rdx
    pushq   %rdx
    popq    %rdi
    popq    %rsi
    movl    $0, %eax
    call    scaled
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    movq    0(%rax), %rdx
    pushq   %rdx
    popq    %rdi
    movl    $0, %eax
    call    just_one
    popq    %rdx
    addl    %edx, %eax
    jmp     .L3
    movl    $0, %eax
.L3:
    leave
    ret
.section .note.GNU-stack,"",@progbits
