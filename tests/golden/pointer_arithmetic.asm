.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    subl    $20, %esp
    movl    $0, -16(%ebp)
    movl    $0, -12(%ebp)
    movl    $0, -8(%ebp)
    movl    $0, -4(%ebp)
    movl    $0, -20(%ebp)
    movl    $2, %eax
    push    %eax
    leal    -16(%ebp), %eax
    push    %eax
    movl    $0, %eax
    imull   $4, %eax
    pop     %edx
    addl    %edx, %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    $4, %eax
    push    %eax
    leal    -16(%ebp), %eax
    push    %eax
    movl    $1, %eax
    imull   $4, %eax
    pop     %edx
    addl    %edx, %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    $6, %eax
    push    %eax
    leal    -16(%ebp), %eax
    push    %eax
    movl    $2, %eax
    imull   $4, %eax
    pop     %edx
    addl    %edx, %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    leal    -16(%ebp), %eax
    push    %eax
    leal    -20(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -20(%ebp), %eax
    push    %eax
    movl    $1, %eax
    pop     %edx
    imull   $4, %eax
    addl    %edx, %eax
    movl    (%eax), %eax
    push    %eax
    leal    -16(%ebp), %eax
    push    %eax
    movl    $2, %eax
    imull   $4, %eax
    pop     %edx
    addl    %edx, %eax
    movl    (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    -20(%ebp), %eax
    push    %eax
    movl    $3, %eax
    pop     %edx
    imull   $4, %eax
    addl    %edx, %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -20(%ebp), %eax
    push    %eax
    addl    $4, %eax
    movl    %eax, -20(%ebp)
    pop     %eax
    movl    -20(%ebp), %eax
    movl    (%eax), %eax
    push    %eax
    movl    -20(%ebp), %eax
    push    %eax
    movl    $2, %eax
    pop     %edx
    imull   $4, %eax
    addl    %edx, %eax
    movl    (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
