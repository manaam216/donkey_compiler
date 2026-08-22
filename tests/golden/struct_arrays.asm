.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    subl    $36, %esp
    movl    $0, -24(%ebp)
    movl    $0, -16(%ebp)
    movl    $0, -8(%ebp)
    movw    $0, -32(%ebp)
    movw    $0, -30(%ebp)
    movw    $0, -28(%ebp)
    movw    $0, -26(%ebp)
    movl    $0, -36(%ebp)
    movl    $1, %eax
    push    %eax
    leal    -24(%ebp), %eax
    push    %eax
    movl    $0, %eax
    imull   $8, %eax
    pop     %edx
    addl    %edx, %eax
    addl    $4, %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    $2, %eax
    push    %eax
    leal    -24(%ebp), %eax
    push    %eax
    movl    $0, %eax
    imull   $8, %eax
    pop     %edx
    addl    %edx, %eax
    addl    $0, %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    $10, %eax
    push    %eax
    leal    -24(%ebp), %eax
    push    %eax
    movl    $1, %eax
    imull   $8, %eax
    pop     %edx
    addl    %edx, %eax
    addl    $4, %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    $20, %eax
    push    %eax
    leal    -24(%ebp), %eax
    push    %eax
    movl    $1, %eax
    imull   $8, %eax
    pop     %edx
    addl    %edx, %eax
    addl    $0, %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    $100, %eax
    push    %eax
    leal    -24(%ebp), %eax
    push    %eax
    movl    $2, %eax
    imull   $8, %eax
    pop     %edx
    addl    %edx, %eax
    addl    $4, %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    $200, %eax
    push    %eax
    leal    -24(%ebp), %eax
    push    %eax
    movl    $2, %eax
    imull   $8, %eax
    pop     %edx
    addl    %edx, %eax
    addl    $0, %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    $300, %eax
    movswl  %ax, %eax
    push    %eax
    leal    -32(%ebp), %eax
    push    %eax
    movl    $0, %eax
    imull   $2, %eax
    pop     %edx
    addl    %edx, %eax
    pop     %edx
    movw    %dx, (%eax)
    movl    %edx, %eax
    movl    $400, %eax
    movswl  %ax, %eax
    push    %eax
    leal    -32(%ebp), %eax
    push    %eax
    movl    $1, %eax
    imull   $2, %eax
    pop     %edx
    addl    %edx, %eax
    pop     %edx
    movw    %dx, (%eax)
    movl    %edx, %eax
    movl    $500, %eax
    movswl  %ax, %eax
    push    %eax
    leal    -32(%ebp), %eax
    push    %eax
    movl    $2, %eax
    imull   $2, %eax
    pop     %edx
    addl    %edx, %eax
    pop     %edx
    movw    %dx, (%eax)
    movl    %edx, %eax
    movl    $600, %eax
    movswl  %ax, %eax
    push    %eax
    leal    -32(%ebp), %eax
    push    %eax
    movl    $3, %eax
    imull   $2, %eax
    pop     %edx
    addl    %edx, %eax
    pop     %edx
    movw    %dx, (%eax)
    movl    %edx, %eax
    leal    -24(%ebp), %eax
    push    %eax
    movl    $0, %eax
    imull   $8, %eax
    pop     %edx
    addl    %edx, %eax
    addl    $4, %eax
    movl    (%eax), %eax
    push    %eax
    leal    -24(%ebp), %eax
    push    %eax
    movl    $0, %eax
    imull   $8, %eax
    pop     %edx
    addl    %edx, %eax
    addl    $0, %eax
    movl    (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -24(%ebp), %eax
    push    %eax
    movl    $1, %eax
    imull   $8, %eax
    pop     %edx
    addl    %edx, %eax
    addl    $4, %eax
    movl    (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -24(%ebp), %eax
    push    %eax
    movl    $1, %eax
    imull   $8, %eax
    pop     %edx
    addl    %edx, %eax
    addl    $0, %eax
    movl    (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -24(%ebp), %eax
    push    %eax
    movl    $2, %eax
    imull   $8, %eax
    pop     %edx
    addl    %edx, %eax
    addl    $4, %eax
    movl    (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -24(%ebp), %eax
    push    %eax
    movl    $2, %eax
    imull   $8, %eax
    pop     %edx
    addl    %edx, %eax
    addl    $0, %eax
    movl    (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -36(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -36(%ebp), %eax
    push    %eax
    leal    -32(%ebp), %eax
    push    %eax
    movl    $0, %eax
    imull   $2, %eax
    pop     %edx
    addl    %edx, %eax
    movswl  (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -32(%ebp), %eax
    push    %eax
    movl    $1, %eax
    imull   $2, %eax
    pop     %edx
    addl    %edx, %eax
    movswl  (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -32(%ebp), %eax
    push    %eax
    movl    $2, %eax
    imull   $2, %eax
    pop     %edx
    addl    %edx, %eax
    movswl  (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -32(%ebp), %eax
    push    %eax
    movl    $3, %eax
    imull   $2, %eax
    pop     %edx
    addl    %edx, %eax
    movswl  (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -36(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -36(%ebp), %eax
    push    %eax
    movl    $24, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    $8, %eax
    pop     %edx
    addl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
