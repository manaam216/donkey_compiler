# Donkey Compiler

Donkey is a small educational compiler written in C. It accepts a tiny C-like
program, builds an abstract syntax tree, and emits x86-64 System V assembly for
integer-returning functions.

## Directory Layout

```text
.
|-- include/          Public compiler headers
|   |-- decl.h
|   |-- defs.h
|   |-- diag.h
|   |-- symbol.h
|   `-- type.h
|-- src/              Compiler implementation
|   |-- main.c        CLI entry point
|   |-- lexer.c       Tokenizer
|   |-- parser.c      Recursive descent parser and AST allocation
|   |-- semantic.c    Name, scope, type, and function-call validation
|   |-- type.c        Type representation, sizes, and struct layout
|   |-- symbol.c      Storage identities and stack frame layout
|   |-- diag.c        Diagnostics, error recovery, and warnings
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
|-- tests/            Test inputs and expectations
|   |-- harness.c     Prints a compiled program's result (see Test)
|   |-- harness_freestanding.c  Same, without libc (see Test)
|   |-- unit/         Unit tests for compiler internals
|   |-- golden/       Reference assembly for every example
|   |-- expected/     Reference program output for every example
|   |-- syntax/       Inputs that must fail to parse
|   |-- semantic/     Inputs that must fail semantic analysis
|   `-- limits/       Inputs that must hit a compiler capacity limit
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
gcc -Iinclude -Wall -Wextra -g -o build\donkey.exe src\main.c src\lexer.c src\parser.c src\semantic.c src\type.c src\symbol.c src\codegen.c
```

## Test

Run the project checks:

```sh
make test
```

The test script rebuilds the compiler and, for every example in `examples/`:

1. Compiles it to assembly and diffs that against the golden copy in
   `tests/golden/`.
2. Renames the example's `main` symbol to `donkey_main`, links it against
   `tests/harness.c`, runs it, and diffs the printed result against
   `tests/expected/`.

It then checks that each program in `tests/` is rejected with the expected
diagnostic.

Results are compared as **printed values rather than process exit codes**.
Exit codes are truncated to 8 unsigned bits, so they silently accept wrong
answers: a function returning `100000` exits `160`, and one returning `-42`
exits `214`. `examples/wide_values.c` covers that range explicitly. Donkey
itself cannot call `printf` — it has no preprocessor, and semantic analysis
rejects undeclared functions — hence the separate harness.

After an intentional codegen or diagnostic change, regenerate the golden files
and review the diff before committing:

```sh
UPDATE_GOLDEN=1 make test
```

Assembling and running the output needs an x86-64 System V host. The suite
detects this: where the host toolchain does not match, it still compiles every
example, diffs the golden assembly, and checks every diagnostic, but skips
execution. Force either mode with:

```sh
SKIP_RUN=1 make test
```

To run the generated code from a host that cannot execute it directly (a
32-bit MinGW box, for instance), `scripts/run64.sh` cross-assembles with clang,
links with `ld.lld` against `tests/harness_freestanding.c`, and executes the
result under WSL:

```sh
sh scripts/run64.sh examples/sample.c
```

CI runs three jobs on GitHub Actions: the full flow on Linux x86-64, which is
the platform that can assemble the output and link it against the system libc;
a compile-only pass on Windows MSYS2 MINGW32 that catches portability bugs in
the compiler's own source; and an ASan/UBSan build. MinGW GCC cannot build with
sanitizers, so that job is Linux-only.

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
- Pointer subtraction for compatible pointer types
- Array parameters such as `int values[4]`, treated as pointer parameters
- Character literals including common escapes: `'A'`, `'\n'`, `'\0'`,
  `'\''`, and `'\\'`
- String literals with static storage, usable as `char *`
- Limited structs with integer/pointer fields, stack/global variables, field
  read/write via `value.field`, and arrays of structs such as `struct P pts[3]`
  with `pts[i].field` access
