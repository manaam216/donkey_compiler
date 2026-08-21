.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    movl    $3, %eax
    push    %eax
    call    _add_two
    addl    $4, %esp
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.globl _add_two
_add_two:
    push    %ebp
    movl    %esp, %ebp
    movl    8(%ebp), %eax
    push    %eax
    movl    $2, %eax
    pop     %edx
    addl    %edx, %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
