#!/usr/bin/env sh
set -eu

cc="${CC:-gcc}"
build_dir="${BUILD_DIR:-build}"
compiler="${build_dir}/donkey"

rm -rf "$build_dir"
mkdir -p "$build_dir"

"$cc" -Iinclude -Wall -Wextra -g -o "$compiler" \
    src/main.c src/lexer.c src/parser.c src/semantic.c src/codegen.c

"$compiler" examples/sample.c "$build_dir/sample.asm"
"$compiler" examples/unary.c "$build_dir/unary.asm"
"$compiler" examples/operators.c "$build_dir/operators.asm"
"$compiler" examples/assignment.c "$build_dir/assignment.asm"
"$compiler" examples/short_circuit.c "$build_dir/short_circuit.asm"
"$compiler" examples/locals.c "$build_dir/locals.asm"
"$compiler" examples/multiple_functions.c "$build_dir/multiple_functions.asm"
"$compiler" examples/control_flow.c "$build_dir/control_flow.asm"
"$compiler" examples/missing_ops.c "$build_dir/missing_ops.asm"
"$compiler" examples/casts.c "$build_dir/casts.asm"
"$compiler" examples/comments.c "$build_dir/comments.asm"
"$compiler" examples/globals.c "$build_dir/globals.asm"
"$compiler" examples/types.c "$build_dir/types.asm"
"$compiler" tests/semantic/valid_forward_call.c "$build_dir/valid_forward_call.asm"

expect_semantic_error() {
    input="$1"
    expected="$2"
    diagnostics="$build_dir/semantic-errors.txt"

    if "$compiler" "$input" "$build_dir/invalid.asm" 2>"$diagnostics"; then
        echo "Expected semantic analysis to reject $input" >&2
        exit 1
    fi
    if ! grep -F "$expected" "$diagnostics" >/dev/null; then
        echo "Expected diagnostic '$expected' for $input" >&2
        cat "$diagnostics" >&2
        exit 1
    fi
}

expect_semantic_error tests/semantic/undeclared_variable.c "use of undeclared variable 'missing'"
expect_semantic_error tests/semantic/wrong_argument_count.c "expects 2 argument(s), but 1 provided"
expect_semantic_error tests/semantic/duplicate_declaration.c "duplicate declaration of 'value'"
expect_semantic_error tests/semantic/break_outside_loop.c "'break' statement is not inside a loop"
expect_semantic_error tests/semantic/shadowing.c "variable shadowing is not supported for 'value'"
expect_semantic_error tests/semantic/call_shadowed_function.c "called object 'helper' is not a function"

awk '
    NR == FNR {
        expected[NR] = $0
        expected_count = NR
        next
    }
    {
        actual_count = FNR
        if ($0 != expected[FNR]) {
            printf("sample.asm mismatch on line %d\nexpected: %s\nactual:   %s\n", FNR, expected[FNR], $0) > "/dev/stderr"
            exit 1
        }
    }
    END {
        if (expected_count != actual_count) {
            printf("sample.asm line count mismatch: expected %d, got %d\n", expected_count, actual_count) > "/dev/stderr"
            exit 1
        }
    }
' examples/sample.asm "$build_dir/sample.asm"

"$cc" -x assembler "$build_dir/sample.asm" -o "$build_dir/sample.exe"
"$cc" -x assembler "$build_dir/unary.asm" -o "$build_dir/unary.exe"
"$cc" -x assembler "$build_dir/operators.asm" -o "$build_dir/operators.exe"
"$cc" -x assembler "$build_dir/assignment.asm" -o "$build_dir/assignment.exe"
"$cc" -x assembler "$build_dir/short_circuit.asm" -o "$build_dir/short_circuit.exe"
"$cc" -x assembler "$build_dir/locals.asm" -o "$build_dir/locals.exe"
"$cc" -x assembler "$build_dir/multiple_functions.asm" -o "$build_dir/multiple_functions.exe"
"$cc" -x assembler "$build_dir/control_flow.asm" -o "$build_dir/control_flow.exe"
"$cc" -x assembler "$build_dir/missing_ops.asm" -o "$build_dir/missing_ops.exe"
"$cc" -x assembler "$build_dir/casts.asm" -o "$build_dir/casts.exe"
"$cc" -x assembler "$build_dir/comments.asm" -o "$build_dir/comments.exe"
"$cc" -x assembler "$build_dir/globals.asm" -o "$build_dir/globals.exe"
"$cc" -x assembler "$build_dir/types.asm" -o "$build_dir/types.exe"
"$cc" -x assembler "$build_dir/valid_forward_call.asm" -o "$build_dir/valid_forward_call.exe"

run_and_expect() {
    exe="$1"
    expected="$2"

    set +e
    "$exe"
    actual="$?"
    set -e

    if [ "$actual" -ne "$expected" ]; then
        echo "Expected $exe to exit $expected, got $actual" >&2
        exit 1
    fi
}

run_and_expect "$build_dir/sample.exe" 14
run_and_expect "$build_dir/unary.exe" 6
run_and_expect "$build_dir/operators.exe" 1
run_and_expect "$build_dir/assignment.exe" 15
run_and_expect "$build_dir/short_circuit.exe" 1
run_and_expect "$build_dir/locals.exe" 14
run_and_expect "$build_dir/multiple_functions.exe" 16
run_and_expect "$build_dir/control_flow.exe" 16
run_and_expect "$build_dir/missing_ops.exe" 52
run_and_expect "$build_dir/casts.exe" 29
run_and_expect "$build_dir/comments.exe" 12
run_and_expect "$build_dir/globals.exe" 21
run_and_expect "$build_dir/types.exe" 162
run_and_expect "$build_dir/valid_forward_call.exe" 5

echo "All compiler checks passed."
