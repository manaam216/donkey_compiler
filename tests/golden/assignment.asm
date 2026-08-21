.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    subl    $4, %esp
    movl    $3, %eax
    movl    %eax, -4(%ebp)
    movl    -4(%ebp), %eax
    push    %eax
    movl    $2, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -4(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -4(%ebp), %eax
    push    %eax
    movl    $3, %eax
    pop     %edx
    imull   %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
