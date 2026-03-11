# Vexium Language Support for VS Code

This extension provides syntax highlighting and language support for the Vexium programming language.

## Features

- Syntax highlighting for `.vxm` and `.vxmc` files
- Language configuration for indentation and brackets
- File associations for Vexium source files

## Installation

1. Open VS Code
2. Go to Extensions (`Ctrl+Shift+X`)
3. Search for "Vexium"
4. Click Install

## Manual Installation

1. Copy this folder to `~/.vscode/extensions/` (Linux/Mac) or `%USERPROFILE%\.vscode\extensions\` (Windows)
2. Restart VS Code (or run `Developer: Reload Window`)
3. Open any `.vxm` file and ensure language mode is set to `Vexium`

## Color Highlighting

The syntax highlighter now colors:
- Keywords and control flow (`if`, `for`, `attempt`, `otherwise`, `await`, `give back`)
- Declarations (`let`, `const`, `struct`, `trait`, `impl`, `fn`, `can`, `task`)
- Imports/modules (`from ... use ...` with module-name coloring)
- Built-in functions (`display`, `len`, `str`, `tokenize`, math/time/json/http helpers)
- Types, constants, operators, function definitions/calls, strings with interpolation, and numbers

## Language Features

### Keywords
- Control flow: `fn`, `if`, `elif`, `else`, `while`, `for`, `each`, `match`, `attempt`, `otherwise`, `give back`, `break`, `skip`, `pass`, `defer`
- Concurrency: `spawn`, `task`, `channel`, `await`
- Declaration: `let`, `const`, `struct`, `trait`, `impl`, `has`, `can`, `use`, `from`

### Operators
- Arithmetic: `+`, `-`, `*`, `/`, `%`, `**` (power), `//` (floor division)
- Assignment: `=`, `be`, `+=`, `-=`, `*=`, `/=`
- Comparison: `is`, `is not`, `>`, `<`, `>=`, `<=`
- Logical: `and`, `or`, `not`

### Strings
- Double-quoted strings with `{variable}` interpolation
- Single-quoted character strings

### Numbers
- Integers: `42`
- Floats: `3.14`
- Hex: `0xFF`
- Binary: `0b1010`

## Example Vexium Code

```vexium
# Hello World
display "Hello, World!"

# Variables
let name be "Vexium"
let version be 1.0

# Function
fn greet(person):
    display "Hello, {person}!"

greet(name)

# Struct
struct Dog:
    has name: str
    has age: int
    
    can bark(self):
        display "{self.name} says Woof!"

let rex be Dog("Rex", 3)
rex.bark()

# Async
spawn greet("Async")
```

## License

MIT License
