#!/usr/bin/env sh
set -eu

cc="${CC:-gcc}"
build_dir="${BUILD_DIR:-build}"
compiler="${build_dir}/donkey"

rm -rf "$build_dir"
mkdir -p "$build_dir"

"$cc" -Iinclude -Wall -Wextra -g -o "$compiler" \
    src/main.c src/lexer.c src/parser.c src/codegen.c

"$compiler" examples/sample.c "$build_dir/sample.asm"
"$compiler" examples/unary.c "$build_dir/unary.asm"
"$compiler" examples/operators.c "$build_dir/operators.asm"
"$compiler" examples/assignment.c "$build_dir/assignment.asm"
"$compiler" examples/short_circuit.c "$build_dir/short_circuit.asm"

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
run_and_expect "$build_dir/unary.exe" 250
run_and_expect "$build_dir/operators.exe" 1
run_and_expect "$build_dir/assignment.exe" 15
run_and_expect "$build_dir/short_circuit.exe" 1

echo "All compiler checks passed."
