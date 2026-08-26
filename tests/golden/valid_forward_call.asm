.globl main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    movl    $3, %eax
    pushq   %rax
    popq    %rdi
    movl    $0, %eax
    call    add_two
    jmp     .L0
    movl    $0, %eax
.L0:
    leave
    ret
.globl add_two
add_two:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $16, %rsp
    movl    %edi, -4(%rbp)
    movl    -4(%rbp), %eax
    pushq   %rax
    movl    $2, %eax
    popq    %rdx
    addl    %edx, %eax
    jmp     .L1
    movl    $0, %eax
.L1:
    leave
    ret
.section .note.GNU-stack,"",@progbits
