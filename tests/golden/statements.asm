.globl nothing
nothing:
    pushq   %rbp
    movq    %rsp, %rbp
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.globl classify
classify:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    %edi, -4(%rbp)
    movl    $0, -8(%rbp)
    movl    -4(%rbp), %eax
    cmpl    $1, %eax
    je      .L3
    cmpl    $2, %eax
    je      .L4
    cmpl    $3, %eax
    je      .L5
    jmp     .L6
.L3:
    movl    $10, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    jmp     .L2
.L4:
.L5:
    movl    $20, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    jmp     .L2
.L6:
    movl    $30, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    jmp     .L2
.L2:
    movl    -8(%rbp), %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
.globl count_down
count_down:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    %edi, -4(%rbp)
    movl    $0, %eax
    movl    %eax, -8(%rbp)
.L8:
    movl    -8(%rbp), %eax
    pushq   %rax
    movl    -4(%rbp), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -8(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $1, %eax
    popq    %rdx
    subl    %eax, %edx
    movl    %edx, %eax
    pushq   %rax
    leaq    -4(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
.L9:
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $0, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    setg    %al
    cmpl    $0, %eax
    jne     .L8
.L10:
    movl    -8(%rbp), %eax
    jmp     .L7
    movl    $0, %eax
.L7:
    leave
    ret
.globl find_first_negative
find_first_negative:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movq    %rdi, -8(%rbp)
    movl    $0, %eax
    movl    %eax, -12(%rbp)
.L13:
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $4, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    setl    %al
    cmpl    $0, %eax
    je      .L14
    movq    -8(%rbp), %rax
    pushq   %rax
    movl    -12(%rbp), %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    movl    (%rax), %eax
    pushq   %rax
    movl    $0, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    setl    %al
    cmpl    $0, %eax
    je      .L15
    jmp     .L12
    jmp     .L16
.L15:
.L16:
    movl    -12(%rbp), %eax
    pushq   %rax
    movl    $1, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    jmp     .L13
.L14:
    movl    $1, %eax
    negl    %eax
    jmp     .L11
.L12:
    movl    -12(%rbp), %eax
    jmp     .L11
    movl    $0, %eax
.L11:
    leave
    ret
.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $32, %rsp
    movl    $0, -16(%rbp)
    movl    $0, -12(%rbp)
    movl    $0, -8(%rbp)
    movl    $0, -4(%rbp)
    movl    $0, %eax
    movl    %eax, -20(%rbp)
    movl    $0, %eax
    call    nothing
    movl    $5, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    pushq   %rax
    movl    $0, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $6, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    pushq   %rax
    movl    $1, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $0, %eax
    pushq   %rax
    movl    $7, %eax
    popq    %rdx
    subl    %eax, %edx
    movl    %edx, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    pushq   %rax
    movl    $2, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $8, %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    pushq   %rax
    movl    $3, %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    $1, %eax
    pushq   %rax
    popq    %rdi
    movl    $0, %eax
    call    classify
    pushq   %rax
    movl    $3, %eax
    pushq   %rax
    popq    %rdi
    movl    $0, %eax
    call    classify
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    movl    $9, %eax
    pushq   %rax
    popq    %rdi
    movl    $0, %eax
    call    classify
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -20(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -20(%rbp), %eax
    pushq   %rax
    movl    $4, %eax
    pushq   %rax
    popq    %rdi
    movl    $0, %eax
    call    count_down
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -20(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -20(%rbp), %eax
    pushq   %rax
    leaq    -16(%rbp), %rax
    pushq   %rax
    popq    %rdi
    movl    $0, %eax
    call    find_first_negative
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -20(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -20(%rbp), %eax
    jmp     .L17
    movl    $0, %eax
.L17:
    leave
    ret
.section .note.GNU-stack,"",@progbits
