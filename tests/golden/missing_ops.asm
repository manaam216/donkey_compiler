.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    subl    $12, %esp
    movl    $2, %eax
    movl    %eax, -4(%ebp)
    movl    $5, %eax
    movl    %eax, -8(%ebp)
    movl    $0, %eax
    movl    %eax, -12(%ebp)
    movl    -12(%ebp), %eax
    push    %eax
    movl    -4(%ebp), %eax
    push    %eax
    movl    $3, %eax
    pop     %edx
    movl    %eax, %ecx
    movl    %edx, %eax
    sall    %cl, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -12(%ebp), %eax
    push    %eax
    movl    -8(%ebp), %eax
    push    %eax
    movl    $1, %eax
    pop     %edx
    movl    %eax, %ecx
    movl    %edx, %eax
    sarl    %cl, %eax
    pop     %edx
    subl    %eax, %edx
    movl    %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -12(%ebp), %eax
    push    %eax
    movl    $2, %eax
    pop     %edx
    imull   %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -12(%ebp), %eax
    push    %eax
    movl    $7, %eax
    pop     %edx
    push    %eax
    movl    %edx, %eax
    pop     %ecx
    cdq
    idivl   %ecx
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -12(%ebp), %eax
    push    %eax
    movl    $5, %eax
    pop     %edx
    push    %eax
    movl    %edx, %eax
    pop     %ecx
    cdq
    idivl   %ecx
    movl    %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -12(%ebp), %eax
    push    %eax
    movl    $8, %eax
    pop     %edx
    orl     %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -12(%ebp), %eax
    push    %eax
    movl    $15, %eax
    pop     %edx
    andl    %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -12(%ebp), %eax
    push    %eax
    movl    $3, %eax
    pop     %edx
    xorl    %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -12(%ebp), %eax
    push    %eax
    movl    $4, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -12(%ebp), %eax
    push    %eax
    movl    $4, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -12(%ebp), %eax
    push    %eax
    movl    $2, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -12(%ebp), %eax
    push    %eax
    movl    -4(%ebp), %eax
    push    %eax
    addl    $1, %eax
    movl    %eax, -4(%ebp)
    pop     %eax
    push    %eax
    movl    -4(%ebp), %eax
    addl    $1, %eax
    movl    %eax, -4(%ebp)
    pop     %edx
    addl    %edx, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -12(%ebp), %eax
    push    %eax
    movl    -8(%ebp), %eax
    push    %eax
    subl    $1, %eax
    movl    %eax, -8(%ebp)
    pop     %eax
    push    %eax
    movl    -8(%ebp), %eax
    subl    $1, %eax
    movl    %eax, -8(%ebp)
    pop     %edx
    addl    %edx, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -12(%ebp), %eax
    push    %eax
    movl    -4(%ebp), %eax
    push    %eax
    movl    -8(%ebp), %eax
    pop     %edx
    cmpl    %eax, %edx
    movl    $0, %eax
    setg    %al
    cmpl    $0, %eax
    je      .L1
    movl    $10, %eax
    jmp     .L2
.L1:
    movl    $20, %eax
.L2:
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -12(%ebp), %eax
    push    %eax
    movl    $1, %eax
    push    %eax
    leal    -4(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    $2, %eax
    push    %eax
    leal    -8(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -4(%ebp), %eax
    push    %eax
    movl    -8(%ebp), %eax
    pop     %edx
    addl    %edx, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -12(%ebp), %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
