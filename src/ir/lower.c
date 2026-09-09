/*
 * Lowering: the analysed syntax tree becomes three-address code.
 *
 * The tree is nested and the IR is flat, so every expression is lowered to a
 * value and every statement to a run of blocks. Two rules keep it honest:
 *
 *   Locals live in memory. A declaration becomes an IR_ALLOCA and every read
 *   and write goes through IR_LOAD and IR_STORE. That is always correct, needs
 *   no analysis, and works the same whether the variable's address is taken or
 *   not. ir_build_ssa promotes the ones it can afterwards.
 *
 *   Control flow is explicit. Short-circuit operators and the conditional
 *   operator become real branches over real blocks, with the result written to
 *   a slot the promotion pass turns into a phi. Nothing in the IR needs to know
 *   that && was ever special.
 *
 * Lowering runs after semantic analysis and reads what that pass attached:
 * `ty` for sizes and pointer scaling, `sym` for storage. It does not re-derive
 * either, and it does not diagnose -- anything ill-formed was rejected before
 * it got here.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
#include "decl.h"
#include "diag.h"
#include "support/mem.h"

struct loop_target {
    struct ir_block *break_block;
    struct ir_block *continue_block;
};

struct label_target {
    char *name;
    struct ir_block *block;
};

struct lower_ctx {
    struct ir_func *func;

    /* Where instructions are appended. Moves as blocks are opened. */
    struct ir_block *block;

    struct loop_target *loops;
    int loop_depth;
    int loop_capacity;

    /* The break target of the enclosing switch, if it is nearer than a loop. */
    struct ir_block *switch_break;

    /*
     * Blocks for the source labels in this function, created on first mention.
     * A goto may name a label that has not been reached yet, so the block has
     * to exist before the statement that carries it is lowered.
     */
    struct label_target *labels;
    int label_count;
    int label_capacity;

    /*
     * The frame slot for each local, keyed by symbol. A declaration creates
     * one; every mention of the name finds it again here rather than through a
     * name lookup, so two variables that share a name stay apart.
     */
    struct {
        struct Symbol *sym;
        struct ir_value *slot;
    } *slots;
    int slot_count;
    int slot_capacity;

    /*
     * The block each case label of the enclosing switches starts. A case node
     * records its index here in string_label, which is unused on a case -- the
     * old back end kept its label number in the same place for the same
     * reason. Indices are unique across the function, so a nested switch adds
     * to the same table without disturbing the one outside it.
     */
    struct ir_block **case_blocks;
    int case_block_count;
    int case_block_capacity;
};

static struct ir_value *lower_expr(struct lower_ctx *ctx, struct ast_node *node);
static struct ir_value *lower_addr(struct lower_ctx *ctx, struct ast_node *node);
static void lower_statement(struct lower_ctx *ctx, struct ast_node *node);

/* ---------------------------------------------------------- block plumbing -- */

/*
 * Open a block and make it the insertion point. Blocks are created in the
 * order they will be laid out, which keeps the dump readable and means
 * reverse postorder usually matches source order.
 */
static struct ir_block *open_block(struct lower_ctx *ctx, struct ir_block *block)
{
    ctx->block = block;
    return block;
}

static struct ir_block *new_block(struct lower_ctx *ctx, const char *label)
{
    return ir_block_new(ctx->func, label);
}

/*
 * End the current block with a jump, unless it is already terminated. A
 * `return` in the middle of a body leaves the block finished; the statements
 * after it still lower, but into a block nothing reaches, and the CFG pass
 * drops them.
 */
static void jump_to(struct lower_ctx *ctx, struct ir_block *target)
{
    struct ir_instr *jump;

    if (ir_terminator(ctx->block)) {
        return;
    }
    jump = ir_emit(ctx->block, IR_JMP);
    jump->target = target;
}

static void branch_to(struct lower_ctx *ctx, struct ir_value *condition,
    struct ir_block *then_block, struct ir_block *else_block)
{
    struct ir_instr *branch;

    if (ir_terminator(ctx->block)) {
        return;
    }
    branch = ir_emit(ctx->block, IR_BR);
    ir_add_arg(branch, condition);
    branch->then_block = then_block;
    branch->else_block = else_block;
}

static struct ir_value *emit_const(struct lower_ctx *ctx, long long value,
    struct Type *ty)
{
    struct ir_instr *instr = ir_emit(ctx->block, IR_CONST);

    instr->imm = value;
    return ir_set_dst(ctx->func, instr, ty ? ty : ty_int);
}

static struct ir_value *emit_load(struct lower_ctx *ctx, struct ir_value *address,
    struct Type *ty)
{
    struct ir_instr *instr = ir_emit(ctx->block, IR_LOAD);

    ir_add_arg(instr, address);
    instr->mem_type = ty;
    return ir_set_dst(ctx->func, instr, ty);
}

static void emit_store(struct lower_ctx *ctx, struct ir_value *address,
    struct ir_value *value, struct Type *ty)
{
    struct ir_instr *instr = ir_emit(ctx->block, IR_STORE);

