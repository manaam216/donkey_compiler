.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    subl    $12, %esp
    movb    $0, -4(%ebp)
    movb    $0, -3(%ebp)
    movb    $0, -2(%ebp)
    movb    $0, -1(%ebp)
    movl    $0, -8(%ebp)
    movl    $0, -12(%ebp)
    movl    $65, %eax
    movsbl  %al, %eax
    push    %eax
    leal    -4(%ebp), %eax
    push    %eax
    movl    $0, %eax
    imull   $1, %eax
    pop     %edx
    addl    %edx, %eax
    pop     %edx
    movb    %dl, (%eax)
    movl    %edx, %eax
    movl    $66, %eax
    movsbl  %al, %eax
    push    %eax
    leal    -4(%ebp), %eax
    push    %eax
    movl    $1, %eax
    imull   $1, %eax
    pop     %edx
    addl    %edx, %eax
    pop     %edx
    movb    %dl, (%eax)
    movl    %edx, %eax
    movl    $67, %eax
    movsbl  %al, %eax
    push    %eax
    leal    -4(%ebp), %eax
    push    %eax
    movl    $2, %eax
    imull   $1, %eax
    pop     %edx
    addl    %edx, %eax
    pop     %edx
    movb    %dl, (%eax)
    movl    %edx, %eax
    movl    $68, %eax
    movsbl  %al, %eax
    push    %eax
    leal    -4(%ebp), %eax
    push    %eax
    movl    $3, %eax
    imull   $1, %eax
    pop     %edx
    addl    %edx, %eax
    pop     %edx
    movb    %dl, (%eax)
    movl    %edx, %eax
    leal    -4(%ebp), %eax
    push    %eax
    movl    $2, %eax
    imull   $1, %eax
    pop     %edx
    addl    %edx, %eax
    movsbl  (%eax), %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -12(%ebp), %eax
    push    %eax
    leal    -4(%ebp), %eax
    push    %eax
    movl    $0, %eax
    imull   $1, %eax
    pop     %edx
    addl    %edx, %eax
    movsbl  (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -4(%ebp), %eax
    push    %eax
    movl    $1, %eax
    imull   $1, %eax
    pop     %edx
    addl    %edx, %eax
    movsbl  (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -4(%ebp), %eax
    push    %eax
    movl    $2, %eax
    imull   $1, %eax
    pop     %edx
    addl    %edx, %eax
    movsbl  (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -4(%ebp), %eax
    push    %eax
    movl    $3, %eax
    imull   $1, %eax
    pop     %edx
    addl    %edx, %eax
    movsbl  (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    leal    -4(%ebp), %eax
    push    %eax
    leal    -8(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -8(%ebp), %eax
    push    %eax
    movl    $2, %eax
    pop     %edx
    imull   $1, %eax
    addl    %edx, %eax
    push    %eax
    leal    -8(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -12(%ebp), %eax
    push    %eax
    movl    -8(%ebp), %eax
    movsbl  (%eax), %eax
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
