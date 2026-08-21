.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    movl    $1, %eax
    push    %eax
    movl    $2, %eax
    push    %eax
    movl    $3, %eax
    push    %eax
    movl    $4, %eax
    pop     %edx
    addl    %edx, %eax
    pop     %edx
    imull   %edx, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    $0, %eax
    cmpl    $0, %eax
    movl    $0, %eax
    sete    %al
    pop     %edx
    subl    %eax, %edx
    movl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
