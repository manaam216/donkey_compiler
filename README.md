# Donkey Compiler

## Overview

**Donkey Compiler** is a simple C compiler written in C, which is capable of parsing C-like programs, generating an Abstract Syntax Tree (AST), and producing assembly code from the parsed expressions. It can handle basic arithmetic operations, unary operators, and generate assembly for functions with simple return statements.

The project is being developed step-by-step, starting from a lexer to parsing, followed by code generation. Currently, it handles basic expressions involving addition, subtraction, multiplication, and division.

## Features

- **Lexical Analysis (Lexer):**
  - Tokenizes input programs into a stream of tokens such as operators, literals, and identifiers.

- **Parsing:**
  - Supports parsing of simple C-like functions, return statements, and basic expressions (arithmetic and unary operations).
  
- **Code Generation:**
  - Generates assembly code for arithmetic expressions such as `e1 + e2`, `e1 - e2`, `e1 * e2`, and `e1 / e2`.
  - Handles unary operators such as negation (`-`), bitwise complement (`~`), and logical negation (`!`).

## Build Steps

1. **Clone the repository:**

   ```bash
   git clone https://github.com/your-username/donkey-compiler.git
   cd donkey-compiler
   ```

2. **Build the project:**

   The project uses `Make` for building. To build the project, simply run:

   ```bash
   make
   ```

3. **Run the compiler:**

   After building the project, you can use the compiled `donkey` executable to parse and compile a C-like input file.

   ```bash
   ./donkey input_file.c
   ```

   This will parse the input C file, generate the corresponding assembly, and print it to the console.

## Input Syntax

The input to the **Donkey Compiler** is a simple C-like syntax that defines functions with basic arithmetic and unary operations.

### Supported Features:
- **Function Definition:**
  - Functions are defined with `int` as the return type, an identifier as the function name, and a return statement with an expression.
  
- **Supported Operations:**
  - **Addition**: `+`
  - **Subtraction**: `-`
  - **Multiplication**: `*`
  - **Division**: `/`
  - **Unary negation**: `-`
  - **Bitwise complement**: `~`
  - **Logical negation**: `!`

### Example Input:

**Example 1 (input_file.c):**

```c
int add() {
    return 1 + 2 * 3;
}
```

**Example 2 (input_file.c):**

```c
int negate() {
    return -5;
}
```

## Output

The **Donkey Compiler** will generate assembly code from the input C-like program. For example:

### Example 1 (Assembly Output):

```asm
.globl _add
_add:
    movl    $1, %eax
    push    %eax
    movl    $2, %eax
    imul    $3, %eax
    pop     %ecx
    addl    %ecx, %eax
    ret
```

### Example 2 (Assembly Output):

```asm
.globl _negate
_negate:
    movl    $5, %eax
    negl    %eax
    ret
```

## How It Works

### 1. **Lexer (Tokenization):**
The lexer processes the input file and converts the characters into tokens such as keywords, identifiers, operators, and literals.

### 2. **Parser (Syntax Analysis):**
The parser takes the tokens from the lexer and builds an Abstract Syntax Tree (AST). This tree represents the structure of the program based on the grammar rules defined for functions, statements, and expressions.

### 3. **Code Generation:**
The code generation phase traverses the AST and converts it into assembly instructions. For example:
- `1 + 2 * 3` is transformed into a series of assembly instructions that handle multiplication and addition.
- Unary operations like `-5` are handled by generating the appropriate assembly instructions for negation.

## File Structure

- **`defs.h`**: Defines the data structures and constants used across the compiler.
- **`decl.h`**: Contains function declarations for the AST and helper functions.
- **`lex_tokenization.c`**: Handles the lexical analysis and tokenization of the input source code.
- **`ast_algo_parser.c`**: Parses the tokenized input and generates an Abstract Syntax Tree (AST).
- **`generate_asm.c`**: Converts the AST into assembly code.
- **`main.c`**: The entry point of the compiler, responsible for driving the parsing and code generation.
- **`input`** Test input file

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for more details.

## Acknowledgements

- The **Donkey Compiler** is a project designed to help learn about compiler construction, including lexing, parsing, and code generation.
- Special thanks to all the compiler construction resources and tutorials that helped guide this project.