    ir_add_arg(instr, address);
    ir_add_arg(instr, value);
    instr->mem_type = ty;
}

static struct ir_value *emit_binary(struct lower_ctx *ctx, IROp op,
    struct ir_value *left, struct ir_value *right, struct Type *ty)
{
    struct ir_instr *instr = ir_emit(ctx->block, op);

    ir_add_arg(instr, left);
    ir_add_arg(instr, right);
    return ir_set_dst(ctx->func, instr, ty);
}

/* --------------------------------------------------------------- slots -- */

static struct ir_value *find_slot(struct lower_ctx *ctx, struct Symbol *sym)
{
    int i;

    for (i = 0; i < ctx->slot_count; i++) {
        if (ctx->slots[i].sym == sym) {
            return ctx->slots[i].slot;
        }
    }
    return NULL;
}

/*
 * The slot for a local. Allocas are emitted into the entry block wherever the
 * declaration appears, so the frame is laid out once on entry rather than
 * being reserved and released as scopes come and go.
 */
static struct ir_value *slot_for(struct lower_ctx *ctx, struct Symbol *sym)
{
    struct ir_instr *instr;
    struct ir_value *slot = find_slot(ctx, sym);

    if (slot) {
        return slot;
    }

    instr = ir_emit_front(ctx->func->entry, IR_ALLOCA);
    instr->sym = sym;
    slot = ir_set_dst(ctx->func, instr, ty_pointer_to(sym->ty));

    grow_array((void **)&ctx->slots, ctx->slot_count, &ctx->slot_capacity,
        sizeof(*ctx->slots), 16, "IR slot table");
    ctx->slots[ctx->slot_count].sym = sym;
    ctx->slots[ctx->slot_count].slot = slot;
    ctx->slot_count++;
    return slot;
}

/*
 * An unnamed slot, for a value that has to survive a branch: the result of &&,
 * of ?:, of anything the source did not give a name to. The promotion pass
 * turns it into a phi, so nothing downstream sees the memory.
 */
static struct ir_value *temp_slot(struct lower_ctx *ctx, struct Type *ty)
{
    struct ir_instr *instr;
    struct ir_value *slot;

    instr = ir_emit_front(ctx->func->entry, IR_ALLOCA);
    instr->sym = NULL;
    slot = ir_set_dst(ctx->func, instr, ty_pointer_to(ty));
    return slot;
}

/* -------------------------------------------------------------- labels -- */

static struct ir_block *block_for_label(struct lower_ctx *ctx, const char *name)
{
    int i;

    for (i = 0; i < ctx->label_count; i++) {
        if (strcmp(ctx->labels[i].name, name) == 0) {
            return ctx->labels[i].block;
        }
    }

    grow_array((void **)&ctx->labels, ctx->label_count, &ctx->label_capacity,
        sizeof(*ctx->labels), 8, "IR label table");
    ctx->labels[ctx->label_count].name = xstrdup(name, "IR label name");
    ctx->labels[ctx->label_count].block = new_block(ctx, name);
    return ctx->labels[ctx->label_count++].block;
}

static void push_loop(struct lower_ctx *ctx, struct ir_block *break_block,
    struct ir_block *continue_block)
{
    grow_array((void **)&ctx->loops, ctx->loop_depth, &ctx->loop_capacity,
        sizeof(*ctx->loops), 8, "IR loop stack");
    ctx->loops[ctx->loop_depth].break_block = break_block;
    ctx->loops[ctx->loop_depth].continue_block = continue_block;
    ctx->loop_depth++;
}

static void pop_loop(struct lower_ctx *ctx)
{
    if (ctx->loop_depth > 0) {
        ctx->loop_depth--;
    }
}

/* --------------------------------------------------------- expressions -- */

static int is_pointer_operand(struct ast_node *node)
{
    return node->ty && ty_is_pointer_like(node->ty);
}

static IROp binary_op_for(ASTNodeType type)
{
    switch (type) {
        case AST_ADD:           return IR_ADD;
        case AST_SUB:           return IR_SUB;
        case AST_MUL:           return IR_MUL;
        case AST_DIV:           return IR_DIV;
        case AST_MOD:           return IR_MOD;
        case AST_SHIFT_LEFT:    return IR_SHL;
        case AST_SHIFT_RIGHT:   return IR_SHR;
        case AST_BITWISE_AND:   return IR_AND;
        case AST_BITWISE_OR:    return IR_OR;
        case AST_BITWISE_XOR:   return IR_XOR;
        case AST_EQUAL:         return IR_EQ;
        case AST_NOT_EQUAL:     return IR_NE;
        case AST_LESS:          return IR_LT;
        case AST_LESS_EQUAL:    return IR_LE;
        case AST_GREATER:       return IR_GT;
        case AST_GREATER_EQUAL: return IR_GE;
        default:                return IR_NOP;
    }
}

