/*
 * Constant folding, algebraic identities, and strength reduction.
 *
 * All three are the same shape of work -- look at one instruction, decide it
 * can be written more cheaply, rewrite it -- so they share a pass rather than
 * making three walks over the function.
 *
 * There is no separate constant *propagation* pass, because SSA does not need
 * one. An operand is a pointer to the instruction that defines it, so once
 * `%3 = add 2, 3` has been rewritten in place to `%3 = const 5`, every use of
 * %3 is already looking at a constant. The propagation is the representation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opt.h"

/*
 * Reduce a folded result to what the target type would actually hold.
 *
 * The arithmetic above is done in 64 bits, but `(char)200 + (char)100` does not
 * produce 300 in a char, and a compiler that folded it to 300 would disagree
 * with the same expression evaluated at run time. Folding has to agree with not
 * folding.
 */
static long long truncate_to(long long value, struct Type *ty)
{
    if (!ty) {
        return value;
    }
    switch (ty->size) {
        case 1:
            return ty->is_unsigned ? (long long)(unsigned char)value
                                   : (long long)(signed char)value;
        case 2:
            return ty->is_unsigned ? (long long)(unsigned short)value
                                   : (long long)(short)value;
        case 4:
            return ty->is_unsigned ? (long long)(unsigned int)value
                                   : (long long)(int)value;
        default:
            return value;
    }
}

static int is_const(struct ir_value *value)
{
    return value && value->def && value->def->op == IR_CONST;
}

static long long const_of(struct ir_value *value)
{
    return value->def->imm;
}

static int is_const_equal(struct ir_value *value, long long number)
{
    return is_const(value) && const_of(value) == number;
}

/* Turn an instruction into a constant in place, keeping its result value. */
static void become_const(struct ir_instr *instr, long long value)
{
    instr->op = IR_CONST;
    instr->imm = truncate_to(value, instr->dst ? instr->dst->ty : NULL);
    instr->arg_count = 0;
}

/* The bit position of a power of two, or -1 if it is not one. */
static int power_of_two(long long value)
{
    int bit = 0;

    if (value <= 0 || (value & (value - 1)) != 0) {
        return -1;
    }
    while ((value >> bit) != 1) {
        bit++;
    }
    return bit;
}

static int is_unsigned(struct ir_instr *instr)
{
    return instr->dst && instr->dst->ty && instr->dst->ty->is_unsigned;
}

/*
 * Fold a binary operation on two constants. Returns 0 when the operation has
 * no defined answer -- division by zero -- so the instruction is left alone
 * and the program still traps at run time exactly where it would have.
 */
static int fold_binary(IROp op, long long left, long long right,
    int unsigned_op, long long *result)
{
    unsigned long long ul = (unsigned long long)left;
    unsigned long long ur = (unsigned long long)right;

    switch (op) {
        case IR_ADD: *result = left + right; return 1;
        case IR_SUB: *result = left - right; return 1;
        case IR_MUL: *result = left * right; return 1;
        case IR_DIV:
            if (right == 0) {
                return 0;
            }
            *result = unsigned_op ? (long long)(ul / ur) : left / right;
            return 1;
        case IR_MOD:
            if (right == 0) {
                return 0;
            }
            *result = unsigned_op ? (long long)(ul % ur) : left % right;
            return 1;
        case IR_SHL:
            /*
             * A shift by more than the width is undefined in C, so folding it
             * would be inventing an answer. Left alone, it stays whatever the
             * machine does with it.
             */
            if (right < 0 || right >= 64) {
                return 0;
            }
            *result = (long long)(ul << right);
            return 1;
        case IR_SHR:
            if (right < 0 || right >= 64) {
                return 0;
            }
            *result = unsigned_op ? (long long)(ur >> right) : left >> right;
            return 1;
        case IR_AND: *result = left & right; return 1;
        case IR_OR:  *result = left | right; return 1;
        case IR_XOR: *result = left ^ right; return 1;
        case IR_EQ:  *result = left == right; return 1;
        case IR_NE:  *result = left != right; return 1;
        case IR_LT:  *result = unsigned_op ? ul <  ur : left <  right; return 1;
        case IR_LE:  *result = unsigned_op ? ul <= ur : left <= right; return 1;
        case IR_GT:  *result = unsigned_op ? ul >  ur : left >  right; return 1;
        case IR_GE:  *result = unsigned_op ? ul >= ur : left >= right; return 1;
        default:     return 0;
    }
}

/*
 * Whether replacing an instruction with one of its operands would change the
 * type the uses see. Lowering scales pointer arithmetic through operands wider
 * than the expression they came from, so `x * 1` can be a long whose x is an
 * int: handing the int back would quietly drop a widening.
 */
static int same_type(struct ir_instr *instr, struct ir_value *operand)
{
    return instr->dst && operand && instr->dst->ty == operand->ty;
}

/*
 * Identities that hold whatever the operand is. Each replaces the instruction
 * with one of its operands, or with a constant, without needing to know what
 * the other side computes.
 */
