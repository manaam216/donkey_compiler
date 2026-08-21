.globl _helper
_helper:
    push    %ebp
    movl    %esp, %ebp
    movl    8(%ebp), %eax
    push    %eax
    movl    $3, %eax
    pop     %edx
    addl    %edx, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    subl    $4, %esp
    movl    $4, %eax
    movl    %eax, -4(%ebp)
    movl    -4(%ebp), %eax
    push    %eax
    movl    $5, %eax
    push    %eax
    call    _helper
    addl    $4, %esp
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -4(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    movl    -4(%ebp), %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