/*
 * Pointer arithmetic scales the integer side by the pointee's size, and a
 * difference of two pointers divides it back out. Doing it here means the IR
 * carries plain byte arithmetic and no later pass has to remember that one
 * operand was a pointer.
 */
static struct ir_value *lower_pointer_arithmetic(struct lower_ctx *ctx,
    struct ast_node *node, struct ir_value *left, struct ir_value *right)
{
    int left_pointer = is_pointer_operand(node->left);
    int right_pointer = is_pointer_operand(node->right);
    struct Type *ty = node->ty ? node->ty : ty_long;
    struct ir_value *scale;

    if (left_pointer && right_pointer) {
        struct ir_value *difference =
            emit_binary(ctx, IR_SUB, left, right, ty_long);

        scale = emit_const(ctx, ty_element_size(node->left->ty), ty_long);
        return emit_binary(ctx, IR_DIV, difference, scale, ty);
    }

    if (left_pointer) {
        scale = emit_const(ctx, ty_element_size(node->left->ty), ty_long);
        right = emit_binary(ctx, IR_MUL, right, scale, ty_long);
        return emit_binary(ctx, node->type == AST_ADD ? IR_ADD : IR_SUB,
            left, right, ty);
    }

    /* `n + p`, the only shape where the pointer is on the right. */
    scale = emit_const(ctx, ty_element_size(node->right->ty), ty_long);
    left = emit_binary(ctx, IR_MUL, left, scale, ty_long);
    return emit_binary(ctx, IR_ADD, left, right, ty);
}

/*
 * && and || evaluate their right operand only when the left did not already
 * decide the answer. The result goes through a slot rather than being built
 * from a phi here, so that lowering never has to reason about which block it
 * will end up in.
 */
static struct ir_value *lower_short_circuit(struct lower_ctx *ctx,
    struct ast_node *node)
{
    int is_and = node->type == AST_LOGICAL_AND;
    struct ir_block *rhs_block = new_block(ctx, NULL);
    struct ir_block *done_block = new_block(ctx, NULL);
    struct ir_value *slot = temp_slot(ctx, ty_int);
    struct ir_value *left = lower_expr(ctx, node->left);
    struct ir_value *zero = emit_const(ctx, 0, ty_int);
    struct ir_value *left_true = emit_binary(ctx, IR_NE, left, zero, ty_int);
    struct ir_value *right;

    emit_store(ctx, slot, left_true, ty_int);
    if (is_and) {
        branch_to(ctx, left_true, rhs_block, done_block);
    } else {
        branch_to(ctx, left_true, done_block, rhs_block);
    }

    open_block(ctx, rhs_block);
    right = lower_expr(ctx, node->right);
    zero = emit_const(ctx, 0, ty_int);
    emit_store(ctx, slot, emit_binary(ctx, IR_NE, right, zero, ty_int), ty_int);
    jump_to(ctx, done_block);

    open_block(ctx, done_block);
    return emit_load(ctx, slot, ty_int);
}

static struct ir_value *lower_conditional(struct lower_ctx *ctx,
    struct ast_node *node)
{
    struct ast_node *branches = node->right;
    struct Type *ty = node->ty ? node->ty : ty_int;
    struct ir_block *then_block = new_block(ctx, NULL);
    struct ir_block *else_block = new_block(ctx, NULL);
    struct ir_block *done_block = new_block(ctx, NULL);
    struct ir_value *slot = temp_slot(ctx, ty);
    struct ir_value *condition = lower_expr(ctx, node->left);

    branch_to(ctx, condition, then_block, else_block);

    open_block(ctx, then_block);
    emit_store(ctx, slot, lower_expr(ctx, branches->left), ty);
    jump_to(ctx, done_block);

    open_block(ctx, else_block);
    emit_store(ctx, slot, lower_expr(ctx, branches->right), ty);
    jump_to(ctx, done_block);

    open_block(ctx, done_block);
    return emit_load(ctx, slot, ty);
}

static struct ir_value *lower_call(struct lower_ctx *ctx, struct ast_node *node)
{
    struct ir_instr *instr;
    struct ir_value *callee;
    struct ast_node *argument;
    struct Type *return_type = node->ty ? node->ty : ty_int;
    struct ir_value **arguments = NULL;
    int argument_count = 0;
    int argument_capacity = 0;
    int i;

    /*
     * The callee is just another operand. A direct call names a function; an
     * indirect one loads the pointer out of the variable holding it. After this
     * point the two are the same instruction with a differently-produced first
     * argument, which is the whole reason the callee is an operand at all.
     */
    if (node->is_indirect_call) {
        struct ir_value *slot = slot_for(ctx, node->sym);

        callee = emit_load(ctx, slot, node->sym->ty);
    } else {
        struct ir_instr *address = ir_emit(ctx->block, IR_GLOBAL);

        address->sym = node->sym;
        address->text = xstrdup(node->value ? node->value : "?", "IR callee name");
        callee = ir_set_dst(ctx->func, address, ty_pointer_to(return_type));
    }

