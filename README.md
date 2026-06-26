# Donkey Compiler

Donkey is a small educational compiler written in C. It accepts a tiny C-like
program, builds an abstract syntax tree, and emits 32-bit x86-style assembly for
integer-returning functions.

## Directory Layout

```text
.
|-- include/          Public compiler headers
|   |-- decl.h
|   `-- defs.h
|-- src/              Compiler implementation
|   |-- main.c        CLI entry point
|   |-- lexer.c       Tokenizer
|   |-- parser.c      Recursive descent parser and AST allocation
|   |-- semantic.c    Name, scope, and function-call validation
|   `-- codegen.c     Assembly generator
|-- examples/         Source examples and reference assembly
|   |-- sample.c
|   |-- sample.asm
|   |-- operators.c
|   |-- assignment.c
|   |-- short_circuit.c
|   |-- locals.c
|   |-- multiple_functions.c
|   |-- control_flow.c
|   |-- casts.c
|   |-- comments.c
|   |-- globals.c
|   |-- missing_ops.c
|   `-- unary.c
|-- build/            Generated binaries and assembly output
`-- Makefile
```

`build/` is intentionally ignored by Git. Rebuild it whenever you need fresh
artifacts.

## Build

With `make`:

```sh
make
```

The compiler binary is written to:

```text
build/donkey
```

On Windows with MinGW GCC and no `make`, run:

```powershell
New-Item -ItemType Directory -Force build
gcc -Iinclude -Wall -Wextra -g -o build\donkey.exe src\main.c src\lexer.c src\parser.c src\codegen.c
```

## Test

Run the project checks:

```sh
make test
```

The test script rebuilds the compiler, compiles every example in `examples/`,
assembles the generated files, runs the produced executables, and checks their
exit codes.

CI runs the same `make test` flow on GitHub Actions using Windows plus MSYS2
MINGW32, which matches the current `_main` assembly symbol convention.

## Run

Compile the main example:

```sh
./build/donkey examples/sample.c build/sample.asm
```

On Windows:

```powershell
.\build\donkey.exe examples\sample.c build\sample.asm
```

Compile the broader operator example:

```sh
./build/donkey examples/operators.c build/operators.asm
```

Compile the assignment and short-circuit examples:

```sh
./build/donkey examples/assignment.c build/assignment.asm
./build/donkey examples/short_circuit.c build/short_circuit.asm
./build/donkey examples/locals.c build/locals.asm
./build/donkey examples/multiple_functions.c build/multiple_functions.asm
./build/donkey examples/control_flow.c build/control_flow.asm
./build/donkey examples/casts.c build/casts.asm
./build/donkey examples/comments.c build/comments.asm
./build/donkey examples/globals.c build/globals.asm
```

If you omit the output path, Donkey writes to `output.asm` in the current
directory:

```sh
./build/donkey examples/unary.c
```

You can also build and run the sample target in one step:

```sh
make sample
```

## Language Support

Donkey currently accepts C-like `int` functions in this form:

```c
int main()
{
    return 1 + 2 * (3 + 4) - !0;
}
```

Supported expression features:

- Integer literals
- Parenthesized expressions
- Unary negation: `-x`
- Bitwise complement: `~x`
- Logical negation: `!x`
- Multiplicative operators: `*`, `/`, `%`
- Additive operators: `+`, `-`
- Relational operators: `<`, `<=`, `>`, `>=`
- Equality operators: `==`, `!=`
- Bitwise operators: `&`, `^`, `|`
- Logical operators: `&&`, `||`
- Assignment expression: `x = expression`
- Local declarations: `int x;` and `int x = expression;`
- Integer declarations and parameters using signed/unsigned `char`, `short`,
  `int`, and `long`
- Single-level pointer declarations and parameters such as `int *p`
- Local fixed-size integer arrays such as `int values[4]`
- Global fixed-size integer arrays such as `int table[4]`
- Brace initializers for arrays, with omitted elements zero-filled:
  `int values[3] = {1, 2};`
- Address-of, dereference, and indexing expressions: `&x`, `*p`, and `a[i]`
- Array-to-pointer decay in expressions, plus scaled pointer arithmetic:
  `p + 1`, `p - 1`, `p++`, and `p--`
- Multiple statements inside a function body
- Multiple integer-returning functions per input file
- Function parameters: `int helper(int x, int y)`
- Function calls with arguments: `helper(x, 4)`
- Global variables: `int g;` and `int g = constant_expression;`
- Conditionals: `if` and `if/else`
- Loops: `while`, expression-clause `for`, and declaration-initializer `for`
- Loop control: `break` and `continue`
- C-like precedence for the supported expression operators
- Shifts: `<<`, `>>`
- Increment/decrement: `++x`, `x++`, `--x`, `x--`
- Compound assignments: `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`
- Ternary conditional: `condition ? then_expr : else_expr`
- Comma expressions: `a, b`
- `sizeof` for common integer type names: `char`, `short`, `int`, `long`,
  plus signed and unsigned variants
- Casts for common integer type names: `(char)x`, `(unsigned char)x`,
  `(short)x`, `(unsigned short)x`, `(int)x`, `(long)x`, and signed/unsigned
  int/long variants
- Comments: `// line comments` and `/* block comments */`

Before generating assembly, Donkey performs semantic analysis. It rejects
duplicate declarations, undeclared variables and functions, calls with the
wrong number of arguments, non-constant global initializers, and `break` or
`continue` statements outside loops. Function signatures are collected before
function bodies are checked, so calls to functions defined later in the file
are valid.

Lexer, parser, and semantic diagnostics include the source path, line, and
column of the offending token:

```text
Semantic error at examples/bad.c:3:12 in function 'main': use of undeclared variable 'missing'
```

Expressions use C-style integer promotions and usual arithmetic conversions.
Assignments, arguments, and return values are converted to their destination
types; unsigned division, comparisons, and right shifts use unsigned machine
operations. All current integer types occupy four-byte storage slots, while
`char` and `short` values are narrowed and sign- or zero-extended as required.
Pointer assignments are type checked, and array indexing currently uses
four-byte elements. Pointer arithmetic is scaled by four-byte elements for the
current integer-only pointer model.

Local variables are stored in a simple stack frame. Assignment leaves the
assigned value in `%eax`, so it can be used inside larger expressions.

## Reference Output

`examples/sample.asm` is the checked-in reference output for
`examples/sample.c`. To regenerate it:

```sh
./build/donkey examples/sample.c examples/sample.asm
```

To assemble the generated file with GCC, force assembler mode because `.asm`
is not always detected automatically:

```sh
gcc -x assembler build/sample.asm -o build/sample
```

## Clean

```sh
make clean
```

This removes the `build/` directory.

## Current Limitations

- Nested blocks have lexical visibility, but variable shadowing is rejected
  until the code generator assigns symbols unique storage identities
- Global initializers must be constant expressions
- Arrays cannot be assigned as whole values
- Pointer subtraction between two pointers is not supported yet
- Assembly output is for learning and demonstration, not a complete production
  toolchain