static int simplify_identity(struct ir_func *func, struct ir_instr *instr)
{
    struct ir_value *left = instr->arg_count > 0 ? instr->args[0] : NULL;
    struct ir_value *right = instr->arg_count > 1 ? instr->args[1] : NULL;

    if (!left || !right) {
        return 0;
    }

    switch (instr->op) {
        case IR_ADD:
        case IR_SUB:
        case IR_OR:
        case IR_XOR:
        case IR_SHL:
        case IR_SHR:
            if (is_const_equal(right, 0) && same_type(instr, left)) {
                ir_replace_uses(func, instr->dst, left);
                return 1;
            }
            /* Only addition and or are commutative among these. */
            if (is_const_equal(left, 0) && same_type(instr, right) &&
                (instr->op == IR_ADD || instr->op == IR_OR ||
                 instr->op == IR_XOR)) {
                ir_replace_uses(func, instr->dst, right);
                return 1;
            }
            break;
        case IR_MUL:
            if (is_const_equal(right, 1) && same_type(instr, left)) {
                ir_replace_uses(func, instr->dst, left);
                return 1;
            }
            if (is_const_equal(left, 1) && same_type(instr, right)) {
                ir_replace_uses(func, instr->dst, right);
                return 1;
            }
            if (is_const_equal(right, 0) || is_const_equal(left, 0)) {
                become_const(instr, 0);
                return 1;
            }
            break;
        case IR_DIV:
            if (is_const_equal(right, 1) && same_type(instr, left)) {
                ir_replace_uses(func, instr->dst, left);
                return 1;
            }
            break;
        case IR_AND:
            if (is_const_equal(right, 0) || is_const_equal(left, 0)) {
                become_const(instr, 0);
                return 1;
            }
            if (left == right && same_type(instr, left)) {
                ir_replace_uses(func, instr->dst, left);
                return 1;
            }
            break;
        default:
            break;
    }

    /*
     * Comparisons and subtraction of a value with itself. This is worth doing
     * even though the source rarely says `x - x`: after copy propagation and
     * common subexpression elimination, two different-looking expressions
     * often turn out to be the same value.
     */
    if (left == right) {
        switch (instr->op) {
            case IR_SUB:
            case IR_XOR:
                become_const(instr, 0);
                return 1;
            case IR_OR:
                if (!same_type(instr, left)) {
                    return 0;
                }
                ir_replace_uses(func, instr->dst, left);
                return 1;
            case IR_EQ:
            case IR_LE:
            case IR_GE:
                become_const(instr, 1);
                return 1;
            case IR_NE:
            case IR_LT:
            case IR_GT:
                become_const(instr, 0);
                return 1;
            default:
                break;
        }
    }
    return 0;
}

/*
 * Replace an expensive operation with a cheap one that computes the same
 * thing: multiplication and division by a power of two become shifts, and a
 * remainder by one becomes a mask.
 *
 * Signed division is deliberately left alone. C rounds it toward zero and an
 * arithmetic right shift rounds toward negative infinity, so `-7 / 2` is -3
 * but `-7 >> 1` is -4. Getting that right needs a rounding correction, which
 * is the back end's business once it is choosing instructions.
 */
static int strength_reduce(struct ir_func *func, struct ir_instr *instr)
{
    struct ir_value *right = instr->arg_count > 1 ? instr->args[1] : NULL;
    struct ir_instr *replacement;
    long long divisor;
    long long operand;
    IROp op;
    int bit;

    if (!right || !is_const(right)) {
        return 0;
    }
    divisor = const_of(right);
    bit = power_of_two(divisor);
    if (bit <= 0) {
        return 0;               /* not a power of two, or a shift by nothing */
    }

    switch (instr->op) {
        case IR_MUL:
            op = IR_SHL;
            operand = bit;
            break;
        case IR_DIV:
            if (!is_unsigned(instr)) {
                return 0;
            }
            op = IR_SHR;
            operand = bit;
            break;
        case IR_MOD:
            if (!is_unsigned(instr)) {
                return 0;
            }
            /* x % 2^k is the low k bits of x, for unsigned x. */
            op = IR_AND;
            operand = divisor - 1;
            break;
        default:
            return 0;
    }

    replacement = ir_emit_before(instr, IR_CONST);
    replacement->imm = operand;
    instr->op = op;
    instr->args[1] = ir_set_dst(func, replacement, right->ty);
    return 1;
}

int opt_fold(struct ir_func *func)
{
    struct ir_block *block;
    int changes = 0;

    for (block = func->entry; block; block = block->next) {
        struct ir_instr *instr;

        if (block->rpo_index < 0) {
            continue;
        }
        for (instr = block->first; instr; instr = instr->next) {
            long long result;

            if (!instr->dst) {
                continue;
            }

            if (instr->arg_count == 2 && is_const(instr->args[0]) &&
                is_const(instr->args[1]) &&
                fold_binary(instr->op, const_of(instr->args[0]),
                    const_of(instr->args[1]),
                    instr->args[0]->ty && instr->args[0]->ty->is_unsigned,
                    &result)) {
                become_const(instr, result);
                changes++;
                continue;
            }

            if (instr->arg_count == 1 && is_const(instr->args[0])) {
                long long operand = const_of(instr->args[0]);

                switch (instr->op) {
                    case IR_NEG:        become_const(instr, -operand); break;
                    case IR_COMPLEMENT: become_const(instr, ~operand); break;
                    case IR_LOGNOT:     become_const(instr, !operand); break;
                    case IR_CAST:       become_const(instr, operand);  break;
                    default:            continue;
                }
                changes++;
                continue;
            }

            /*
             * A cast to the type a value already has computes nothing. This
             * appears constantly after promotion, where the same variable is
             * read and converted to its own type on the way into an
             * expression.
             */
            if (instr->op == IR_CAST && instr->arg_count == 1 &&
                instr->args[0]->ty == instr->dst->ty) {
                ir_replace_uses(func, instr->dst, instr->args[0]);
                changes++;
                continue;
            }

            if (simplify_identity(func, instr)) {
                changes++;
                continue;
            }
            if (strength_reduce(func, instr)) {
                changes++;
            }
        }
    }
    return changes;
}