    /*
     * Every argument is lowered before the call instruction exists. Building
     * the call first and adding operands as they are lowered would leave the
     * instructions that compute them sitting after the call.
     */
    for (argument = node->left; argument; argument = argument->right) {
        if (argument->type != AST_ARG_LIST) {
            break;
        }
        if (argument->left) {
            grow_array((void **)&arguments, argument_count, &argument_capacity,
                sizeof(*arguments), 8, "IR call argument list");
            arguments[argument_count++] = lower_expr(ctx, argument->left);
        }
    }

    instr = ir_emit(ctx->block, IR_CALL);
    instr->location = node->location;
    ir_add_arg(instr, callee);
    for (i = 0; i < argument_count; i++) {
        ir_add_arg(instr, arguments[i]);
    }
    free(arguments);

    if (return_type->kind == TY_VOID) {
        return NULL;
    }
    return ir_set_dst(ctx->func, instr, return_type);
}

/*
 * ++x and x++ differ only in which value is handed back, so one function does
 * both. The step is the pointee's size for a pointer and one otherwise.
 */
static struct ir_value *lower_incdec(struct lower_ctx *ctx, struct ast_node *node,
    int is_increment, int is_prefix)
{
    struct Type *ty = node->left->ty ? node->left->ty : ty_int;
    struct ir_value *address = lower_addr(ctx, node->left);
    struct ir_value *before = emit_load(ctx, address, ty);
    struct ir_value *step = emit_const(ctx,
        ty_is_pointer_like(ty) ? ty_element_size(ty) : 1, ty);
    struct ir_value *after = emit_binary(ctx, is_increment ? IR_ADD : IR_SUB,
        before, step, ty);

    emit_store(ctx, address, after, ty);
    return is_prefix ? after : before;
}

static struct ir_value *lower_assign(struct lower_ctx *ctx, struct ast_node *node)
{
    struct Type *ty = node->left->ty ? node->left->ty : ty_int;
    struct ir_value *address;
    struct ir_value *value;

    /*
     * A struct is copied, not moved: it does not fit in a value. Both sides
     * are addresses, and the size comes from the type rather than from
     * counting registers.
     */
    if (ty->kind == TY_STRUCT) {
        struct ir_value *source;
        struct ir_instr *copy;

        /*
         * A struct that came back from a call has no address: it is in the
         * return registers. Storing the value is the honest description, and
         * it is the back end's job to know how many registers that is.
         */
        if (node->right->type == AST_CALL) {
            struct ir_value *returned = lower_expr(ctx, node->right);

            address = lower_addr(ctx, node->left);
            emit_store(ctx, address, returned, ty);
            return address;
        }

        source = lower_addr(ctx, node->right);
        address = lower_addr(ctx, node->left);
        copy = ir_emit(ctx->block, IR_MEMCPY);
        ir_add_arg(copy, address);
        ir_add_arg(copy, source);
        copy->imm = ty->size;
        return address;
    }

    value = lower_expr(ctx, node->right);
    address = lower_addr(ctx, node->left);
    emit_store(ctx, address, value, ty);
    return value;
}

/*
 * The address of an object. Every assignable expression has one, and this is
 * the only place that knows how each kind of object is addressed.
 */
static struct ir_value *lower_addr(struct lower_ctx *ctx, struct ast_node *node)
{
    switch (node->type) {
        case AST_IDENTIFIER:
        case AST_COMPOUND_LITERAL: {
            struct Symbol *sym = node->sym;

            if (sym && (sym->kind == SYM_LOCAL || sym->kind == SYM_PARAM)) {
                return slot_for(ctx, sym);
            } else {
                struct ir_instr *instr = ir_emit(ctx->block, IR_GLOBAL);

                instr->sym = sym;
                instr->text = xstrdup(sym ? sym->name : node->value,
                    "IR global name");
                return ir_set_dst(ctx->func, instr,
                    ty_pointer_to(node->ty ? node->ty : ty_int));
            }
        }
        case AST_DEREFERENCE:
            /* `*p` as an lvalue is the pointer itself; there is nothing to do. */
            return lower_expr(ctx, node->left);
        case AST_ARRAY_SUBSCRIPT: {
            struct Type *element = node->left->ty && node->left->ty->base
                ? node->left->ty->base : ty_int;
            struct ir_value *base = lower_expr(ctx, node->left);
            struct ir_value *index = lower_expr(ctx, node->right);
            struct ir_value *scale = emit_const(ctx,
                ty_element_size(node->left->ty), ty_long);
            struct ir_value *offset =
                emit_binary(ctx, IR_MUL, index, scale, ty_long);

            return emit_binary(ctx, IR_ADD, base, offset,
                ty_pointer_to(element));
        }
        case AST_FIELD_ACCESS: {
            struct Member *member = node->left->ty
                ? ty_find_member(node->left->ty, node->value) : NULL;
            struct ir_value *base = lower_addr(ctx, node->left);
            struct ir_value *offset =
                emit_const(ctx, member ? member->offset : 0, ty_long);

            return emit_binary(ctx, IR_ADD, base, offset,
                ty_pointer_to(node->ty ? node->ty : ty_int));
        }
        default:
            diag_internal(node->location,
                "expression has no address in IR lowering (node type %d)",
                node->type);
            return emit_const(ctx, 0, ty_long);
    }
}

