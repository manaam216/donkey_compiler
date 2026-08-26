# Donkey Compiler

Donkey is a small educational compiler written in C. It accepts a tiny C-like
program, builds an abstract syntax tree, and emits x86-64 System V assembly for
integer-returning functions.

## Directory Layout

```text
.
|-- include/          Public compiler headers
|   |-- cli.h
|   |-- decl.h
|   |-- defs.h
|   |-- diag.h
|   |-- dump.h
|   |-- preprocess.h
|   |-- symbol.h
|   `-- type.h
|-- src/              Compiler implementation
|   |-- main.c        CLI entry point
|   |-- preprocess.c  Directives, macro expansion, #include
|   |-- lexer.c       Tokenizer
|   |-- parser.c      Recursive descent parser and AST allocation
|   |-- semantic.c    Name, scope, type, and function-call validation
|   |-- type.c        Type representation, sizes, and struct layout
|   |-- symbol.c      Storage identities and stack frame layout
|   |-- diag.c        Diagnostics, error recovery, and warnings
|   |-- cli.c         Command-line option parsing
|   |-- dump.c        Token and syntax-tree debug output
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
|   |-- harness_libc.c  Same, for programs that call the C library
|   |-- start.s        Entry point for tests linked without crt1.o
|   |-- unit/         Unit tests, run in process (type, lexer, cli)
|   |-- golden/       Reference assembly for every example
|   |-- expected/     Reference program output for every example
|   |-- syntax/       Inputs that must fail to parse
|   |-- semantic/     Inputs that must fail semantic analysis
|   |-- limits/       Inputs that must hit a compiler capacity limit
|   `-- preprocess/   Preprocessor inputs, checked against the system cpp
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
gcc -Iinclude -Wall -Wextra -g -o build\donkey.exe src\main.c src\preprocess.c src\lexer.c src\parser.c src\semantic.c src\type.c src\symbol.c src\diag.c src\dump.c src\cli.c src\codegen.c
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
exits `214`. `examples/wide_values.c` covers that range explicitly. Donkey can call `printf` now that prototypes exist, but most examples do not,
so the harness reports their result instead. Programs that do call the C
library keep their own output and are linked against it -- see
`examples/libc_call.c`.

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

```sh
./build/donkey examples/sample.c -o build/sample.asm
```

On Windows:

```powershell
.\build\donkey.exe examples\sample.c -o build\sample.asm
```

Run `./build/donkey --help` for the full list. The options that exist are:

| Option | Effect |
| --- | --- |
| `-o`, `--output <file>` | Where to write the assembly (default `output.asm`) |
| `-S` | Emit assembly; the only code-generating mode |
| `-E` | Preprocess only, and print the result |
| `-I <dir>` | Add a directory to the include search path |
| `-D <name>[=value]` | Define a macro; without a value it becomes `1` |
| `-Wall` | Enable all warnings, which is already the default |
| `-Werror` | Treat warnings as errors |
| `-w` | Suppress warnings |
| `--dump-tokens` | Print the token stream and stop |
| `--dump-ast` | Print the annotated syntax tree and stop |
| `-v`, `--verbose` | Report each stage as it runs |
| `-h`, `--help` | Usage |
| `--version` | Version |

Options belonging to stages that do not exist yet -- `-O`, `-g`, `-c`,
`--dump-ir` -- are refused with an explanation rather than accepted
and ignored, so a build never quietly does something other than what was asked:

```text
$ ./build/donkey -O2 examples/sample.c
-O2: not supported yet: there is no optimiser yet
```

The older form, `donkey input.c output.asm`, still works.

### Inspecting the compiler's own state

`--dump-ast` runs after semantic analysis, so each node shows the type and the
storage location resolved for it:

```text
function 'main' : int [function frame=8]   <1:5>
  block   <2:1>
    statement_list   <3:9>
      decl 'x' : int [local -4(%rbp)]   <3:9>
        intlit '3' : int   <3:13>
