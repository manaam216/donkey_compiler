.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $96, %rsp
    movl    $1, %eax
    movl    %eax, -12(%rbp)
    movl    $2, %eax
    movl    %eax, -8(%rbp)
    movl    $3, %eax
    movl    %eax, -4(%rbp)
    movl    $0, -36(%rbp)
    movl    $0, -28(%rbp)
    movl    $0, -24(%rbp)
    movl    $0, -16(%rbp)
    movl    $10, %eax
    movl    %eax, -32(%rbp)
    movl    $40, %eax
    movl    %eax, -20(%rbp)
    movl    $0, -48(%rbp)
    movl    $0, -40(%rbp)
    movl    $7, %eax
    movl    %eax, -52(%rbp)
    movl    $9, %eax
    movl    %eax, -44(%rbp)
    movl    $0, -64(%rbp)
    movl    $0, -60(%rbp)
    movl    $0, -56(%rbp)
    movl    $1, %eax
    movl    %eax, -64(%rbp)
    movl    $2, %eax
    movl    %eax, -60(%rbp)
    movl    $3, %eax
    movl    %eax, -56(%rbp)
    movl    $0, -76(%rbp)
    movl    $0, -72(%rbp)
    movl    $0, -68(%rbp)
    movl    $30, %eax
    movl    %eax, -68(%rbp)
    movl    $10, %eax
    movl    %eax, -76(%rbp)
    movl    $0, -88(%rbp)
    movl    $0, -84(%rbp)
    movl    $0, -80(%rbp)
    movl    $5, %eax
    movl    %eax, -88(%rbp)
    movl    $0, %eax
    movl    %eax, -92(%rbp)
    movl    $0, -96(%rbp)
    movl    $0, %eax
    pushq   %rax
    leaq    -96(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
.L1:
    movl    -96(%rbp), %eax
    pushq   %rax
    movl    $3, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    setl    %al
    cmpl    $0, %eax
    je      .L3
    movl    -92(%rbp), %eax
    pushq   %rax
    leaq    -12(%rbp), %rax
    pushq   %rax
    movl    -96(%rbp), %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -92(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
.L2:
    movl    -96(%rbp), %eax
    pushq   %rax
    movl    $1, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -96(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    jmp     .L1
.L3:
    movl    $0, %eax
    pushq   %rax
    leaq    -96(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
.L4:
    movl    -96(%rbp), %eax
    pushq   %rax
    movl    $6, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    setl    %al
    cmpl    $0, %eax
    je      .L6
    movl    -92(%rbp), %eax
    pushq   %rax
    leaq    -36(%rbp), %rax
    pushq   %rax
    movl    -96(%rbp), %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -92(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
.L5:
    movl    -96(%rbp), %eax
    pushq   %rax
    movl    $1, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -96(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    jmp     .L4
.L6:
    movl    $0, %eax
    pushq   %rax
    leaq    -96(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
.L7:
    movl    -96(%rbp), %eax
    pushq   %rax
    movl    $4, %eax
    popq    %rdx
    cmpl    %eax, %edx
    movl    $0, %eax
    setl    %al
    cmpl    $0, %eax
    je      .L9
    movl    -92(%rbp), %eax
    pushq   %rax
    leaq    -52(%rbp), %rax
    pushq   %rax
    movl    -96(%rbp), %eax
    cltq
    imulq   $4, %rax
    popq    %rdx
    addq    %rdx, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -92(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
.L8:
    movl    -96(%rbp), %eax
    pushq   %rax
    movl    $1, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -96(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    jmp     .L7
.L9:
    movl    -92(%rbp), %eax
    pushq   %rax
    leaq    -64(%rbp), %rax
    addq    $0, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -64(%rbp), %rax
    addq    $4, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -64(%rbp), %rax
    addq    $8, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -92(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -92(%rbp), %eax
    pushq   %rax
    leaq    -76(%rbp), %rax
    addq    $0, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -76(%rbp), %rax
    addq    $4, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -76(%rbp), %rax
    addq    $8, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -92(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -92(%rbp), %eax
    pushq   %rax
    leaq    -88(%rbp), %rax
    addq    $0, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -88(%rbp), %rax
    addq    $4, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -88(%rbp), %rax
    addq    $8, %rax
    movl    (%rax), %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -92(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -92(%rbp), %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.section .note.GNU-stack,"",@progbits
