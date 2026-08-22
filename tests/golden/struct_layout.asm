.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    subl    $20, %esp
    movl    $0, -12(%ebp)
    movl    $0, -16(%ebp)
    movl    $0, -20(%ebp)
    movl    $3, %eax
    movsbl  %al, %eax
    push    %eax
    leal    -12(%ebp), %eax
    addl    $8, %eax
    pop     %edx
    movb    %dl, (%eax)
    movl    %edx, %eax
    movl    $1000, %eax
    push    %eax
    leal    -12(%ebp), %eax
    addl    $4, %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    $7, %eax
    movsbl  %al, %eax
    push    %eax
    leal    -12(%ebp), %eax
    addl    $0, %eax
    pop     %edx
    movb    %dl, (%eax)
    movl    %edx, %eax
    movl    $1, %eax
    movsbl  %al, %eax
    push    %eax
    leal    -16(%ebp), %eax
    addl    $2, %eax
    pop     %edx
    movb    %dl, (%eax)
    movl    %edx, %eax
    movl    $2, %eax
    movsbl  %al, %eax
    push    %eax
    leal    -16(%ebp), %eax
    addl    $1, %eax
    pop     %edx
    movb    %dl, (%eax)
    movl    %edx, %eax
    movl    $3, %eax
    movsbl  %al, %eax
    push    %eax
    leal    -16(%ebp), %eax
    addl    $0, %eax
    pop     %edx
    movb    %dl, (%eax)
    movl    %edx, %eax
    leal    -12(%ebp), %eax
    addl    $8, %eax
    movsbl  (%eax), %eax
    push    %eax
    leal    -12(%ebp), %eax
    addl    $4, %eax
    movl    (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    addl    $0, %eax
    movsbl  (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -20(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -20(%ebp), %eax
    push    %eax
    leal    -16(%ebp), %eax
    addl    $2, %eax
    movsbl  (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -16(%ebp), %eax
    addl    $1, %eax
    movsbl  (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -16(%ebp), %eax
    addl    $0, %eax
    movsbl  (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -20(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -20(%ebp), %eax
    push    %eax
    movl    $12, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    $3, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -20(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -20(%ebp), %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
