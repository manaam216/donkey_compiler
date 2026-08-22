.data
.LC0:
    .byte   72, 105, 0
.text
.globl _first
_first:
    push    %ebp
    movl    %esp, %ebp
    movl    8(%ebp), %eax
    push    %eax
    movl    $0, %eax
    imull   $4, %eax
    pop     %edx
    addl    %edx, %eax
    movl    (%eax), %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    subl    $44, %esp
    movl    $7, %eax
    movl    %eax, -16(%ebp)
    movl    $9, %eax
    movl    %eax, -12(%ebp)
    movl    $11, %eax
    movl    %eax, -8(%ebp)
    movl    $13, %eax
    movl    %eax, -4(%ebp)
    leal    -16(%ebp), %eax
    movl    %eax, -20(%ebp)
    leal    -16(%ebp), %eax
    push    %eax
    movl    $3, %eax
    pop     %edx
    imull   $4, %eax
    addl    %edx, %eax
    movl    %eax, -24(%ebp)
    movl    $65, %eax
    movsbl  %al, %eax
    movl    %eax, -28(%ebp)
    movl    $10, %eax
    movsbl  %al, %eax
    movl    %eax, -32(%ebp)
    movl    $.LC0, %eax
    movl    %eax, -36(%ebp)
    movl    $0, -44(%ebp)
    leal    -16(%ebp), %eax
    push    %eax
    call    _first
    addl    $4, %esp
    push    %eax
    leal    -44(%ebp), %eax
    addl    $4, %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -24(%ebp), %eax
    push    %eax
    movl    -20(%ebp), %eax
    pop     %edx
    subl    %eax, %edx
    movl    %edx, %eax
    cdq
    movl    $4, %ecx
    idivl   %ecx
    push    %eax
    leal    -44(%ebp), %eax
    addl    $0, %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    leal    -44(%ebp), %eax
    addl    $4, %eax
    movl    (%eax), %eax
    push    %eax
    leal    -44(%ebp), %eax
    addl    $0, %eax
    movl    (%eax), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    -28(%ebp), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    -32(%ebp), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    -36(%ebp), %eax
    push    %eax
    movl    $0, %eax
    imull   $1, %eax
    pop     %edx
    addl    %edx, %eax
    movsbl  (%eax), %eax
    movsbl  %al, %eax
    pop     %edx
    addl    %edx, %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