- Arrays of any supported element type, packed at the element's real size:
  `char letters[4]` occupies 4 bytes and indexes by 1
- Nested blocks, with shadowing: an inner declaration hides an outer one of
  the same name
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
- `sizeof` for type names and for expressions, yielding the operand's real
  size: `sizeof(int)` is 4, `sizeof(buf)` for `char buf[10]` is 10, and
  `sizeof` a struct includes its padding
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

### Diagnostics

Every stage reports what it finds and carries on, so one run surfaces several
problems rather than the first one and nothing else. Compilation stops between
stages, where it is safe to: there is no point type-checking a tree the parser
could not build. After a syntax error the parser resynchronises on the next
`;` or statement keyword, so a missing semicolon in one function does not hide
a problem in the next.

Messages use the conventional compiler format, quoting the offending line and
marking the column:

```text
examples/bad.c: In function 'main':
examples/bad.c:3:12: error: use of undeclared variable 'missing'
        return missing;
               ^
```

Warnings are reported the same way but do not fail the build. `unreachable
statement after 'return'` is the first of them.

Beyond twenty errors, further ones are counted but not printed: past that point
they are usually consequences of earlier problems rather than new information.

A message labelled `internal error` means an invariant the compiler itself was
supposed to guarantee has been broken -- a bug in Donkey, not in the input.
Those stop compilation immediately, because continuing would emit wrong code
rather than report a problem.

Types are represented by a `Type` tree (`include/type.h`) that knows its own
size and alignment. Semantic analysis resolves each declaration and expression
to a type, and the code generator reads those types for storage sizes, struct
field offsets, array strides, pointer arithmetic scaling, and `sizeof`.

The backend targets the x86-64 System V ABI. The first six integer or pointer
arguments are passed in `rdi`, `rsi`, `rdx`, `rcx`, `r8`, and `r9` and spilled
into the frame on entry; further arguments go on the stack. Results come back
in `rax`. Stack frames are rounded to a multiple of 16 bytes and call sites pad
an odd number of stack arguments, so `%rsp` is 16-byte aligned at every `call`
as the ABI requires. Generated code touches only caller-saved registers, so no
callee-saved register needs preserving beyond `rbp`. Globals and string
literals are reached with `%rip`-relative addressing.

Semantic analysis also gives every declaration a `Symbol` (`include/symbol.h`)
holding its storage location, and attaches it to the AST along with the frame
size each function needs. The code generator keeps no symbol table of its own
and never resolves a name: it reads storage straight off the annotated tree.
Because identity lives in the symbol rather than the name, a variable can
shadow an outer one, and disjoint blocks reuse the same stack slots.

Expressions use C-style integer promotions and usual arithmetic conversions.
Assignments, arguments, and return values are converted to their destination
types; unsigned division, comparisons, and right shifts use unsigned machine
operations. Integer types have their natural sizes for the x86-64 System V target (LP64):
`char` is 1 byte, `short` is 2, `int` is 4, and `long` and pointers are 8. Array elements, struct
fields, pointer arithmetic, and `sizeof` all use these real sizes, and loads
and stores are emitted at the matching width. Struct fields are laid out with
the padding needed to keep each field aligned, plus tail padding so arrays of
a struct stay aligned. Scalar locals occupy a whole slot, at least a word wide.

Local variables are stored in a simple stack frame. Assignment leaves the
assigned value in `%eax`, so it can be used inside larger expressions.

## Reference Output

`tests/golden/` holds the checked-in reference assembly for every example, and
`tests/expected/` the reference program output. Regenerate both with:

```sh
UPDATE_GOLDEN=1 make test
```

Review the resulting diff before committing — it is the only thing separating
an intentional codegen change from a regression.

(`examples/sample.asm` predates `tests/golden/` and is no longer used by the
test suite.)

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

- Global initializers must be constant expressions
- Arrays cannot be assigned as whole values
- Struct support does not include nested structs or struct parameters yet
- Assembly output is for learning and demonstration, not a complete production
  toolchain
