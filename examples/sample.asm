.globl _main
_main:
    movl    $1, %eax
    push    %eax
    movl    $2, %eax
    push    %eax
    movl    $3, %eax
    push    %eax
    movl    $4, %eax
    pop     %ecx
    addl    %ecx, %eax
    pop     %ecx
    imull   %ecx, %eax
    pop     %ecx
    addl    %ecx, %eax
    push    %eax
    movl    $0, %eax
    cmpl    $0, %eax
    movl    $0, %eax
    sete    %al
    pop     %ecx
    subl    %eax, %ecx
    movl    %ecx, %eax
    ret
