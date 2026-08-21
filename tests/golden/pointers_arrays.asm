.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    subl    $24, %esp
    movl    $3, %eax
    movl    %eax, -4(%ebp)
    movl    $0, -8(%ebp)
    movl    $0, -24(%ebp)
    movl    $0, -20(%ebp)
    movl    $0, -16(%ebp)
    movl    $0, -12(%ebp)
    leal    -4(%ebp), %eax
    push    %eax
    leal    -8(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -8(%ebp), %eax
    movl    (%eax), %eax
    push    %eax
    movl    $4, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    -8(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -4(%ebp), %eax
    push    %eax
    leal    -24(%ebp), %eax
    push    %eax
    movl    $0, %eax
    imull   $4, %eax
    pop     %edx
    addl    %edx, %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -8(%ebp), %eax
    movl    (%eax), %eax
    push    %eax
    movl    $5, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -24(%ebp), %eax
    push    %eax
    movl    $1, %eax
    imull   $4, %eax
    pop     %edx
    addl    %edx, %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    leal    -24(%ebp), %eax
    push    %eax
    movl    $0, %eax
    imull   $4, %eax
    pop     %edx
    addl    %edx, %eax
    movl    (%eax), %eax
    push    %eax
    leal    -24(%ebp), %eax
    push    %eax
    movl    $1, %eax
    imull   $4, %eax
    pop     %edx
    addl    %edx, %eax
    movl    (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
