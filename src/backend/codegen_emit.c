/*
 * Code generator: the instruction layer. One function per thing the machine can
 * be told to do -- loads and stores at each width, conversions, and the SSE
 * moves floating values need.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "decl.h"
#include "type.h"
#include "symbol.h"
#include "diag.h"
#include "support/mem.h"
#include "codegen_internal.h"

const char *fp_suffix(struct Type *ty)
{
    return ty && ty->size == 4 ? "ss" : "sd";
}

void emit_fp_push(struct Type *ty, FILE *output)
{
    fprintf(output, "    subq    $8, %%rsp\n");
    fprintf(output, "    mov%s   %%xmm0, (%%rsp)\n", fp_suffix(ty));
}

void emit_fp_pop(struct Type *ty, const char *reg, FILE *output)
{
    fprintf(output, "    mov%s   (%%rsp), %%%s\n", fp_suffix(ty), reg);
    fprintf(output, "    addq    $8, %%rsp\n");
}

void emit_fp_load_frame(struct Type *ty, int offset, FILE *output)
{
    fprintf(output, "    mov%s   %d(%%rbp), %%xmm0\n", fp_suffix(ty), offset);
}

void emit_fp_store_frame(struct Type *ty, int offset, FILE *output)
{
    fprintf(output, "    mov%s   %%xmm0, %d(%%rbp)\n", fp_suffix(ty), offset);
}

void emit_int_to_fp(struct Type *target, FILE *output)
{
    fprintf(output, "    cvtsi2%s %%eax, %%xmm0\n", fp_suffix(target));
}

void emit_fp_to_int(struct Type *source, FILE *output)
{
    fprintf(output, "    cvtt%s2si %%xmm0, %%eax\n", fp_suffix(source));
}

void emit_fp_widen(struct Type *from, struct Type *to, FILE *output)
{
    if (!from || !to || from->size == to->size) {
        return;
    }
    if (from->size == 4) {
        fprintf(output, "    cvtss2sd %%xmm0, %%xmm0\n");
    } else {
        fprintf(output, "    cvtsd2ss %%xmm0, %%xmm0\n");
    }
}

void emit_load_indirect(struct Type *ty, FILE *output)
{
    int size = ty ? ty->size : 4;

    if (size == 1) {
        fprintf(output, "    %s  (%%rax), %%eax\n",
            ty && ty->is_unsigned ? "movzbl" : "movsbl");
    } else if (size == 2) {
        fprintf(output, "    %s  (%%rax), %%eax\n",
            ty && ty->is_unsigned ? "movzwl" : "movswl");
    } else if (size == 8) {
        fprintf(output, "    movq    (%%rax), %%rax\n");
    } else {
        fprintf(output, "    movl    (%%rax), %%eax\n");
    }
}

void emit_store_offset(struct Type *ty, int offset, FILE *output)
{
    int size = ty ? ty->size : 4;

    if (size == 1) {
        fprintf(output, "    movb    %%al, %d(%%rbp)\n", offset);
    } else if (size == 2) {
        fprintf(output, "    movw    %%ax, %d(%%rbp)\n", offset);
    } else if (size == 8) {
        fprintf(output, "    movq    %%rax, %d(%%rbp)\n", offset);
    } else {
        fprintf(output, "    movl    %%eax, %d(%%rbp)\n", offset);
    }
}

void emit_zero_offset(struct Type *ty, int offset, FILE *output)
{
    int size = ty ? ty->size : 4;

    if (size == 1) {
        fprintf(output, "    movb    $0, %d(%%rbp)\n", offset);
    } else if (size == 2) {
        fprintf(output, "    movw    $0, %d(%%rbp)\n", offset);
    } else if (size == 8) {
        fprintf(output, "    movq    $0, %d(%%rbp)\n", offset);
    } else {
        fprintf(output, "    movl    $0, %d(%%rbp)\n", offset);
    }
}

void emit_store_indirect(struct Type *ty, FILE *output)
{
    int size = ty ? ty->size : 4;

    if (size == 1) {
        fprintf(output, "    movb    %%dl, (%%rax)\n");
    } else if (size == 2) {
        fprintf(output, "    movw    %%dx, (%%rax)\n");
    } else if (size == 8) {
        fprintf(output, "    movq    %%rdx, (%%rax)\n");
    } else {
        fprintf(output, "    movl    %%edx, (%%rax)\n");
    }
}

void emit_store_slot(struct Type *ty, int offset, FILE *output)
{
    /* A floating value is in %xmm0, not %rax. */
    if (ty_is_float(ty)) {
        emit_fp_store_frame(ty, offset, output);
        return;
    }
    if (ty && ty->size == 8) {
        fprintf(output, "    movq    %%rax, %d(%%rbp)\n", offset);
    } else {
        fprintf(output, "    movl    %%eax, %d(%%rbp)\n", offset);
    }
}

void emit_zero_slot(struct Type *ty, int offset, FILE *output)
{
    /* Zeroing a floating slot writes the bit pattern, so an integer store. */
    if (ty_is_float(ty)) {
        fprintf(output, "    movq    $0, %d(%%rbp)\n", offset);
        return;
    }
    if (ty && ty->size == 8) {
        fprintf(output, "    movq    $0, %d(%%rbp)\n", offset);
    } else {
        fprintf(output, "    movl    $0, %d(%%rbp)\n", offset);
    }
}

