.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    subl    $12, %esp
    movl    $260, %eax
    movl    %eax, -4(%ebp)
    movl    $65535, %eax
    movl    %eax, -8(%ebp)
    movl    $0, %eax
    movl    %eax, -12(%ebp)
    movl    -12(%ebp), %eax
    push    %eax
    movl    -4(%ebp), %eax
    movsbl  %al, %eax
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
    movsbl  %al, %eax
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
    movzbl  %al, %eax
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
    movswl  %ax, %eax
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
    movswl  %ax, %eax
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
    movzwl  %ax, %eax
    push    %eax
    movl    $7, %eax
    pop     %edx
    push    %eax
    movl    %edx, %eax
    pop     %ecx
    cdq
    idivl   %ecx
    movl    %edx, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -12(%ebp), %eax
    push    %eax
    movl    $3, %eax
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
    movl    $1, %eax
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
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
