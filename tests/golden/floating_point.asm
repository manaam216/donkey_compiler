.globl half
half:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movsd   %xmm0, -8(%rbp)
    movsd   -8(%rbp), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   .LF0(%rip), %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    divsd   %xmm0, %xmm1
    movsd   %xmm1, %xmm0
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.globl weighted
weighted:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $32, %rsp
    movsd   %xmm0, -8(%rbp)
    movsd   %xmm1, -16(%rbp)
    movsd   %xmm2, -24(%rbp)
    movsd   -8(%rbp), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   -24(%rbp), %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    mulsd   %xmm1, %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   -16(%rbp), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   .LF1(%rip), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   -24(%rbp), %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    subsd   %xmm0, %xmm1
    movsd   %xmm1, %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    mulsd   %xmm1, %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    addsd   %xmm1, %xmm0
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
.globl mixed
mixed:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $32, %rsp
    movl    %edi, -4(%rbp)
    movsd   %xmm0, -16(%rbp)
    movl    %esi, -20(%rbp)
    movsd   %xmm1, -32(%rbp)
    movsd   -16(%rbp), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movl    -4(%rbp), %eax
    cvtsi2sd %eax, %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    mulsd   %xmm1, %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   -32(%rbp), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movl    -20(%rbp), %eax
    cvtsi2sd %eax, %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    mulsd   %xmm1, %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    addsd   %xmm1, %xmm0
    jmp     .L2
    movl    $0, %eax
.L2:
    leave
    ret
.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $32, %rsp
    movsd   .LF2(%rip), %xmm0
    movsd   %xmm0, -8(%rbp)
    movsd   .LF3(%rip), %xmm0
    movsd   %xmm0, -16(%rbp)
    movss   .LF4(%rip), %xmm0
    movss   %xmm0, -20(%rbp)
    movl    $7, %eax
    movl    %eax, -24(%rbp)
    movl    $0, %eax
    movl    %eax, -28(%rbp)
    movl    -28(%rbp), %eax
    pushq   %rax
    movsd   -8(%rbp), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   -16(%rbp), %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    addsd   %xmm1, %xmm0
    cvttsd2si %xmm0, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -28(%rbp), %eax
    pushq   %rax
    movsd   -8(%rbp), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   -16(%rbp), %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    subsd   %xmm0, %xmm1
    movsd   %xmm1, %xmm0
    cvttsd2si %xmm0, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -28(%rbp), %eax
    pushq   %rax
    movsd   -8(%rbp), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   -16(%rbp), %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    mulsd   %xmm1, %xmm0
    cvttsd2si %xmm0, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -28(%rbp), %eax
    pushq   %rax
    movsd   -8(%rbp), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   -16(%rbp), %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    divsd   %xmm0, %xmm1
    movsd   %xmm1, %xmm0
    cvttsd2si %xmm0, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -28(%rbp), %eax
    pushq   %rax
    movss   -20(%rbp), %xmm0
    cvtss2sd %xmm0, %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   .LF0(%rip), %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    mulsd   %xmm1, %xmm0
    cvttsd2si %xmm0, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -28(%rbp), %eax
    pushq   %rax
    movsd   -8(%rbp), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movl    -24(%rbp), %eax
    cvtsi2sd %eax, %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    addsd   %xmm1, %xmm0
    cvttsd2si %xmm0, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -28(%rbp), %eax
    pushq   %rax
    movsd   -8(%rbp), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   -16(%rbp), %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    ucomisd %xmm0, %xmm1
    movl    $0, %eax
    seta   %al
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -28(%rbp), %eax
    pushq   %rax
    movsd   -8(%rbp), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   -16(%rbp), %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    ucomisd %xmm1, %xmm0
    movl    $0, %eax
    seta   %al
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -28(%rbp), %eax
    pushq   %rax
    movsd   -8(%rbp), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   .LF2(%rip), %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    ucomisd %xmm1, %xmm0
    movl    $0, %eax
    sete   %al
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -28(%rbp), %eax
    pushq   %rax
    movsd   -8(%rbp), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   .LF2(%rip), %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    ucomisd %xmm0, %xmm1
    movl    $0, %eax
    setae   %al
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -28(%rbp), %eax
    pushq   %rax
    movsd   .LF5(%rip), %xmm0
    cvttsd2si %xmm0, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -28(%rbp), %eax
    pushq   %rax
    movsd   .LF6(%rip), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   (%rsp), %xmm0
    addq    $8, %rsp
    movl    $1, %eax
    call    half
    cvttsd2si %xmm0, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -28(%rbp), %eax
    pushq   %rax
    movsd   .LF7(%rip), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   .LF8(%rip), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   .LF2(%rip), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   (%rsp), %xmm0
    addq    $8, %rsp
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    movsd   (%rsp), %xmm2
    addq    $8, %rsp
    movl    $3, %eax
    call    weighted
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movsd   .LF2(%rip), %xmm0
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    mulsd   %xmm1, %xmm0
    cvttsd2si %xmm0, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -28(%rbp), %eax
    pushq   %rax
    movsd   .LF9(%rip), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movl    $4, %eax
    pushq   %rax
    movsd   .LF0(%rip), %xmm0
    subq    $8, %rsp
    movsd   %xmm0, (%rsp)
    movl    $3, %eax
    pushq   %rax
    popq    %rdi
    movsd   (%rsp), %xmm0
    addq    $8, %rsp
    popq    %rsi
    movsd   (%rsp), %xmm1
    addq    $8, %rsp
    movl    $2, %eax
    call    mixed
    cvttsd2si %xmm0, %eax
    popq    %rdx
    addl    %edx, %eax
    pushq   %rax
    leaq    -28(%rbp), %rax
    popq    %rdx
    movl    %edx, (%rax)
    movl    %edx, %eax
    movl    -28(%rbp), %eax
    jmp     .L3
    movl    $0, %eax
.L3:
    leave
    ret
.data
    .align  8
.LF0:
    .double   2.0
    .align  8
.LF1:
    .double   1.0
    .align  8
.LF2:
    .double   10.0
    .align  8
.LF3:
    .double   4.0
    .align  4
.LF4:
    .float    2.5
    .align  8
.LF5:
    .double   3.99
    .align  8
.LF6:
    .double   9.0
    .align  8
.LF7:
    .double   0.25
    .align  8
.LF8:
    .double   20.0
    .align  8
.LF9:
    .double   1.5
.text
.section .note.GNU-stack,"",@progbits
