.globl _helper
_helper:
    push    %ebp
    movl    %esp, %ebp
    subl    $4, %esp
    movl    $2, %eax
    movl    %eax, -4(%ebp)
    movl    8(%ebp), %eax
    push    %eax
    movl    12(%ebp), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    -4(%ebp), %eax
    pop     %edx
    addl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.globl _unused
_unused:
    push    %ebp
    movl    %esp, %ebp
    movl    $99, %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    subl    $4, %esp
    movl    $7, %eax
    movl    %eax, -4(%ebp)
    movl    $4, %eax
    push    %eax
    movl    -4(%ebp), %eax
    push    %eax
    call    _helper
    addl    $8, %esp
    push    %eax
    movl    $3, %eax
    pop     %edx
    addl    %edx, %eax
    jmp     .L2
    movl    $0, %eax
.L2:
    leave
    ret