static struct ir_value *lower_expr(struct lower_ctx *ctx, struct ast_node *node)
{
    if (!node) {
        return NULL;
    }

    switch (node->type) {
        case AST_INTLIT:
            return emit_const(ctx, node->value ? strtoll(node->value, NULL, 0) : 0,
                node->ty ? node->ty : ty_int);
        case AST_FLOATLIT: {
            struct ir_instr *instr = ir_emit(ctx->block, IR_CONST_FP);

            instr->text = xstrdup(node->value ? node->value : "0",
                "IR floating constant");
            return ir_set_dst(ctx->func, instr, node->ty ? node->ty : ty_double);
        }
        case AST_STRINGLIT: {
            struct ir_instr *instr = ir_emit(ctx->block, IR_STR);

            instr->text = xstrdup(node->value ? node->value : "",
                "IR string literal");
            return ir_set_dst(ctx->func, instr, ty_pointer_to(ty_char));
        }
        case AST_IDENTIFIER:
        case AST_FIELD_ACCESS:
        case AST_ARRAY_SUBSCRIPT:
        case AST_DEREFERENCE:
        case AST_COMPOUND_LITERAL: {
            struct Type *ty = node->ty ? node->ty : ty_int;
            struct ir_value *address = lower_addr(ctx, node);

            /*
             * An array or a struct is used by address, not by value: `a` in an
             * expression is where a starts, and loading a whole aggregate into
             * a value is not something the machine can do.
             */
            if (ty->kind == TY_ARRAY || ty->kind == TY_STRUCT) {
                return address;
            }
            return emit_load(ctx, address, ty);
        }
        case AST_ASSIGN:
            return lower_assign(ctx, node);
        case AST_ADDRESS_OF:
            return lower_addr(ctx, node->left);
        case AST_CALL:
            return lower_call(ctx, node);
        case AST_CAST: {
            struct ir_instr *instr;
            struct ir_value *operand = lower_expr(ctx, node->left);

            if (!node->ty || (node->left->ty && node->ty == node->left->ty)) {
                return operand;
            }
            instr = ir_emit(ctx->block, IR_CAST);
            ir_add_arg(instr, operand);
            return ir_set_dst(ctx->func, instr, node->ty);
        }
        case AST_SIZEOF:
            return emit_const(ctx, node->ty ? node->ty->size : 0, ty_ulong);
        case AST_NEGATION:
        case AST_BITWISE_COMPLEMENT:
        case AST_LOGICAL_NEGATION: {
            IROp op = node->type == AST_NEGATION ? IR_NEG
                : node->type == AST_BITWISE_COMPLEMENT ? IR_COMPLEMENT
                : IR_LOGNOT;
            struct ir_value *operand = lower_expr(ctx, node->left);
            struct ir_instr *instr = ir_emit(ctx->block, op);

            ir_add_arg(instr, operand);
            return ir_set_dst(ctx->func, instr, node->ty ? node->ty : ty_int);
        }
        case AST_PRE_INCREMENT:  return lower_incdec(ctx, node, 1, 1);
        case AST_PRE_DECREMENT:  return lower_incdec(ctx, node, 0, 1);
        case AST_POST_INCREMENT: return lower_incdec(ctx, node, 1, 0);
        case AST_POST_DECREMENT: return lower_incdec(ctx, node, 0, 0);
        case AST_LOGICAL_AND:
        case AST_LOGICAL_OR:
            return lower_short_circuit(ctx, node);
        case AST_CONDITIONAL:
            return lower_conditional(ctx, node);
        case AST_COMMA:
            lower_expr(ctx, node->left);
            return lower_expr(ctx, node->right);
        default: {
            IROp op = binary_op_for(node->type);
            struct ir_value *left;
            struct ir_value *right;

            if (op == IR_NOP) {
                diag_internal(node->location,
                    "unsupported expression in IR lowering (node type %d)",
                    node->type);
                return emit_const(ctx, 0, ty_int);
            }

            left = lower_expr(ctx, node->left);
            right = lower_expr(ctx, node->right);

            if ((node->type == AST_ADD || node->type == AST_SUB) &&
                (is_pointer_operand(node->left) || is_pointer_operand(node->right))) {
                return lower_pointer_arithmetic(ctx, node, left, right);
            }
            return emit_binary(ctx, op, left, right,
                node->ty ? node->ty : ty_int);
        }
    }
}

/* ---------------------------------------------------------- statements -- */

