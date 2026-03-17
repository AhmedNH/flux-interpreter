# Flux

A hand-built scripting language interpreter written in C from scratch — no parser generators, no external libraries, no shortcuts.

Flux is implemented as a classic three-phase pipeline:

```
Source code  →  Lexer  →  Token stream
                           ↓
                         Parser  →  Abstract Syntax Tree (AST)
                                     ↓
                                   Interpreter  →  Output
```

---

## Features

- **Variables** — `let x = 42;`
- **Arithmetic** — `+`, `-`, `*`, `/`, `%`
- **String concatenation** — `"Hello, " + name`
- **Booleans** — `true`, `false`, `!`, `&&`, `||`
- **Comparisons** — `==`, `!=`, `<`, `>`, `<=`, `>=`
- **Conditionals** — `if` / `else`
- **Loops** — `while`
- **Functions** — `fn`, `return` (including recursion)
- **Print** — `print(expr)`
- **Comments** — `// line comments`
- **REPL** — interactive mode with multi-line input

---

## Building

Requires GCC and Make.

```bash
make
```

---

## Usage

**Run a script:**
```bash
./flux examples/fibonacci.flux
```

**Interactive REPL:**
```bash
./flux
flux> let x = 10;
flux> print(x * 2);
20
flux> exit
```

---

## Language Tour

```flux
// Variables
let name = "Ahmed";
let score = 95;
print("Hello, " + name);

// Conditionals
if (score >= 90) {
    print("A");
} else {
    print("B");
}

// Loops
let i = 1;
while (i <= 5) {
    print(i);
    i = i + 1;
}

// Functions
fn factorial(n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

print(factorial(10));
```

---

## Project Structure

```
flux-interpreter/
├── src/
│   ├── lexer.h / lexer.c          # Tokenizer
│   ├── ast.h   / ast.c            # AST node types and constructors
│   ├── parser.h / parser.c        # Recursive-descent parser
│   ├── interpreter.h / interpreter.c  # Tree-walking evaluator
│   └── main.c                     # Entry point: file execution + REPL
├── examples/
│   ├── hello.flux
│   ├── fibonacci.flux
│   └── fizzbuzz.flux
├── Makefile
└── README.md
```

### Key implementation details

| Component | Technique |
|-----------|-----------|
| Lexer | Hand-written character-by-character scanner |
| Parser | Recursive descent with 1-token lookahead |
| AST | Tagged union (`struct Node`) with recursive destructor |
| Evaluator | Tree-walking interpreter |
| Scoping | Linked-list scope chain (lexical, block-scoped) |
| Functions | First-class with call stack and `return` propagation |
| Memory | Manual `malloc`/`free`; strings are deep-copied at every ownership boundary |

---

## Examples

**FizzBuzz:**
```bash
./flux examples/fizzbuzz.flux
```

**Fibonacci (recursive + iterative):**
```bash
./flux examples/fibonacci.flux
```
