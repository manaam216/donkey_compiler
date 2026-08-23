.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    subl    $12, %esp
    movl    $0, -4(%ebp)
    movl    $0, -8(%ebp)
    movl    $1, %eax
    push    %eax
    leal    -4(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    $0, %eax
    push    %eax
    leal    -8(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    $0, -12(%ebp)
    movl    $20, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -8(%ebp), %eax
    push    %eax
    movl    -12(%ebp), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -8(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -8(%ebp), %eax
    push    %eax
    movl    -4(%ebp), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -8(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    $0, -12(%ebp)
    movl    $300, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -8(%ebp), %eax
    push    %eax
    movl    -12(%ebp), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -8(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -8(%ebp), %eax
    push    %eax
    movl    -4(%ebp), %eax
    pop     %edx
    addl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
