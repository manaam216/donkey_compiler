.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    subl    $4, %esp
    movl    $1, %eax
    movl    %eax, -4(%ebp)
    movl    -4(%ebp), %eax
    cmpl    $0, %eax
    jne     .L1
    movl    $10, %eax
    push    %eax
    movl    $0, %eax
    pop     %edx
    push    %eax
    movl    %edx, %eax
    pop     %ecx
    cdq
    idivl   %ecx
    cmpl    $0, %eax
    jne     .L1
    movl    $0, %eax
    jmp     .L2
.L1:
    movl    $1, %eax
.L2:
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
