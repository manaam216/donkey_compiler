.globl _main
_main:
    push    %ebp
    movl    %esp, %ebp
    movl    $10, %eax
    push    %eax
    movl    $4, %eax
    pop     %edx
    push    %eax
    movl    %edx, %eax
    pop     %ecx
    cdq
    idivl   %ecx
    movl    %edx, %eax
    push    %eax
    movl    $2, %eax
    pop     %edx
    cmpl    %eax, %edx
    movl    $0, %eax
    sete    %al
    cmpl    $0, %eax
    je      .L15
    movl    $7, %eax
    push    %eax
    movl    $3, %eax
    pop     %edx
    andl    %edx, %eax
    push    %eax
    movl    $3, %eax
    pop     %edx
    cmpl    %eax, %edx
    movl    $0, %eax
    sete    %al
    cmpl    $0, %eax
    je      .L15
    movl    $1, %eax
    jmp     .L16
.L15:
    movl    $0, %eax
.L16:
    cmpl    $0, %eax
    je      .L13
    movl    $4, %eax
    push    %eax
    movl    $1, %eax
    pop     %edx
    orl     %edx, %eax
    push    %eax
    movl    $5, %eax
    pop     %edx
    cmpl    %eax, %edx
    movl    $0, %eax
    sete    %al
    cmpl    $0, %eax
    je      .L13
    movl    $1, %eax
    jmp     .L14
.L13:
    movl    $0, %eax
.L14:
    cmpl    $0, %eax
    je      .L11
    movl    $6, %eax
    push    %eax
    movl    $3, %eax
    pop     %edx
    xorl    %edx, %eax
    push    %eax
    movl    $5, %eax
    pop     %edx
    cmpl    %eax, %edx
    movl    $0, %eax
    sete    %al
    cmpl    $0, %eax
    je      .L11
    movl    $1, %eax
    jmp     .L12
.L11:
    movl    $0, %eax
.L12:
    cmpl    $0, %eax
    je      .L9
    movl    $3, %eax
    push    %eax
    movl    $4, %eax
    pop     %edx
    cmpl    %eax, %edx
    movl    $0, %eax
    setl    %al
    cmpl    $0, %eax
    je      .L9
    movl    $1, %eax
    jmp     .L10
.L9:
    movl    $0, %eax
.L10:
    cmpl    $0, %eax
    je      .L7
    movl    $4, %eax
    push    %eax
    movl    $4, %eax
    pop     %edx
    cmpl    %eax, %edx
    movl    $0, %eax
    setle   %al
    cmpl    $0, %eax
    je      .L7
    movl    $1, %eax
    jmp     .L8
.L7:
    movl    $0, %eax
.L8:
    cmpl    $0, %eax
    je      .L5
    movl    $5, %eax
    push    %eax
    movl    $2, %eax
    pop     %edx
    cmpl    %eax, %edx
    movl    $0, %eax
    setg    %al
    cmpl    $0, %eax
    je      .L5
    movl    $1, %eax
    jmp     .L6
.L5:
    movl    $0, %eax
.L6:
    cmpl    $0, %eax
    je      .L3
    movl    $5, %eax
    push    %eax
    movl    $5, %eax
    pop     %edx
    cmpl    %eax, %edx
    movl    $0, %eax
    setge   %al
    cmpl    $0, %eax
    je      .L3
    movl    $1, %eax
    jmp     .L4
.L3:
    movl    $0, %eax
.L4:
    cmpl    $0, %eax
    je      .L1
    movl    $0, %eax
    cmpl    $0, %eax
    jne     .L17
    movl    $9, %eax
    cmpl    $0, %eax
    jne     .L17
    movl    $0, %eax
    jmp     .L18
.L17:
    movl    $1, %eax
.L18:
    cmpl    $0, %eax
    je      .L1
    movl    $1, %eax
    jmp     .L2
.L1:
    movl    $0, %eax
.L2:
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
