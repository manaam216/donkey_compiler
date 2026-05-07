# Donkey Compiler

Donkey is a small educational compiler written in C. It accepts a tiny C-like
program, builds an abstract syntax tree, and emits 32-bit x86-style assembly for
a single integer-returning function.

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
|   `-- codegen.c     Assembly generator
|-- examples/         Source examples and reference assembly
|   |-- sample.c
|   |-- sample.asm
|   |-- operators.c
|   |-- assignment.c
|   |-- short_circuit.c
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

Donkey currently accepts one function in this form:

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
- C-like precedence for the supported expression operators

Identifiers used in expressions are backed by simple static storage emitted in
the generated assembly. Assignment leaves the assigned value in `%eax`, so it can
be used inside larger expressions.

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

- Only one function per input file
- Only one `return` statement per function body
- No explicit variable declarations, parameters, conditionals, or loops
- Identifiers are implicitly created as static integer slots
- Assembly output is for learning and demonstration, not a complete production
  toolchain
