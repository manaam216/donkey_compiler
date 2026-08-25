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

# Donkey emits x86-64 System V assembly, so assembling and running the output
# needs a matching host toolchain. Where there is not one -- a 32-bit MinGW
# development box, say -- the suite still compiles every example, diffs the
# golden assembly, and checks every diagnostic; it just cannot execute.
# Set SKIP_RUN=1 to force that mode, or 0 to insist on running.
if [ -n "${SKIP_RUN:-}" ]; then
    skip_run="$SKIP_RUN"
elif "$cc" -dumpmachine 2>/dev/null | grep -q "^x86_64.*linux"; then
    skip_run=0
else
    skip_run=1
fi

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
examples/shadowing.c shadowing
examples/many_args.c many_args
tests/semantic/valid_forward_call.c valid_forward_call
"

rm -rf "$build_dir"
mkdir -p "$build_dir"
mkdir -p "$golden_dir" "$expected_dir"

# Honour CFLAGS so CI can rebuild the compiler under ASan/UBSan.
cflags="${CFLAGS:--Wall -Wextra -g}"

# shellcheck disable=SC2086 # cflags is a deliberate word-split flag list
"$cc" -Iinclude $cflags -o "$compiler" \
    src/main.c src/lexer.c src/parser.c src/semantic.c src/codegen.c src/type.c src/symbol.c src/diag.c src/dump.c src/cli.c

failures=0

fail() {
    echo "FAIL: $*" >&2
    failures=$((failures + 1))
}

echo "== unit tests =="

# Each suite exercises compiler stages in process, so it can check things the
# generated assembly never shows: type layout, token positions, option parsing.
# Some suites deliberately provoke diagnostics and usage messages, so their
# output is captured and only the summary shown -- everything on failure.
run_unit() {
    name="$1"
    shift
    # shellcheck disable=SC2086
    "$cc" -Iinclude $cflags -o "$build_dir/$name" "tests/unit/$name.c" "$@"
    if "$build_dir/$name" >"$build_dir/$name.log" 2>&1; then
        # The summary, not the last line: provoked diagnostics interleave.
        grep -E "checks passed|check\(s\) failed" "$build_dir/$name.log" | tail -1
    else
        fail "$name"
        cat "$build_dir/$name.log" >&2
    fi
}

run_unit test_type src/type.c
run_unit test_lexer src/lexer.c src/diag.c
run_unit test_cli src/cli.c

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

    "$compiler" "$source" -o "$asm" >/dev/null

    # Golden assembly: guards every example against codegen regressions, not
    # just examples/sample.c as the previous suite did.
    check_golden "$asm" "$golden_dir/$name.asm" "$name (asm)" || continue

    if [ "$skip_run" = "1" ]; then
        echo "  ok  $name (compile only)"
        continue
    fi


    # Rename the example's entry point so the harness can own `main`.
    # \bmain\b avoids touching identifiers that merely contain "main".
    sed 's/\bmain\b/donkey_main/g' "$asm" > "$wrapped"

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

    if "$compiler" "$input" -o "$build_dir/invalid.asm" >/dev/null 2>"$diagnostics"; then
        fail "expected compiler to reject $input"
        return 1
    fi
    if ! grep -F -- "$expected" "$diagnostics" >/dev/null; then
        fail "expected diagnostic '$expected' for $input"
        cat "$diagnostics" >&2
        return 1
    fi
    echo "  ok  $(basename "$input")"
}

# Recovery: check that a file yields at least N distinct errors, so one problem
# does not mask the rest.
expect_error_count() {
    input="$1"
    minimum="$2"
    diagnostics="$build_dir/errors.txt"

    if "$compiler" "$input" -o "$build_dir/invalid.asm" >/dev/null 2>"$diagnostics"; then
        fail "expected compiler to reject $input"
        return 1
    fi
    count=$(grep -c ": error: " "$diagnostics" || true)
    if [ "$count" -lt "$minimum" ]; then
        fail "expected at least $minimum errors from $input, got $count"
        cat "$diagnostics" >&2
        return 1
    fi
    echo "  ok  $(basename "$input") ($count errors reported)"
}

# A warning is reported but must not fail the build.
expect_warning() {
    input="$1"
    expected="$2"
    diagnostics="$build_dir/warnings.txt"

    if ! "$compiler" "$input" -o "$build_dir/warned.asm" >/dev/null 2>"$diagnostics"; then
        fail "$input should compile despite warnings"
        cat "$diagnostics" >&2
        return 1
    fi
    if ! grep -F -- "$expected" "$diagnostics" >/dev/null; then
        fail "expected warning '$expected' for $input"
        cat "$diagnostics" >&2
        return 1
    fi
    echo "  ok  $(basename "$input") (warned, still compiled)"
}

