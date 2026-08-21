.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    movl    $8, %eax
    push    %eax
    movl    $2, %eax
    pop     %edx
    push    %eax
    movl    %edx, %eax
    pop     %ecx
    cdq
    idivl   %ecx
    negl    %eax
    push    %eax
    movl    $1, %eax
    notl    %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    $12, %eax
    pop     %edx
    addl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