static void lower_declaration(struct lower_ctx *ctx, struct ast_node *node)
{
    struct Type *ty = node->ty ? node->ty : ty_int;
    struct ir_value *slot;

    if (!node->sym) {
        return;
    }
    slot = slot_for(ctx, node->sym);
    if (!node->left) {
        return;                 /* uninitialised; the slot exists, that is all */
    }

    /*
     * A brace initialiser fills an object member by member or element by
     * element. Each piece is stored through the slot's address, so an
     * aggregate that is initialised is no different from one that is assigned
     * to a field at a time.
     */
    if (node->left->type == AST_INITIALIZER_LIST) {
        struct ast_node *item;
        struct Member *member = ty->kind == TY_STRUCT ? ty->members : NULL;
        int index = 0;

        for (item = initializer_items(node->left); item; item = item->right) {
            struct ir_value *offset;
            struct ir_value *address;
            struct Type *piece;

            if (!item->left) {
                continue;
            }
            if (ty->kind == TY_STRUCT) {
                if (item->left->designator_field) {
                    member = ty_find_member(ty, item->left->designator_field);
                }
                if (!member) {
                    break;
                }
                piece = member->ty;
                offset = emit_const(ctx, member->offset, ty_long);
                member = member->next;
            } else {
                if (item->left->designator_index >= 0) {
                    index = item->left->designator_index;
                }
                piece = ty->base ? ty->base : ty_int;
                offset = emit_const(ctx, index * ty_element_size(ty), ty_long);
                index++;
            }
            address = emit_binary(ctx, IR_ADD, slot, offset,
                ty_pointer_to(piece));
            emit_store(ctx, address, lower_expr(ctx, item->left), piece);
        }
        return;
    }

    if (ty->kind == TY_STRUCT) {
        struct ir_value *source;
        struct ir_instr *copy;

        /* Initialised from a call: the struct is a value, not a place. */
        if (node->left->type == AST_CALL) {
            emit_store(ctx, slot, lower_expr(ctx, node->left), ty);
            return;
        }

        source = lower_addr(ctx, node->left);
        copy = ir_emit(ctx->block, IR_MEMCPY);
        ir_add_arg(copy, slot);
        ir_add_arg(copy, source);
        copy->imm = ty->size;
        return;
    }

    emit_store(ctx, slot, lower_expr(ctx, node->left), ty);
}

static void lower_if(struct lower_ctx *ctx, struct ast_node *node)
{
    struct ast_node *branches = node->right;
    struct ir_block *then_block = new_block(ctx, NULL);
    struct ir_block *else_block = branches->right ? new_block(ctx, NULL) : NULL;
    struct ir_block *done_block = new_block(ctx, NULL);
    struct ir_value *condition = lower_expr(ctx, node->left);

    branch_to(ctx, condition, then_block, else_block ? else_block : done_block);

    open_block(ctx, then_block);
    lower_statement(ctx, branches->left);
    jump_to(ctx, done_block);

    if (else_block) {
        open_block(ctx, else_block);
        lower_statement(ctx, branches->right);
        jump_to(ctx, done_block);
    }

    open_block(ctx, done_block);
}

static void lower_while(struct lower_ctx *ctx, struct ast_node *node)
{
    struct ir_block *test_block = new_block(ctx, NULL);
    struct ir_block *body_block = new_block(ctx, NULL);
    struct ir_block *done_block = new_block(ctx, NULL);

    jump_to(ctx, test_block);

    open_block(ctx, test_block);
    branch_to(ctx, lower_expr(ctx, node->left), body_block, done_block);

    open_block(ctx, body_block);
    push_loop(ctx, done_block, test_block);
    lower_statement(ctx, node->right);
    pop_loop(ctx);
    jump_to(ctx, test_block);

    open_block(ctx, done_block);
}

static void lower_do_while(struct lower_ctx *ctx, struct ast_node *node)
{
    struct ir_block *body_block = new_block(ctx, NULL);
    struct ir_block *test_block = new_block(ctx, NULL);
    struct ir_block *done_block = new_block(ctx, NULL);

    jump_to(ctx, body_block);

    /* The test comes after the body, so `continue` goes to it and not to the top. */
    open_block(ctx, body_block);
    push_loop(ctx, done_block, test_block);
    lower_statement(ctx, node->right);
    pop_loop(ctx);
    jump_to(ctx, test_block);

    open_block(ctx, test_block);
    branch_to(ctx, lower_expr(ctx, node->left), body_block, done_block);

    open_block(ctx, done_block);
}

static void lower_for(struct lower_ctx *ctx, struct ast_node *node)
{
    struct ast_node *init = node->left->left;
    struct ast_node *condition = node->left->right->left;
    struct ast_node *post = node->left->right->right;
    struct ir_block *test_block = new_block(ctx, NULL);
    struct ir_block *body_block = new_block(ctx, NULL);
    struct ir_block *post_block = new_block(ctx, NULL);
    struct ir_block *done_block = new_block(ctx, NULL);

    if (init) {
        if (init->type == AST_DECL) {
            lower_declaration(ctx, init);
        } else {
            lower_expr(ctx, init);
        }
    }
    jump_to(ctx, test_block);

    open_block(ctx, test_block);
    if (condition) {
        branch_to(ctx, lower_expr(ctx, condition), body_block, done_block);
    } else {
        jump_to(ctx, body_block);       /* `for (;;)` has no way out but break */
    }

    open_block(ctx, body_block);
    push_loop(ctx, done_block, post_block);
    lower_statement(ctx, node->right);
    pop_loop(ctx);
    jump_to(ctx, post_block);

    open_block(ctx, post_block);
    if (post) {
        lower_expr(ctx, post);
    }
    jump_to(ctx, test_block);

    open_block(ctx, done_block);
}

