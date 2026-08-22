#!/usr/bin/env sh
set -eu

cc="${CC:-gcc}"
build_dir="${BUILD_DIR:-build}"
compiler="${build_dir}/donkey"

# Set UPDATE_GOLDEN=1 to regenerate tests/golden/*.asm and tests/expected/*.txt
# after an intentional change. Always review the resulting diff before
# committing it -- that diff is the only thing standing between a codegen fix
# and a codegen regression.
update_golden="${UPDATE_GOLDEN:-0}"

# Set SKIP_RUN=1 to check compilation, golden assembly, and diagnostics without
# assembling or executing anything. Codegen still emits MinGW-style `_main`
# symbols, so linking only works on i686 Windows until the x86-64 System V
# backend lands; this lets other platforms exercise the rest of the compiler.
skip_run="${SKIP_RUN:-0}"

golden_dir=tests/golden
expected_dir=tests/expected

# Every program compiled, assembled, linked against tests/harness.c, and run.
# Each entry is "<source path> <name>".
PROGRAMS="
examples/sample.c sample
examples/unary.c unary
examples/operators.c operators
examples/assignment.c assignment
examples/short_circuit.c short_circuit
examples/locals.c locals
examples/multiple_functions.c multiple_functions
examples/control_flow.c control_flow
examples/missing_ops.c missing_ops
examples/casts.c casts
examples/comments.c comments
examples/globals.c globals
examples/types.c types
examples/pointers_arrays.c pointers_arrays
examples/pointer_arithmetic.c pointer_arithmetic
examples/global_arrays.c global_arrays
examples/advanced_types.c advanced_types
examples/wide_values.c wide_values
examples/char_arrays.c char_arrays
examples/struct_layout.c struct_layout
examples/struct_arrays.c struct_arrays
tests/semantic/valid_forward_call.c valid_forward_call
"

rm -rf "$build_dir"
mkdir -p "$build_dir"
mkdir -p "$golden_dir" "$expected_dir"

# Honour CFLAGS so CI can rebuild the compiler under ASan/UBSan.
cflags="${CFLAGS:--Wall -Wextra -g}"

# shellcheck disable=SC2086 # cflags is a deliberate word-split flag list
"$cc" -Iinclude $cflags -o "$compiler" \
    src/main.c src/lexer.c src/parser.c src/semantic.c src/codegen.c src/type.c

failures=0

echo "== unit tests =="
# shellcheck disable=SC2086
"$cc" -Iinclude $cflags -o "$build_dir/test_type" tests/unit/test_type.c src/type.c
"$build_dir/test_type"

fail() {
    echo "FAIL: $*" >&2
    failures=$((failures + 1))
}

# Compare a produced file against its golden copy, or refresh the golden copy
# when UPDATE_GOLDEN=1.
check_golden() {
    actual="$1"
    golden="$2"
    label="$3"

    if [ "$update_golden" = "1" ]; then
        cp "$actual" "$golden"
        return 0
    fi

    if [ ! -f "$golden" ]; then
        fail "$label: no golden file at $golden (run with UPDATE_GOLDEN=1 to create it)"
        return 1
    fi

    if ! diff -u "$golden" "$actual" >"$build_dir/diff.txt" 2>&1; then
        fail "$label: output differs from $golden"
        head -40 "$build_dir/diff.txt" >&2
        return 1
    fi

    return 0
}

echo "== compiling, assembling, and running examples =="

# Fed by redirection rather than a pipe, so the loop runs in this shell and
# its failure count survives.
printf '%s\n' "$PROGRAMS" > "$build_dir/programs.txt"

while read -r source name; do
    [ -n "$source" ] || continue

    asm="$build_dir/$name.asm"
    wrapped="$build_dir/$name.wrapped.s"
    exe="$build_dir/$name.exe"
    stdout_file="$build_dir/$name.out"

    "$compiler" "$source" "$asm" >/dev/null

    # Golden assembly: guards every example against codegen regressions, not
    # just examples/sample.c as the previous suite did.
    check_golden "$asm" "$golden_dir/$name.asm" "$name (asm)" || continue

    if [ "$skip_run" = "1" ]; then
        echo "  ok  $name (compile only)"
        continue
    fi

    # Rename the example's entry point so the harness can own `main`.
    # \b_main\b avoids touching identifiers that merely end in _main.
    sed 's/\b_main\b/_donkey_main/g' "$asm" > "$wrapped"

    "$cc" -x assembler "$wrapped" -x c tests/harness.c -o "$exe"

    "$exe" > "$stdout_file"

    # Golden stdout: full 32-bit values, unlike the exit codes this replaced.
    check_golden "$stdout_file" "$expected_dir/$name.txt" "$name (stdout)" || continue

    echo "  ok  $name"
done < "$build_dir/programs.txt"

echo "== negative tests =="

expect_error() {
    input="$1"
    expected="$2"
    diagnostics="$build_dir/errors.txt"

    if "$compiler" "$input" "$build_dir/invalid.asm" >/dev/null 2>"$diagnostics"; then
        fail "expected compiler to reject $input"
        return 1
    fi
    if ! grep -F "$expected" "$diagnostics" >/dev/null; then
        fail "expected diagnostic '$expected' for $input"
        cat "$diagnostics" >&2
        return 1
    fi
    echo "  ok  $(basename "$input")"
}

expect_error tests/syntax/missing_semicolon.c "Parse error at tests/syntax/missing_semicolon.c:4:1: expected ';', found '}'"
expect_error tests/syntax/invalid_character.c "Lex error at tests/syntax/invalid_character.c:3:12: invalid character '@'"
expect_error tests/semantic/undeclared_variable.c "Semantic error at tests/semantic/undeclared_variable.c:3:12 in function 'main': use of undeclared variable 'missing'"
expect_error tests/semantic/wrong_argument_count.c "expects 2 argument(s), but 1 provided"
expect_error tests/semantic/duplicate_declaration.c "duplicate declaration of 'value'"
expect_error tests/semantic/break_outside_loop.c "'break' statement is not inside a loop"
expect_error tests/semantic/shadowing.c "variable shadowing is not supported for 'value'"
expect_error tests/semantic/call_shadowed_function.c "called object 'helper' is not a function"
expect_error tests/semantic/invalid_pointer_assignment.c "cannot assign int to int*"
expect_error tests/semantic/invalid_dereference.c "cannot dereference non-pointer expression"
expect_error tests/semantic/invalid_pointer_addition.c "invalid operands to pointer arithmetic"
expect_error tests/semantic/too_many_array_initializers.c "too many initializers for array 'values'"
expect_error tests/semantic/invalid_pointer_subtraction.c "cannot subtract incompatible pointer types"
expect_error tests/limits/global_array_too_long.c "exceeds the supported length of 256"

if [ "$failures" -ne 0 ]; then
    echo "$failures check(s) failed." >&2
    exit 1
fi

if [ "$update_golden" = "1" ]; then
    echo "Golden files regenerated. Review the diff before committing."
else
    echo "All compiler checks passed."
fi
