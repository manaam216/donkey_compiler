.globl _choose
_choose:
    push    %ebp
    movl    %esp, %ebp
    movl    8(%ebp), %eax
    push    %eax
    movl    $5, %eax
    pop     %edx
    cmpl    %eax, %edx
    movl    $0, %eax
    setg    %al
    cmpl    $0, %eax
    je      .L1
    movl    8(%ebp), %eax
    jmp     .L0
    jmp     .L2
.L1:
    movl    $5, %eax
    jmp     .L0
.L2:
    movl    $0, %eax
.L0:
    leave
    ret
.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    subl    $12, %esp
    movl    $0, %eax
    movl    %eax, -4(%ebp)
    movl    $0, %eax
    movl    %eax, -8(%ebp)
.L4:
    movl    -4(%ebp), %eax
    push    %eax
    movl    $5, %eax
    pop     %edx
    cmpl    %eax, %edx
    movl    $0, %eax
    setl    %al
    cmpl    $0, %eax
    je      .L5
    movl    -4(%ebp), %eax
    push    %eax
    movl    $1, %eax
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
    cmpl    %eax, %edx
    movl    $0, %eax
    sete    %al
    cmpl    $0, %eax
    je      .L6
    jmp     .L4
    jmp     .L7
.L6:
.L7:
    movl    -8(%ebp), %eax
    push    %eax
    movl    -4(%ebp), %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -8(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    jmp     .L4
.L5:
    movl    $0, %eax
    movl    %eax, -12(%ebp)
.L8:
    movl    -12(%ebp), %eax
    push    %eax
    movl    $5, %eax
    pop     %edx
    cmpl    %eax, %edx
    movl    $0, %eax
    setl    %al
    cmpl    $0, %eax
    je      .L10
    movl    -12(%ebp), %eax
    push    %eax
    movl    $4, %eax
    pop     %edx
    cmpl    %eax, %edx
    movl    $0, %eax
    sete    %al
    cmpl    $0, %eax
    je      .L11
    jmp     .L10
    jmp     .L12
.L11:
.L12:
    movl    -8(%ebp), %eax
    push    %eax
    movl    $1, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -8(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
.L9:
    movl    -12(%ebp), %eax
    push    %eax
    movl    $1, %eax
    pop     %edx
    addl    %edx, %eax
    push    %eax
    leal    -12(%ebp), %eax
    pop     %edx
    movl    %edx, (%eax)
    movl    %edx, %eax
    jmp     .L8
.L10:
    movl    -8(%ebp), %eax
    push    %eax
    call    _choose
    addl    $4, %esp
    jmp     .L3
    movl    $0, %eax
.L3:
    leave
    ret
