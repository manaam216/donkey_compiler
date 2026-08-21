.data
.globl _shared
_shared:
    .long   5
.globl _zero
_zero:
    .long   0
.text
.globl _helper
_helper:
    push    %ebp
    movl    %esp, %ebp
    movl    _shared, %eax
    push    %eax
    movl    8(%ebp), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    $_shared, %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    _shared, %eax
    push    %eax
    movl    $1, %eax
    pop     %edx
    subl    %eax, %edx
    movl    %edx, %eax
    push    %eax
    movl    $_zero, %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    _zero, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    subl    $8, %esp
    movl    $2, %eax
    movl    %eax, -4(%ebp)
    movl    $4, %eax
    push    %eax
    call    _helper
    addl    $4, %esp
    movl    %eax, -8(%ebp)
    movl    -4(%ebp), %eax
    push    %eax
    movl    $3, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -4(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -8(%ebp), %eax
    push    %eax
    movl    -4(%ebp), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    _zero, %eax
    pop     %edx
    addl    %edx, %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