void emit_load_frame(struct Type *ty, int offset, FILE *output)
{
    int size = ty ? ty->size : 4;

    if (size == 1) {
        fprintf(output, "    %s  %d(%%rbp), %%eax\n",
            ty && ty->is_unsigned ? "movzbl" : "movsbl", offset);
    } else if (size == 2) {
        fprintf(output, "    %s  %d(%%rbp), %%eax\n",
            ty && ty->is_unsigned ? "movzwl" : "movswl", offset);
    } else if (size == 8) {
        fprintf(output, "    movq    %d(%%rbp), %%rax\n", offset);
    } else {
        fprintf(output, "    movl    %d(%%rbp), %%eax\n", offset);
    }
}

void emit_load_global(struct Type *ty, const char *name, FILE *output)
{
    int size = ty ? ty->size : 4;

    if (size == 1) {
        fprintf(output, "    %s  %s(%%rip), %%eax\n",
            ty && ty->is_unsigned ? "movzbl" : "movsbl", name);
    } else if (size == 2) {
        fprintf(output, "    %s  %s(%%rip), %%eax\n",
            ty && ty->is_unsigned ? "movzwl" : "movswl", name);
    } else if (size == 8) {
        fprintf(output, "    movq    %s(%%rip), %%rax\n", name);
    } else {
        fprintf(output, "    movl    %s(%%rip), %%eax\n", name);
    }
}

void emit_store_global(struct Type *ty, const char *name, FILE *output)
{
    int size = ty ? ty->size : 4;

    if (size == 1) {
        fprintf(output, "    movb    %%al, %s(%%rip)\n", name);
    } else if (size == 2) {
        fprintf(output, "    movw    %%ax, %s(%%rip)\n", name);
    } else if (size == 8) {
        fprintf(output, "    movq    %%rax, %s(%%rip)\n", name);
    } else {
        fprintf(output, "    movl    %%eax, %s(%%rip)\n", name);
    }
}

void emit_step(struct Type *ty, int is_increment, FILE *output)
{
    int pointer = ty && ty->kind == TY_PTR;
    int step = pointer ? ty_element_size(ty) : 1;

    if (pointer) {
        fprintf(output, "    %s    $%d, %%rax\n", is_increment ? "addq" : "subq", step);
    } else {
        fprintf(output, "    %s    $%d, %%eax\n", is_increment ? "addl" : "subl", step);
    }
}

void emit_compare(struct Type *left, struct Type *right, FILE *output)
{
    int size = 4;

    if (left && left->size > size) size = left->size;
    if (right && right->size > size) size = right->size;

    fprintf(output, size == 8 ? "    cmpq    %%rax, %%rdx\n"
                              : "    cmpl    %%eax, %%edx\n");
}

void emit_struct_copy(int size, FILE *output)
{
    int offset = 0;

    while (size - offset >= 8) {
        fprintf(output, "    movq    %d(%%rdx), %%rcx\n", offset);
        fprintf(output, "    movq    %%rcx, %d(%%rax)\n", offset);
        offset += 8;
    }
    while (size - offset >= 4) {
        fprintf(output, "    movl    %d(%%rdx), %%ecx\n", offset);
        fprintf(output, "    movl    %%ecx, %d(%%rax)\n", offset);
        offset += 4;
    }
    while (size - offset >= 1) {
        fprintf(output, "    movb    %d(%%rdx), %%cl\n", offset);
        fprintf(output, "    movb    %%cl, %d(%%rax)\n", offset);
        offset += 1;
    }
}

void generate_epilogue(FILE *output)
{
    fprintf(output, "    leave\n");
    fprintf(output, "    ret\n");
}

int is_frame_symbol(struct Symbol *sym)
{
    return sym->kind == SYM_LOCAL || sym->kind == SYM_PARAM;
}

void generate_cast_between(struct Type *from, struct Type *to, FILE *output)
{
    if (ty_is_float(from) && ty_is_float(to)) {
        emit_fp_widen(from, to, output);
        return;
    }
    if (ty_is_float(from)) {
        emit_fp_to_int(from, output);       /* truncates, as C requires */
        return;
    }
    if (ty_is_float(to)) {
        emit_int_to_fp(to, output);
    }
}

void generate_cast(const char *type, FILE *output)
{
    if (!type) {
        return;
    }
    if (strcmp(type, "char") == 0) {
        fprintf(output, "    movsbl  %%al, %%eax\n");
    } else if (strcmp(type, "uchar") == 0) {
        fprintf(output, "    movzbl  %%al, %%eax\n");
    } else if (strcmp(type, "short") == 0) {
        fprintf(output, "    movswl  %%ax, %%eax\n");
    } else if (strcmp(type, "ushort") == 0) {
        fprintf(output, "    movzwl  %%ax, %%eax\n");
    } else if (strcmp(type, "long") == 0) {
        /*
         * Widening to 8 bytes: values are computed in %eax, so the upper half
         * of %rax is undefined until it is extended explicitly.
         */
        fprintf(output, "    cltq\n");
    } else if (strcmp(type, "ulong") == 0) {
        /* Writing %eax zeroes the upper half of %rax. */
        fprintf(output, "    movl    %%eax, %%eax\n");
    }
}

const char *codegen_type_name(CType type)
{
    switch (type) {
        case TYPE_CHAR: return "char";
        case TYPE_UCHAR: return "uchar";
        case TYPE_SHORT: return "short";
        case TYPE_USHORT: return "ushort";
        case TYPE_UINT: return "uint";
        case TYPE_LONG: return "long";
        case TYPE_ULONG: return "ulong";
        default: return "int";
    }
}

int is_unsigned_type(CType type)
{
    return type == TYPE_UCHAR || type == TYPE_USHORT ||
        type == TYPE_UINT || type == TYPE_ULONG;
}

void generate_stack_note(FILE *output)
{
    fprintf(output, ".section .note.GNU-stack,\"\",@progbits\n");
}