/*
 * Give every case of this switch a block, without descending into a nested
 * switch, which owns its own cases. The blocks are recorded on the AST node so
 * the body can find them again when it reaches the label.
 */
static void collect_cases(struct ast_node *node, struct ast_node **cases,
    int *count, int capacity, struct ast_node **default_case)
{
    if (!node || node->type == AST_SWITCH) {
        return;
    }
    if (node->type == AST_CASE) {
        if (*count < capacity) {
            cases[(*count)++] = node;
        }
        collect_cases(node->left, cases, count, capacity, default_case);
        return;
    }
    if (node->type == AST_DEFAULT) {
        *default_case = node;
        collect_cases(node->left, cases, count, capacity, default_case);
        return;
    }
    collect_cases(node->left, cases, count, capacity, default_case);
    collect_cases(node->right, cases, count, capacity, default_case);
}

/* Record a block for a case label and stamp its index on the node. */
static void note_case_block(struct lower_ctx *ctx, struct ast_node *node,
    struct ir_block *block)
{
    grow_array((void **)&ctx->case_blocks, ctx->case_block_count,
        &ctx->case_block_capacity, sizeof(*ctx->case_blocks), 16,
        "IR case table");
    ctx->case_blocks[ctx->case_block_count] = block;
    node->string_label = ctx->case_block_count++;
}

static struct ir_block *case_block(struct lower_ctx *ctx, struct ast_node *node)
{
    if (node->string_label < 0 || node->string_label >= ctx->case_block_count) {
        return NULL;
    }
    return ctx->case_blocks[node->string_label];
}

/*
 * A switch becomes a chain of equality tests. That is what the existing back
 * end emits, and a jump table is a decision for instruction selection to make
 * later from the case values -- not something to bake into the IR here.
 */
static void lower_switch(struct lower_ctx *ctx, struct ast_node *node)
{
    struct ast_node *cases[256];
    struct ast_node *default_case = NULL;
    struct ir_block *done_block;
    struct ir_block *default_block;
    struct ir_value *control;
    struct ir_block *saved_switch_break = ctx->switch_break;
    int count = 0;
    int i;

    collect_cases(node->right, cases, &count,
        (int)(sizeof(cases) / sizeof(cases[0])), &default_case);

    control = lower_expr(ctx, node->left);
    done_block = new_block(ctx, NULL);

    for (i = 0; i < count; i++) {
        struct ir_block *body = new_block(ctx, NULL);
        struct ir_block *next_test = new_block(ctx, NULL);
        struct ir_value *label_value = emit_const(ctx,
            cases[i]->value ? strtoll(cases[i]->value, NULL, 0) : 0, ty_int);
        struct ir_value *matches =
            emit_binary(ctx, IR_EQ, control, label_value, ty_int);

        note_case_block(ctx, cases[i], body);
        branch_to(ctx, matches, body, next_test);
        open_block(ctx, next_test);
    }

    if (default_case) {
        default_block = new_block(ctx, NULL);
        note_case_block(ctx, default_case, default_block);
    } else {
        default_block = done_block;
    }
    jump_to(ctx, default_block);

    ctx->switch_break = done_block;
    push_loop(ctx, done_block, ctx->loop_depth > 0
        ? ctx->loops[ctx->loop_depth - 1].continue_block : done_block);

    /*
     * The body is lowered into a block nothing branches to. Every case within
     * it opens its own block, so control only arrives through the tests above
     * -- which is what makes fallthrough work: reaching the end of one case
     * without a break simply runs on into the next block.
     */
    open_block(ctx, new_block(ctx, NULL));
    lower_statement(ctx, node->right);
    jump_to(ctx, done_block);

    pop_loop(ctx);
    ctx->switch_break = saved_switch_break;
    open_block(ctx, done_block);
}

static void lower_return(struct lower_ctx *ctx, struct ast_node *node)
{
    struct ir_instr *instr;
    struct ir_value *value;

    if (ir_terminator(ctx->block)) {
        return;
    }
    /*
     * The operand is lowered before the return is emitted. Emitting first and
     * filling in the operand afterwards would append the instructions that
     * compute it after the terminator, where nothing would ever run them.
     */
    value = node->left ? lower_expr(ctx, node->left) : NULL;
    instr = ir_emit(ctx->block, IR_RET);
    instr->location = node->location;
    if (value) {
        ir_add_arg(instr, value);
    }

    /*
     * Anything after a return is unreachable, but it still has to lower
     * somewhere: statements are visited in source order and the next one needs
     * an insertion point. It goes into a block with no predecessors, which the
     * CFG pass then drops.
     */
    open_block(ctx, new_block(ctx, NULL));
}

