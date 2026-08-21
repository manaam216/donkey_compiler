.data
.globl _global_byte
_global_byte:
    .long   44
.globl _global_signed_byte
_global_signed_byte:
    .long   -1
.text
.globl _narrow_return
_narrow_return:
    push    %ebp
    movl    %esp, %ebp
    movl    $300, %eax
    movzbl  %al, %eax
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.globl _narrow_argument
_narrow_argument:
    push    %ebp
    movl    %esp, %ebp
    movl    8(%ebp), %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    subl    $16, %esp
    movl    $1, %eax
    negl    %eax
    movl    %eax, -4(%ebp)
    movl    $255, %eax
    movzbl  %al, %eax
    movl    %eax, -8(%ebp)
    movl    $1, %eax
    negl    %eax
    movl    %eax, -12(%ebp)
    movl    $0, %eax
    movl    %eax, -16(%ebp)
    movl    -8(%ebp), %eax
    push    %eax
    addl    $1, %eax
    movzbl  %al, %eax
    movl    %eax, -8(%ebp)
    pop     %eax
    call    _narrow_return
    push    %eax
    movl    $300, %eax
    movzbl  %al, %eax
    push    %eax
    call    _narrow_argument
    addl    $4, %esp
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    -4(%ebp), %eax
    push    %eax
    movl    $1, %eax
    pop     %edx
    cmpl    %eax, %edx
    movl    $0, %eax
    seta    %al
    push    %eax
    movl    $10, %eax
    pop     %edx
    imull   %edx, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    -4(%ebp), %eax
    push    %eax
    movl    $31, %eax
    pop     %edx
    movl    %eax, %ecx
    movl    %edx, %eax
    shrl    %cl, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    -4(%ebp), %eax
    push    %eax
    movl    $2, %eax
    pop     %edx
    push    %eax
    movl    %edx, %eax
    pop     %ecx
    xorl    %edx, %edx
    divl    %ecx
    push    %eax
    movl    $100, %eax
    pop     %edx
    cmpl    %eax, %edx
    movl    $0, %eax
    seta    %al
    push    %eax
    movl    $20, %eax
    pop     %edx
    imull   %edx, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    _global_byte, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    _global_signed_byte, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    -8(%ebp), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    movl    -12(%ebp), %eax
    push    %eax
    movl    -16(%ebp), %eax
    pop     %edx
    cmpl    %eax, %edx
    movl    $0, %eax
    setb    %al
    pop     %edx
    addl    %edx, %eax
    jmp     .L2
    movl    $0, %eax
.L2:
    leave
    ret