```

`--dump-tokens` prints the token stream with the line and column each token
carries, which is what every diagnostic points at.

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
- Brace initializers for arrays and structs, with anything not covered left
  zero: `int values[3] = {1, 2};`, `struct Point p = {1, 2};`
- Designated initializers, which say where a value goes and let the ones after
  it follow on: `int a[6] = {[1] = 10, [4] = 40};`,
  `struct Point p = {.z = 30, .x = 10};`
- Address-of, dereference, and indexing expressions: `&x`, `*p`, and `a[i]`
- Struct field access through a pointer: `p->field`
- `++` and `--` on any assignable expression, not only named variables:
  `a[i]++`, `p->count++`
- Whole-struct assignment: `b = a` copies every field
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
- Function prototypes, so a function can be called before it is defined:
  `int helper(int x);`
- `void` as a return type, and `f(void)` for a function taking nothing
- Variadic declarations: `int printf(const char *format, ...);`, which makes
  the C library callable
- Storage classes and qualifiers -- `extern`, `static`, `const`, `volatile` --
  accepted wherever a declaration allows them
- Several declarators in one declaration: `int a = 1, b, *p;`
- `enum`, with explicit values: `enum Status { OK = 10, FAILED };`
- `typedef`, including pointer aliases: `typedef int *IntPtr;`
- Function calls with arguments: `helper(x, 4)`
- Global variables: `int g;` and `int g = constant_expression;`
- Conditionals: `if` and `if/else`
- `switch` with `case`, `default`, and fallthrough between cases
- Loops: `while`, `do`/`while`, expression-clause `for`, and
  declaration-initializer `for`
- Loop control: `break` and `continue`
- `goto` and labels
- The empty statement, `;`, and a bare `return;`
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

### Preprocessing

Donkey runs a preprocessor over the source before parsing it. It works on
tokens rather than raw text, because text substitution cannot get `#` and `##`
right and has no reliable way to tell a macro name from the same letters inside
a string literal.

Supported: `#include` in both `"file"` and `<file>` forms, `#define` for object
and function-like macros, `#undef`, `#if` / `#ifdef` / `#ifndef` / `#elif` /
`#else` / `#endif` with full constant-expression evaluation and `defined`,
`#pragma once`, `#error`, `#warning`, and the `#` and `##` operators.
`__STDC__` and `__DONKEY__` are predefined. A macro is not re-expanded inside
its own expansion, so a self-referential definition terminates.

`-E` prints the result:

```sh
./build/donkey -E examples/sample.c
```

The test suite compares that output against the system `cpp`, token for token,
for `tests/preprocess/features.c`.

Diagnostics name the file a token actually came from, so an error inside an
included header points at the header rather than at the file that included it.

**`#include <stdio.h>` still does not work**, though it is closer. `typedef`,
`extern`, prototypes, `void`, and varargs all exist now, so a function can be
declared by hand and called:

```c
int printf(const char *format, ...);

int main()
{
    printf("hello from donkey
");
    return 0;
}
```

That compiles, links against the system C library, and runs -- see
`examples/libc_call.c`. A real `stdio.h` still needs more than the declaration
forms: compiler-specific attributes, nested struct definitions, and
`unsigned long long` among them.

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

- A struct larger than eight bytes cannot be passed by value, and no struct
  can be returned by value. Both need System V's two-register and stack-copy
  rules; oversized ones are refused rather than quietly miscompiled. Assigning
  a whole struct, and passing a pointer to one, both work.
- Function-pointer declarators are recognised only in the form
  `TYPE (*name)(params)`. The general recursive declarator grammar, which would
  also give `int (*a)[10]`, is not implemented.
- `long double` is not distinguished from `double`.
- No compound literals: `(struct Point){1, 2}` does not parse. Designated
  initializers, the more useful half of that pair, do.
- No `union`, and no nested struct definitions.
- Global initializers must be constant expressions.
- Arrays cannot be assigned as whole values.
- Arrays nest at most four deep.
- Assembly output is for learning and demonstration, not a complete production
  toolchain.
