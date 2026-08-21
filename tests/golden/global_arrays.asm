.data
.globl _table
_table:
    .long   3
    .long   5
    .long   0
    .long   0
.globl _zeros
_zeros:
    .long   0
    .long   0
.text
.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    subl    $16, %esp
    movl    $1, %eax
    movl    %eax, -12(%ebp)
    movl    $2, %eax
    movl    %eax, -8(%ebp)
    movl    $3, %eax
    movl    %eax, -4(%ebp)
    movl    $0, -16(%ebp)
    movl    $_table, %eax
    push    %eax
    leal    -16(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    leal    -12(%ebp), %eax
    push    %eax
    movl    $1, %eax
    imull   $4, %eax
    pop     %edx
    addl    %edx, %eax
    movl    (%eax), %eax
    push    %eax
    movl    $7, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    $_table, %eax
    push    %eax
    movl    $2, %eax
    imull   $4, %eax
    pop     %edx
    addl    %edx, %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    $_table, %eax
    push    %eax
    movl    $0, %eax
    imull   $4, %eax
    pop     %edx
    addl    %edx, %eax
    movl    (%eax), %eax
    push    %eax
    movl    $_table, %eax
    push    %eax
    movl    $1, %eax
    imull   $4, %eax
    pop     %edx
    addl    %edx, %eax
    movl    (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    -16(%ebp), %eax
    push    %eax
    movl    $2, %eax
    pop     %edx
    imull   $4, %eax
    addl    %edx, %eax
    movl    (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    $_table, %eax
    push    %eax
    movl    $3, %eax
    imull   $4, %eax
    pop     %edx
    addl    %edx, %eax
    movl    (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    $_zeros, %eax
    push    %eax
    movl    $1, %eax
    imull   $4, %eax
    pop     %edx
    addl    %edx, %eax
    movl    (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    push    %eax
    movl    $2, %eax
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