expect_error tests/syntax/missing_semicolon.c "tests/syntax/missing_semicolon.c:4:1: error: expected ';', found '}'"
expect_error tests/syntax/invalid_character.c "tests/syntax/invalid_character.c:3:12: error: invalid character '@'"
expect_error tests/semantic/undeclared_variable.c "tests/semantic/undeclared_variable.c:3:12: error: use of undeclared variable 'missing'"
# The quoted source line and caret under the offending column.
expect_error tests/semantic/undeclared_variable.c "    return missing;"
expect_error tests/semantic/undeclared_variable.c "In function 'main':"
expect_error tests/semantic/wrong_argument_count.c "expects 2 argument(s), but 1 provided"
expect_error tests/semantic/duplicate_declaration.c "duplicate declaration of 'value'"
expect_error tests/semantic/break_outside_loop.c "'break' statement is not inside a loop"
expect_error tests/semantic/call_shadowed_function.c "called object 'helper' is not a function"
expect_error tests/semantic/invalid_pointer_assignment.c "cannot assign int to int*"
expect_error tests/semantic/invalid_dereference.c "cannot dereference non-pointer expression"
expect_error tests/semantic/invalid_pointer_addition.c "invalid operands to pointer arithmetic"
expect_error tests/semantic/too_many_array_initializers.c "too many initializers for array 'values'"
expect_error tests/semantic/invalid_pointer_subtraction.c "cannot subtract incompatible pointer types"
expect_error tests/limits/global_array_too_long.c "exceeds the supported length of 256"
expect_error_count tests/semantic/multiple_errors.c 4
expect_error_count tests/syntax/multiple_errors.c 2
expect_warning tests/semantic/unreachable_after_return.c "warning: unreachable statement after 'return'"

echo "== tooling =="

expect_output() {
    label="$1"
    expected="$2"
    shift 2

    if ! "$@" > "$build_dir/tool.txt" 2>&1; then
        fail "$label: command failed"
        cat "$build_dir/tool.txt" >&2
        return 1
    fi
    if ! grep -F -- "$expected" "$build_dir/tool.txt" >/dev/null; then
        fail "$label: expected to find '$expected'"
        head -20 "$build_dir/tool.txt" >&2
        return 1
    fi
    echo "  ok  $label"
}

expect_output "--dump-tokens" "identifier         main"     "$compiler" --dump-tokens examples/sample.c
# The dump runs after analysis, so nodes carry their types and storage.
expect_output "--dump-ast types" ": int"     "$compiler" --dump-ast examples/locals.c
expect_output "--dump-ast storage" "[local "     "$compiler" --dump-ast examples/locals.c
expect_output "--help" "--dump-tokens" "$compiler" --help
expect_output "--version" "donkey " "$compiler" --version

# A flag with nothing behind it is refused, not quietly ignored.
expect_rejected() {
    label="$1"
    expected="$2"
    shift 2

    if "$@" >"$build_dir/tool.txt" 2>&1; then
        fail "$label: should have been rejected"
        return 1
    fi
    if ! grep -F -- "$expected" "$build_dir/tool.txt" >/dev/null; then
        fail "$label: expected '$expected'"
        cat "$build_dir/tool.txt" >&2
        return 1
    fi
    echo "  ok  $label"
}

expect_rejected "-O2 rejected" "there is no optimiser yet"     "$compiler" -O2 examples/sample.c
expect_rejected "-E rejected" "there is no preprocessor yet"     "$compiler" -E examples/sample.c
expect_rejected "no input" "no input file" "$compiler"

# -Werror turns the warning into a failure; -w removes it.
expect_rejected "-Werror is fatal" "unreachable statement"     "$compiler" -Werror tests/semantic/unreachable_after_return.c -o "$build_dir/we.asm"
if "$compiler" -w tests/semantic/unreachable_after_return.c -o "$build_dir/w.asm"         2>"$build_dir/w.err" >/dev/null && [ ! -s "$build_dir/w.err" ]; then
    echo "  ok  -w silences the warning"
else
    fail "-w should silence the warning and still compile"
    cat "$build_dir/w.err" >&2
fi

if [ "$failures" -ne 0 ]; then
    echo "$failures check(s) failed." >&2
    exit 1
fi

if [ "$update_golden" = "1" ]; then
    echo "Golden files regenerated. Review the diff before committing."
else
    echo "All compiler checks passed."
fi