static void lower_statement(struct lower_ctx *ctx, struct ast_node *node)
{
    if (!node) {
        return;
    }

    switch (node->type) {
        case AST_BLOCK:
            lower_statement(ctx, node->left);
            break;
        case AST_STATEMENT_LIST:
            lower_statement(ctx, node->left);
            lower_statement(ctx, node->right);
            break;
        case AST_DECL:
            lower_declaration(ctx, node);
            break;
        case AST_EXPR_STMT:
            lower_expr(ctx, node->left);
            break;
        case AST_RETURN:
            lower_return(ctx, node);
            break;
        case AST_IF:
            lower_if(ctx, node);
            break;
        case AST_WHILE:
            lower_while(ctx, node);
            break;
        case AST_DO_WHILE:
            lower_do_while(ctx, node);
            break;
        case AST_FOR:
            lower_for(ctx, node);
            break;
        case AST_SWITCH:
            lower_switch(ctx, node);
            break;
        case AST_CASE:
        case AST_DEFAULT: {
            /* The block was created and recorded while the tests were lowered. */
            struct ir_block *block = case_block(ctx, node);

            if (block) {
                jump_to(ctx, block);
                open_block(ctx, block);
            }
            lower_statement(ctx, node->left);
            break;
        }
        case AST_LABEL: {
            struct ir_block *block = block_for_label(ctx, node->value);

            jump_to(ctx, block);
            open_block(ctx, block);
            lower_statement(ctx, node->left);
            break;
        }
        case AST_GOTO:
            jump_to(ctx, block_for_label(ctx, node->value));
            open_block(ctx, new_block(ctx, NULL));
            break;
        case AST_BREAK:
            if (ctx->loop_depth > 0) {
                jump_to(ctx, ctx->loops[ctx->loop_depth - 1].break_block);
                open_block(ctx, new_block(ctx, NULL));
            }
            break;
        case AST_CONTINUE:
            if (ctx->loop_depth > 0) {
                jump_to(ctx, ctx->loops[ctx->loop_depth - 1].continue_block);
                open_block(ctx, new_block(ctx, NULL));
            }
            break;
        case AST_EMPTY:
            break;
        default:
            /* Anything left is an expression used for its effect. */
            lower_expr(ctx, node);
            break;
    }
}

/* ------------------------------------------------------------ functions -- */

static void lower_function(struct ir_program *program, struct ast_node *node)
{
    struct lower_ctx ctx;
    struct ir_func *func;
    struct ast_node *parameter;
    int index = 0;
    int i;

    memset(&ctx, 0, sizeof(ctx));
    func = ir_func_new(program, node->value ? node->value : "?", node->sym);
    func->return_type = node->ty;
    ctx.func = func;
    ctx.block = ir_block_new(func, "entry");

    /*
     * A parameter arrives as a value and is immediately stored to a slot, so
     * the body can treat it like any other local -- including assigning to it,
     * which C allows. The promotion pass undoes the round trip whenever the
     * address is never taken, so the store costs nothing in the end.
     */
    for (parameter = node->left; parameter; parameter = parameter->right) {
        struct ast_node *declaration = parameter->left;
        struct ir_instr *instr;
        struct ir_value *incoming;
        struct ir_value *slot;

        if (!declaration || !declaration->sym) {
            continue;
        }
        instr = ir_emit(ctx.block, IR_PARAM);
        instr->sym = declaration->sym;
        instr->imm = index++;
        incoming = ir_set_dst(func, instr,
            declaration->ty ? declaration->ty : ty_int);

        slot = slot_for(&ctx, declaration->sym);
        emit_store(&ctx, slot, incoming,
            declaration->ty ? declaration->ty : ty_int);
    }

    if (node->right) {
        lower_statement(&ctx, node->right->left);
    }

    /*
     * A function that runs off the end still has to leave. C says the value is
     * undefined unless this is main, and the back end is what decides what
     * ends up in the return register -- the IR only has to be well formed.
     */
    if (!ir_terminator(ctx.block)) {
        ir_emit(ctx.block, IR_RET);
    }

    for (i = 0; i < ctx.label_count; i++) {
        free(ctx.labels[i].name);
    }
    free(ctx.labels);
    free(ctx.loops);
    free(ctx.slots);
    free(ctx.case_blocks);
}

void ir_lower_program(struct ir_program *program, struct ast_node *ast)
{
    if (!ast) {
        return;
    }

    switch (ast->type) {
        case AST_PROGRAM:
        case AST_FUNCTION_LIST:
            ir_lower_program(program, ast->left);
            ir_lower_program(program, ast->right);
            break;
        case AST_FUNCTION:
            lower_function(program, ast);
            break;
        default:
            /* Globals, prototypes and struct definitions carry no code. */
            break;
    }
}
