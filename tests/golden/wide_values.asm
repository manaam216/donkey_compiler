.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    subl    $8, %esp
    movl    $0, -4(%ebp)
    movl    $0, -8(%ebp)
    movl    $100000, %eax
    push    %eax
    leal    -4(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    $0, %eax
    push    %eax
    movl    $42, %eax
    pop     %edx
    subl    %eax, %edx
    movl    %edx, %eax
    push    %eax
    leal    -8(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -4(%ebp), %eax
    push    %eax
    movl    -8(%ebp), %eax
    pop     %edx
    addl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
