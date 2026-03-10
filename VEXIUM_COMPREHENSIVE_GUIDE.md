# 📖 Vexium Language: Comprehensive Definition & Implementation Guide

> **"Code that reads like English. Runs like C. Thinks like AI."**
> 
> *A complete reference for Vexium - the Python-style, AI-native programming language with OS-level capabilities.*

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Language Philosophy & Design](#language-philosophy--design)
3. [Core Architecture](#core-architecture)
4. [Syntax Fundamentals](#syntax-fundamentals)
5. [Data Types & Type System](#data-types--type-system)
6. [Variables, Constants & Scope](#variables-constants--scope)
7. [Control Flow](#control-flow)
8. [Functions & Closures](#functions--closures)
9. [Object-Oriented Programming](#object-oriented-programming)
10. [Module System](#module-system)
11. [Standard Library](#standard-library)
12. [Error Handling](#error-handling)
13. [Bytecode Compilation & VM](#bytecode-compilation--vm)
14. [CLI Tooling & Commands](#cli-tooling--commands)
15. [Implementation Roadmap](#implementation-roadmap)
16. [Best Practices & Patterns](#best-practices--patterns)

---

## Executive Summary

### What is Vexium?

**Vexium** is a modern, high-level programming language designed with four core tenets:

1. **Python-style syntax** - Indentation-based blocks, clean, readable code
2. **OS-first design** - Direct memory access, hardware control, kernel-level operations
3. **AI-native features** - Built-in tensor operations, neural networks, LLM integration
4. **High performance** - Bytecode VM with NaN-boxing optimization, approaching C/Lua speeds

### Key Characteristics

| Aspect | Details |
|--------|---------|
| **Type System** | Gradual typing with optional annotations, Hindley-Milner inference |
| **Paradigm** | Multi-paradigm: imperative, functional, object-oriented |
| **Memory Management** | Mark-and-sweep garbage collection with configurable thresholds |
| **Compilation** | Dual-mode: tree-walk interpreter (v1) or bytecode VM (v2) |
| **Module System** | User-defined modules with `use` and `from...use` syntax |
| **Standard Library** | Math, String, Collections, JSON, Time, HTTP, System, File I/O |
| **Safety** | Memory-safe by default, `unsafe:` blocks for low-level operations |
| **Development Status** | v2.0 MVP (~35% complete), tree-walk interpreter (v1) stable |

### Quick Facts

- **Created**: 2025-2026 (ongoing)
- **Version**: 2.0 (MVP Phase)
- **Implementation**: C99 with recursive descent parser, NaN-boxing value representation
- **Source Lines**: ~15,000+ LOC in C
- **Performance**: 10-100x faster with bytecode VM vs tree-walk
- **File Extension**: `.vxm` (Vex Modules)
- **Bytecode Extension**: `.vxmc` (Vex Modules Compiled)

---

## Language Philosophy & Design

### Core Principles

#### 1. Readability > Performance
Code should read like English documentation. Performance gains through optimization, not compression.

```vex
# ✅ Good - Clear intent
let result be calculate_total_cost(items, tax_rate)

# ❌ Avoid - Hardware-level shortcuts
let r = calc_tot(i, rate)
```

#### 2. Explicit is Better than Implicit
Clear intent prevents bugs. Type annotations are optional but encouraged.

```vex
# ✅ Good - Type annotations clarify intent
fn process_data(items: [int], threshold: int) -> [int]:
    give back filter(items, where value is greater than threshold)

# Also OK - Inference works automatically
fn process_data(items, threshold):
    give back filter(items, where value is greater than threshold)
```

#### 3. One Way to Do It
Unlike Python's "multiple ways" philosophy, Vexium has preferred patterns for common tasks.

```vex
# ✅ Preferred for loop iteration
for item in collection:
    display item

# ✅ Alternative for range loops
for i in 0 to 10:
    display i

# ✅ Functional style (when it's cleaner)
display map(collection, where value * 2)
```

#### 4. Safe by Default, Powerful with Intent
Memory safety is default. Low-level operations require explicit `unsafe:` marking.

```vex
# ✅ Safe - automatic bounds checking
let element be my_array[index]

# Low-level - must be marked
unsafe:
    let ptr be memory_address(value)
    write_bytes(ptr, buffer)
```

### Design Goals

| Goal | Implementation |
|------|----------------|
| **Learnability** | Python-like syntax, 35 keywords, consistent patterns |
| **Safety** | Type inference, garbage collection, bounds checking |
| **Performance** | Bytecode VM, constant folding, NaN-boxing |
| **Integration** | Module system, standard library, FFI (planned) |
| **Scalability** | Multi-core support (planned), async/await (planned) |
| **AI/ML Ready** | Native tensor type, neural net operations (v3.0) |

---

## Core Architecture

### Compilation Pipeline

```
┌─────────────────┐
│  Source Code    │  (.vxm file)
│  (Vex syntax)   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Lexer          │  Tokenization
│  (lexer.c)      │  • Scan characters → tokens
└────────┬────────┘  • 35 keywords, operators, literals
         │
         ▼
┌─────────────────┐
│  Parser         │  Syntax Analysis
│  (parser.c)     │  • Recursive descent
└────────┬────────┘  • AST construction
         │           • Error recovery
         ▼
┌─────────────────┐
│  AST            │  Abstract Syntax Tree
│  (ast.c)        │  • Function definitions
└────────┬────────┘  • Expressions & statements
         │           • Control flow structures
    ┌────┴────┐
    │          │
    ▼          ▼
┌─────────┐  ┌──────────────┐
│Interpret│  │Bytecode      │  Mode 1: Tree-walk (v1)
│  (tree- │  │Compiler      │  Mode 2: Bytecode (v2)
│  walk)  │  │(compiler.c)  │
└────┬────┘  └──────┬───────┘
     │               │
     ▼               ▼
  Output        ┌──────────────┐
  (stdout)      │ Bytecode     │
                │ File         │
                │ (.vxmc)      │
                └──────┬───────┘
                       │
                       ▼
                ┌──────────────┐
                │ Stack-based  │
                │ Virtual      │
                │ Machine (vm) │
                └──────┬───────┘
                       │
                       ▼
                    Output
```

### Component Overview

| Component | File | Purpose |
|-----------|------|---------|
| **Lexer** | `lexer.c/h` | Tokenization - Convert source to tokens |
| **Parser** | `parser.c/h` | Syntax analysis - Build AST from tokens |
| **AST** | `ast.c/h` | Abstract syntax tree representation |
| **Interpreter** | `interpreter.c/h` | Tree-walk evaluation (v1 mode) |
| **Compiler** | `compiler.c/h` | AST → Bytecode conversion |
| **VM** | `vm.c/h` | Stack-based bytecode execution |
| **Type System** | `type_system.c/h` | Type inference & checking |
| **Garbage Collector** | `gc.c/h` | Mark-and-sweep memory management |
| **Module Loader** | `module_loader.c/h` | Import resolution & caching |
| **Standard Library** | `stdlib.c/h` | Built-in functions & constants |
| **Main/CLI** | `main.c` | Command-line interface |

---

## Syntax Fundamentals

### Basic Structure

```vex
# Comments start with #
# Blocks are defined by indentation (4 spaces)
# No curly braces, no semicolons

fn greet(name):
    let message be "Hello, {name}!"
    display message
    give back message

# Call the function
let result be greet("Alice")
```

### Indentation Rules

- **4 spaces per level** - Consistent with Python
- **Tabs prohibited** - Use spaces only
- **Line continuation** - Implicit at operators, explicit with `\`

```vex
# Line continuation (automatic at operator)
let total be first_value +
             second_value +
             third_value

# Explicit continuation
let result be function(param1, \
                      param2, \
                      param3)
```

### Comments

```vex
# Single-line comment

#***
  Multi-line comment
  using repeated # pairs
***#

# Documentation comments (planned)
### Function documentation
fn add(a, b):
    ### Adds two numbers
    give back a + b
```

### String Literals

```vex
# Single-quoted string
let s1 be 'hello'

# Double-quoted string
let s2 be "world"

# Multi-line string
let s3 be """
This is a
multi-line
string literal
"""

# String interpolation
let name be "Alice"
let age be 30
display "Name: {name}, Age: {age}"

# Escape sequences
let escaped be "Quote: \" Backslash: \\ Newline: \n"
```

### Keywords (35 Total)

#### Declaration Keywords
| Keyword | Purpose | Example |
|---------|---------|---------|
| `let` | Mutable variable | `let x be 10` |
| `const` | Immutable variable | `const PI be 3.14159` |
| `fn` | Function definition | `fn add(a, b): give back a + b` |
| `struct` | Class definition | `struct Person: has name` |
| `use` | Module import | `use math` |
| `give back` | Return statement | `give back result` |

#### Control Flow Keywords
| Keyword | Purpose | Example |
|---------|---------|---------|
| `if` | Conditional | `if x > 0:` |
| `elif` | Else-if | `elif x is 0:` |
| `else` | Else branch | `else:` |
| `while` | While loop | `while running:` |
| `for` | For-each loop | `for item in list:` |
| `repeat` | Repeat N times | `repeat 10 times:` |
| `break` | Exit loop | `break` |
| `skip` | Next iteration | `skip` (continue) |
| `pass` | Empty statement | `pass` |
| `match` | Pattern matching | `match value: when 1: ...` |
| `when` | Match case | `when pattern: ...` |

#### Logical/Type Keywords
| Keyword | Purpose | Example |
|---------|---------|---------|
| `true` | Boolean true | `let flag be true` |
| `false` | Boolean false | `let flag be false` |
| `nothing` | Null/nil value | `let x be nothing` |
| `and` | Logical AND | `if a and b:` |
| `or` | Logical OR | `if a or b:` |
| `not` | Logical NOT | `if not flag:` |
| `is` | Equality/comparison | `if x is 5:` |
| `be` | Assignment (English) | `let x be 10` |
| `in` | Membership/iteration | `for x in array:` |
| `to` | Range end | `for i in 0 to 10:` |
| `by` | Range step | `for i in 0 to 100 by 2:` |
| `less` | Less than | `if x less than 5:` |
| `greater` | Greater than | `if x greater than 5:` |
| `at least` | Greater or equal | `if x at least 5:` |
| `at most` | Less or equal | `if x at most 5:` |

#### Error Handling Keywords
| Keyword | Purpose | Example |
|---------|---------|---------|
| `attempt` | Try block | `attempt: ...` |
| `otherwise` | Catch block | `otherwise error: ...` |
| `throw` | Raise error | `throw "Error message"` |

#### Other Keywords
| Keyword | Purpose | Example |
|---------|---------|---------|
| `display` | Print output | `display "Hello"` |
| `input` | Read input | `let x be input("Enter: ")` |
| `can` | Method definition | `can method_name():` |
| `self` | Instance reference | `self.property` |
| `has` | Struct field | `has name` |
| `type` | Type keyword | `type` (planned) |
| `unsafe` | Unsafe block | `unsafe: ...` |
| `from` | From import | `from math use sqrt` |
| `each` | Optional loop emphasis | `for each item in list:` |
| `times` | Repeat keyword | `repeat 10 times:` |

---

## Data Types & Type System

### Primitive Types

#### Integers (int)

```vex
let age be 25
let big_number be 1000000000
let negative be -42
let hex be 0xFF
let binary be 0b1010

# Operations
display 10 + 5      # 15
display 10 - 5      # 5
display 10 * 5      # 50
display 10 / 5      # 2 (integer division)
display 10 % 3      # 1 (modulo)
display 2 ^ 10      # 1024 (exponentiation)
```

#### Floats (float)

```vex
let pi be 3.14159
let negative be -2.5
let scientific be 1.5e-10
let infinity be 1e308  # Approaches infinity

# Operations (same as integers)
display 3.14 + 2.86   # 6.0
display 10.5 / 2.0    # 5.25
display 2.0 ^ 3       # 8.0
```

#### Strings (str)

```vex
let greeting be "Hello, World!"
let empty be ""
let multiline be """
Line 1
Line 2
Line 3
"""

# String interpolation
let name be "Alice"
let message be "Welcome, {name}!"

# String operations
display len("hello")                    # 5
display "hello" + " world"              # "hello world"
display "Hello".lower()                 # "hello"
display "hello".upper()                 # "HELLO"
display "hello".contains("ll")          # true
display "a,b,c".split(",")              # ["a", "b", "c"]
```

#### Booleans (bool)

```vex
let flag be true
let other be false

# Operations
display true and true                   # true
display true and false                  # false
display true or false                   # true
display not true                        # false

# Comparisons produce booleans
display 5 > 3                           # true
display "hello" is "hello"              # true
display 10 is not 5                     # true
```

#### Nothing (null/nil)

```vex
let empty_value be nothing

# Check for nothing
if value is nothing:
    display "Value is empty"
```

### Composite Types

#### Arrays

```vex
# Create arrays with literals
let numbers be [1, 2, 3, 4, 5]
let mixed be [1, "hello", 3.14, true]
let empty be []

# Type annotation
let typed: [int] be [1, 2, 3]

# Access elements (0-indexed)
display numbers[0]                     # 1
display numbers[2]                     # 3

# Modify elements
let list be [10, 20, 30]
list[1] be 25
display list                           # [10, 25, 30]

# Array methods
display len(numbers)                   # 5
numbers.push(6)
display numbers                        # [1, 2, 3, 4, 5, 6]
let last be numbers.pop()              # 6
display numbers                        # [1, 2, 3, 4, 5]
display numbers.contains(3)            # true
display numbers.index_of(4)            # 3

# Array operations
let doubled be map(numbers, where x * 2)
let evens be filter(numbers, where x % 2 is 0)
```

#### Maps/Dictionaries

```vex
# Create maps with literals
let person be {
    "name": "Alice",
    "age": 30,
    "city": "New York"
}

# Type annotation
let typed: {str: int} be {"a": 1, "b": 2}

# Access values
display person["name"]                 # "Alice"
display person["age"]                  # 30

# Modify values
person["age"] be 31
person["job"] be "Engineer"            # Add new key

# Map methods
display len(person)                    # 4 (number of keys)
display person.contains("name")        # true
display person.keys()                  # ["name", "age", "city", "job"]
display person.values()                # ["Alice", 31, "New York", "Engineer"]
```

#### Tuples (Fixed-size collections)

```vex
# Fixed-size tuple with mixed types
let point: (int, int) be (10, 20)
let triple: (str, int, bool) be ("test", 42, true)

# Access by index
display point[0]                       # 10
display triple[1]                      # 42

# Destructuring in assignment
let (x, y) be (5, 10)
display x                              # 5
display y                              # 10
```

### User-Defined Types

#### Structs (Classes)

```vex
# Define a struct
struct Point:
    has x
    has y

# Create instances
let p be Point()
p.x be 10
p.y be 20

# Struct with initialization
struct Person:
    has name
    has age
    has email

    can init(n, a, e):
        self.name be n
        self.age be a
        self.email be e

let alice be Person("Alice", 30, "alice@example.com")
display alice.name                     # "Alice"
```

### Type System Details

#### Gradual Typing

Vexium supports optional type annotations. Types can be:
- **Explicitly specified** - `let x: int be 5`
- **Inferred from initialization** - `let x be 5` (inferred as `int`)
- **Inferred from usage** - Type checker analyzes how variables are used

```vex
# Explicit annotation
let x: int be 5
let y: str be "hello"
let z: [int] be [1, 2, 3]

# Implicit inference
let a be 5                             # Inferred as int
let b be 5.0                           # Inferred as float
let c be [1, 2, 3]                    # Inferred as [int]

# Function signatures
fn add(a: int, b: int) -> int:
    give back a + b

fn greet(name: str) -> str:
    give back "Hello, {name}!"

# Unannotated functions (auto-infer)
fn multiply(a, b):
    give back a * b                   # Inferred: (number, number) -> number
```

#### Type Narrowing

The type checker narrows types in conditional blocks:

```vex
fn process(value: int | str):
    if type(value) is "int":
        display value + 10            # Safe: value is int here
    else:
        display value.upper()         # Safe: value is str here
```

#### Generic Types

```vex
# Generic array of any type
let list: [T] be [1, 2, 3]

# Generic function
fn swap(a: T, b: T) -> (T, T):
    give back (b, a)

let result be swap(5, 10)             # (10, 5)
```

---

## Variables, Constants & Scope

### Variable Declaration

#### Mutable Variables (let)

```vex
let x be 10
x be 20                    # OK - reassignment allowed
display x                  # 20

let greeting be "Hello"
greeting be "Hi"           # OK
```

#### Immutable Constants (const)

```vex
const PI be 3.14159
PI be 3.14                 # ERROR: Cannot reassign const

const COLORS be ["red", "green", "blue"]
COLORS be []               # ERROR: Cannot reassign

# But you can modify contents of mutable data structures
COLORS[0] be "yellow"      # OK
```

### Scope Rules

#### Global Scope

```vex
let global_var be 42      # Available everywhere

fn show():
    display global_var     # Can read global
    global_var be 100      # Can modify global
```

#### Local Scope

```vex
fn process():
    let local_var be 10    # Only available in this function
    
    if true:
        let block_var be 5 # Only available in this block
        display block_var  # 5

display local_var          # ERROR: undefined
display block_var          # ERROR: undefined
```

#### Block Scope

```vex
let x be 1

if true:
    let x be 2            # Shadows outer x
    display x             # 2

display x                 # 1 (outer x unchanged)

while condition:
    let temp be calculate()
    # temp only exists in loop
```

#### Function Parameters

```vex
fn greet(name):           # 'name' is parameter
    let greeting be "Hello"
    display "{greeting}, {name}"

greet("Alice")            # name = "Alice"
greet("Bob")              # name = "Bob"
```

#### Closure Capture

```vex
fn make_multiplier(factor):
    fn multiply(x):
        give back x * factor    # Captures 'factor' from outer scope
    give back multiply

let times3 be make_multiplier(3)
let times5 be make_multiplier(5)

display times3(10)         # 30
display times5(10)         # 50
```

---

## Control Flow

### Conditional Statements

#### if/elif/else

```vex
let age be 20

if age less than 13:
    display "Child"
elif age less than 18:
    display "Teenager"
elif age less than 65:
    display "Adult"
else:
    display "Senior"

# Comparison operators
if x is 5:
display x is greater than 10:
display x is less than 20:
display x is at least 15:
display x is at most 18:
display x is not 0:
```

#### Ternary-style with Short-circuit

```vex
# Using 'if' as expression (planned)
let category be if age less than 18: "minor" else: "adult"

# Boolean short-circuit evaluation
let result be flag or expensive_operation()   # Won't call if flag is true
```

#### Comparison Operators

```vex
display 5 is 5                        # true (equality)
display 5 is not 3                    # true
display 5 > 3                         # true
display 5 less than 10                # true (alternative syntax)
display 5 greater than 3              # true
display 5 at least 5                  # true
display 5 at most 10                  # true
display "hello" is "hello"            # true (strings)
display [1,2] is [1,2]               # true (deep equality)
```

### Loops

#### While Loops

```vex
let i be 0
while i less than 5:
    display i
    i be i + 1

# Output: 0 1 2 3 4

# With break/skip
while true:
    let input be input("Enter (q to quit): ")
    if input is "q":
        break
    display "You entered: {input}"
```

#### For-in Loops (Arrays)

```vex
let numbers be [10, 20, 30, 40]

for num in numbers:
    display num
# Output: 10 20 30 40

# With break/skip
for item in list:
    if item is 0:
        skip                          # Skip this iteration
    if item > 100:
        break                         # Exit loop entirely
    display item
```

#### For-in Loops (Ranges)

```vex
# Range 0 to 9
for i in 0 to 10:
    display i
# Output: 0 1 2 3 4 5 6 7 8 9

# Range with step
for i in 0 to 20 by 2:
    display i
# Output: 0 2 4 6 8 10 12 14 16 18

# Reverse range
for i in 10 to 0 by -1:
    display i
# Output: 10 9 8 7 6 5 4 3 2 1
```

#### Repeat N Times

```vex
repeat 5 times:
    display "Hello"
# Output: Hello (5 times)

# With index
repeat 3 times:
    display "Count: ..."
```

#### For-Each (Array iteration with context)

```vex
let items be ["apple", "banana", "cherry"]

for each item in items:
    display item

# Equivalent to:
for item in items:
    display item
```

### Loop Control

#### break

```vex
for i in 1 to 100:
    if i is 50:
        break                        # Exit loop immediately
    display i
# Output: 1 2 3 ... 49
```

#### skip (continue)

```vex
for i in 1 to 10:
    if i % 2 is 0:
        skip                         # Skip even numbers
    display i
# Output: 1 3 5 7 9
```

### Pattern Matching

```vex
let fruit be "apple"

match fruit:
    when "apple":
        display "Red fruit"
    when "banana":
        display "Yellow fruit"
    when "grape":
        display "Purple fruit"
    otherwise:
        display "Unknown fruit"

# With ranges (planned)
let score be 85

match score:
    when 90 to 100:
        display "A"
    when 80 to 89:
        display "B"
    when 70 to 79:
        display "C"
    otherwise:
        display "F"
```

---

## Functions & Closures

### Function Definition

#### Basic Functions

```vex
fn greet(name):
    display "Hello, {name}!"

greet("Alice")             # Call function
```

#### Functions with Return Values

```vex
fn add(a, b):
    give back a + b

let result be add(5, 3)
display result             # 8
```

#### Named Parameters

```vex
fn create_person(name, age):
    give back {"name": name, "age": age}

let p be create_person("Alice", 30)
```

#### Default Parameters

```vex
fn greet(name, greeting be "Hello"):
    give back "{greeting}, {name}!"

display greet("Alice")                 # "Hello, Alice!"
display greet("Bob", "Hi")             # "Hi, Bob!"
```

#### Variadic Functions (planned)

```vex
fn sum(...numbers):
    let total be 0
    for n in numbers:
        total be total + n
    give back total

display sum(1, 2, 3, 4, 5)            # 15
```

### Function Types

#### First-Class Functions

```vex
# Functions are values
let my_func be fn(x):
    give back x * 2

display my_func(5)         # 10

# Pass functions as parameters
fn apply(f, x):
    give back f(x)

display apply(my_func, 5)  # 10
```

#### Anonymous Functions

```vex
let square be fn(x): give back x * x

display square(5)          # 25

# In higher-order functions
let doubled be map([1, 2, 3], fn(x): give back x * 2)
display doubled            # [2, 4, 6]
```

#### Arrow Functions (Syntax sugar, planned)

```vex
# Planned shorthand
let square be (x) => x * x
let add be (a, b) => a + b
```

### Closures

#### Closure Basics

```vex
fn make_adder(x):
    fn adder(y):
        give back x + y    # Captures 'x' from outer scope
    give back adder

let add5 be make_adder(5)
let add10 be make_adder(10)

display add5(3)            # 8
display add10(3)           # 13
```

#### Closure with Mutable State

```vex
fn make_counter():
    let count be 0
    
    fn increment():
        count be count + 1
        give back count
    
    fn get_count():
        give back count
    
    give back {"increment": increment, "get_count": get_count}

let counter be make_counter()
display counter["get_count"]()         # 0
display counter["increment"]()         # 1
display counter["increment"]()         # 2
display counter["get_count"]()         # 2
```

#### Upvalue Capture

```vex
fn outer():
    let x be 10
    
    fn inner():
        display x              # Upvalue (captured variable)
    
    fn modify():
        x be x + 1             # Modify captured variable
    
    give back (inner, modify)

let (read, write) be outer()
read()                         # 10
write()
read()                         # 11
```

### Function Scope & Recursion

#### Recursion

```vex
fn factorial(n):
    if n less than or equal 1:
        give back 1
    give back n * factorial(n - 1)

display factorial(5)       # 120

fn fibonacci(n):
    if n less than 2:
        give back n
    give back fibonacci(n - 1) + fibonacci(n - 2)

display fibonacci(6)       # 8
```

#### Mutual Recursion

```vex
fn even(n):
    if n is 0:
        give back true
    give back odd(n - 1)

fn odd(n):
    if n is 0:
        give back false
    give back even(n - 1)

display even(4)            # true
display odd(3)             # true
```

---

## Object-Oriented Programming

### Structs (Classes)

#### Basic Struct

```vex
struct Point:
    has x
    has y
    has color

let p be Point()
p.x be 10
p.y be 20
p.color be "red"

display p.x                # 10
display p.color            # "red"
```

#### Struct with Constructor

```vex
struct Rectangle:
    has width
    has height
    has fill_color

    can init(w, h, color be "white"):
        self.width be w
        self.height be h
        self.fill_color be color

let rect be Rectangle(100, 50, "blue")
display rect.width         # 100
display rect.fill_color    # "blue"
```

#### Struct with Methods

```vex
struct Circle:
    has radius
    has center_x
    has center_y

    can area():
        let pi be 3.14159
        give back pi * self.radius * self.radius

    can diameter():
        give back 2 * self.radius

    can move(dx, dy):
        self.center_x be self.center_x + dx
        self.center_y be self.center_y + dy

let circle be Circle()
circle.radius be 5
circle.center_x be 10
circle.center_y be 10

display circle.area()      # 78.54...
display circle.diameter()  # 10
circle.move(5, 5)
display circle.center_x    # 15
```

#### Struct with Properties

```vex
struct Person:
    has first_name
    has last_name
    has birth_year

    can age():
        # Computed property
        let current_year be 2026
        give back current_year - self.birth_year

    can full_name():
        give back "{self.first_name} {self.last_name}"

let alice be Person()
alice.first_name be "Alice"
alice.last_name be "Johnson"
alice.birth_year be 1995

display alice.full_name()  # "Alice Johnson"
display alice.age()        # 31
```

### Inheritance (Single)

#### Basic Inheritance

```vex
struct Animal:
    has name
    has age

    can describe():
        give back "Animal named {self.name}, age {self.age}"

struct Dog extends Animal:
    has breed

    can describe():
        give back "Dog: {self.breed} named {self.name}"

    can bark():
        display "Woof!"

let dog be Dog()
dog.name be "Rex"
dog.age be 5
dog.breed be "German Shepherd"

display dog.describe()     # "Dog: German Shepherd named Rex"
dog.bark()                 # "Woof!"
```

#### Method Overriding

```vex
struct Vehicle:
    has max_speed

    can accelerate():
        give back "Going faster..."

struct Car extends Vehicle:
    can accelerate():
        # Override parent method
        give back "Car accelerating to {self.max_speed} mph"

struct Bicycle extends Vehicle:
    can accelerate():
        # Different behavior
        give back "Pedaling harder..."

let car be Car()
car.max_speed be 120
display car.accelerate()   # "Car accelerating to 120 mph"

let bike be Bicycle()
bike.max_speed be 25
display bike.accelerate()  # "Pedaling harder..."
```

#### Calling Parent Methods

```vex
struct Animal:
    has name

    can introduce():
        give back "I am {self.name}"

struct Dog extends Animal:
    has breed

    can introduce():
        # Access parent behavior (planned)
        let base be super.introduce()
        give back "{base}, a {self.breed}"

# Alternative: manual composition
struct Dog2:
    has animal: Animal
    has breed

    can introduce():
        let base be self.animal.introduce()
        give back "{base}, a {self.breed}"
```

### Polymorphism

#### Type Checking

```vex
struct Animal:
    can speak():
        display "Animal sound"

struct Dog extends Animal:
    can speak():
        display "Woof!"

struct Cat extends Animal:
    can speak():
        display "Meow!"

fn make_sound(animal: Animal):
    animal.speak()

let dog be Dog()
let cat be Cat()

make_sound(dog)            # "Woof!"
make_sound(cat)            # "Meow!"
```

### Structs vs Maps

#### When to Use Each

| Aspect | Struct | Map |
|--------|--------|-----|
| **Field count** | Known at compile-time | Dynamic |
| **Type safety** | Strongly typed | Loosely typed |
| **Methods** | Yes | No |
| **Performance** | Faster | Slightly slower |
| **Flexibility** | Less | More |
| **Use case** | Domain objects | Configuration, JSON |

```vex
# Use Struct for domain objects
struct User:
    has id
    has name
    has email

    can is_valid():
        give back self.email.contains("@")

# Use Map for flexible data
let config be {
    "database": "postgres",
    "port": 5432,
    "debug": true
}

# Hybrid: Struct that contains maps
struct AppConfig:
    has settings: {str: str}
    has features: {str: bool}
```

---

## Module System

### Module Organization

#### Module Path Resolution

```
# Module import
use math_lib

# Resolves to:
# 1. Check if 'math_lib' is in stdlib
# 2. Look for file: math_lib.vxm in current directory
# 3. Look for file: lib/math_lib.vxm
# 4. Look for file: lib/math/init.vxm (package)
```

#### Custom Module Structure

```
project/
├── main.vxm
├── lib/
│   ├── math.vxm        # use math
│   ├── string.vxm      # use string_utils
│   └── utils/
│       ├── init.vxm    # use utils (package)
│       ├── helpers.vxm
│       └── validators.vxm
```

### Module Import Syntax

#### Simple Import (use module)

```vex
# Import entire module
use math

# Access exported symbols
display math.sqrt(16)
display math.PI
```

#### Selective Import (from...use)

```vex
# Import specific symbols
from math use sqrt, PI, pow

# Use directly without prefix
display sqrt(16)           # 4
display PI                 # 3.14159

# Renamed import (planned)
from long_module_name use important_function as func
```

#### Aliased Import (planned)

```vex
use very.long.module.name as vl

display vl.some_function()
```

### Module Definition

#### Exporting Symbols

```vex
# math_lib.vxm - define and automatically export

const PI be 3.14159
const E be 2.71828

fn square(x):
    give back x * x

fn cube(x):
    give back x * x * x

fn power(x, n):
    give back x ^ n
```

#### Using the Module

```vex
# In another file
from math_lib use PI, square, cube

display PI                 # 3.14159
display square(5)          # 25
display cube(3)            # 27

# Or with full qualification
use math_lib
display math_lib.PI        # 3.14159
```

### Module Scope

```vex
# Private variables (module-scoped)
let version be "1.0.0"     # Not exported

# Public constants
const TIMEOUT be 5000

# Public function
fn get_timeout():
    give back TIMEOUT

# Private helper
fn internal_setup():
    pass

# Main entry point
fn initialize():
    internal_setup()
```

### Circular Dependencies

Vexium detects and prevents circular module dependencies:

```vex
# module_a.vxm
use module_b

let x be b.get_value()

# module_b.vxm
use module_a

let y be a.get_value()

# ERROR: Circular dependency detected!
```

---

## Standard Library

### Math Module

```vex
use math

# Constants
display PI                 # 3.14159265358979
display E                  # 2.71828182845905
display TAU                # 6.28318530717959

# Basic functions
display abs(-5)            # 5
display sqrt(16)           # 4.0
display pow(2, 10)         # 1024
display min(5, 3)          # 3
display max(5, 3)          # 5

# Trigonometry
display sin(PI / 2)        # 1.0
display cos(0)             # 1.0
display tan(0)             # 0.0
display asin(1)            # π/2
display acos(1)            # 0
display atan(1)            # π/4

# Rounding
display floor(3.7)         # 3
display ceil(3.2)          # 4
display round(3.5)         # 4
display trunc(3.7)         # 3

# Other
display log(10)            # Natural log
display log10(100)         # Log base 10
display exp(1)             # e^1
display random()           # Random 0.0-1.0
display random_int(1, 100) # Random 1-100
```

### String Module

```vex
use string_utils

# Or use built-in string methods
let text be "Hello World"

display text.length()      # 11
display text.upper()       # "HELLO WORLD"
display text.lower()       # "hello world"
display text.contains("World")                # true
display text.starts_with("Hello")             # true
display text.ends_with("World")               # true
display text.split(" ")    # ["Hello", "World"]
display text.replace("World", "Vexium")       # "Hello Vexium"
display text.slice(0, 5)   # "Hello"
display text.trim()        # Removes leading/trailing whitespace

# Formatting (planned)
display format("Number: {}, String: {}", 42, "test")
```

### Collections Module

```vex
from collections use map, filter, reduce, zip

let numbers be [1, 2, 3, 4, 5]

# Map
let doubled be map(numbers, fn(x): give back x * 2)
display doubled            # [2, 4, 6, 8, 10]

# Filter
let evens be filter(numbers, fn(x): give back x % 2 is 0)
display evens              # [2, 4]

# Reduce
let sum be reduce(numbers, 0, fn(acc, x): give back acc + x)
display sum                # 15

# Zip
let a be [1, 2, 3]
let b be ["a", "b", "c"]
display zip(a, b)          # [(1, "a"), (2, "b"), (3, "c")]

# List comprehensions
let squares be [x * x for x in numbers]
display squares            # [1, 4, 9, 16, 25]

let filtered be [x for x in numbers where x > 2]
display filtered           # [3, 4, 5]
```

### File System Module

```vex
use fs

# Read file
let content be fs.read_file("input.txt")
display content

# Write file
fs.write_file("output.txt", "Hello, World!")

# Check file exists
if fs.exists("data.vxm"):
    display "File found"

# File operations
let size be fs.file_size("data.vxm")
let modified be fs.modified_time("data.vxm")

# Directory operations
fs.create_directory("new_folder")
let files be fs.list_directory("./")
fs.delete_file("temp.txt")
fs.delete_directory("old_folder")
```

### JSON Module

```vex
use json

# Parse JSON string to Vexium value
let json_str be '{"name": "Alice", "age": 30, "hobbies": ["reading", "coding"]}'
let data be json.parse(json_str)

display data["name"]       # "Alice"
display data["age"]        # 30
display data["hobbies"]    # ["reading", "coding"]

# Convert Vexium value to JSON string
let person be {
    "name": "Bob",
    "age": 25,
    "active": true
}

let json_output be json.stringify(person)
display json_output        # {"name":"Bob","age":25,"active":true}

# Pretty printing
let pretty be json.stringify(person, with_format: true)
```

### Time Module

```vex
use time

# Current time
let now be time.now()      # Unix timestamp in seconds
display now

# Sleep
time.sleep(1000)           # Sleep 1000ms (1 second)

# Time formatting
let formatted be time.format(now, "%Y-%m-%d %H:%M:%S")
display formatted          # "2026-03-05 14:30:45"

# High-resolution clock
let start be time.clock()
# ... do work ...
let elapsed be time.clock() - start
display elapsed            # Time in seconds (with decimals)

# Time arithmetic (planned)
let future be time.add_hours(now, 24)
let past be time.subtract_days(now, 7)
```

### HTTP Module

```vex
use http

# GET request
let response be http.get("https://api.example.com/data")
display response.status    # 200
display response.body      # Response content
display response.headers   # Response headers

# POST request
let data be {
    "username": "alice",
    "email": "alice@example.com"
}
let response be http.post("https://api.example.com/users", data)

# PUT request (planned)
let response be http.put("https://api.example.com/user/1", updated_data)

# DELETE request (planned)
let response be http.delete("https://api.example.com/user/1")
```

---

## Error Handling

### Try-Catch (attempt-otherwise)

#### Basic try-catch

```vex
attempt:
    let result be risky_operation()
    display result
otherwise error:
    display "Error occurred: {error.message}"
```

#### Error object

```vex
attempt:
    divide(10, 0)
otherwise err:
    display err.message      # "Division by zero"
    display err.type         # "ValueError"
    display err.line         # Source line number
```

#### Without error variable

```vex
attempt:
    file_read("missing.txt")
otherwise:
    display "File not found, using default"
    let data be "default content"
```

#### Nested try-catch

```vex
attempt:
    attempt:
        inner_operation()
    otherwise inner_err:
        display "Inner error: {inner_err.message}"
        throw "Nested error"
otherwise outer_err:
    display "Outer error: {outer_err.message}"
```

### Throwing Errors

#### Basic throw

```vex
fn validate_age(age):
    if age less than 0:
        throw "Age cannot be negative"
    if age greater than 150:
        throw "Age seems unrealistic"
    give back age

attempt:
    let age be validate_age(-5)
otherwise err:
    display err.message      # "Age cannot be negative"
```

#### Custom error types

```vex
fn validate_email(email):
    if not email.contains("@"):
        throw "ValidationError: Invalid email format"

fn connect_to_database(url):
    if url is "":
        throw "ConnectionError: Empty database URL"

attempt:
    validate_email("invalid")
otherwise err:
    if err.type is "ValidationError":
        display "Validation failed"
    else:
        display "Unknown error"
```

### Error Propagation

```vex
fn read_and_process(filename):
    let content be read_file(filename)     # Might throw
    let data be parse_json(content)        # Might throw
    give back data

fn main():
    attempt:
        let data be read_and_process("data.json")
        display data
    otherwise err:
        display "Failed to process file: {err.message}"
```

### Error Types

| Type | Example | Cause |
|------|---------|-------|
| `ValueError` | `throw "Invalid input"` | Invalid value/operation |
| `TypeError` | `throw "Type mismatch"` | Type error |
| `IndexError` | `throw "Index out of bounds"` | Array/string access |
| `KeyError` | `throw "Key not found"` | Map lookup |
| `IOError` | `throw "File not found"` | File/network operation |
| `RuntimeError` | `throw "Unexpected state"` | General runtime error |
| `AssertionError` | `assert condition` (fails) | Assertion violation |

---

## Bytecode Compilation & VM

### Compilation Process

```
Source Code (.vxm)
       ↓
   Lexer
       ↓
   Token stream
       ↓
   Parser
       ↓
   AST (Abstract Syntax Tree)
       ↓
   Compiler
       ↓
   Bytecode Instructions
       ↓
   Stack-based VM
       ↓
   Output/Results
```

### Value Representation (NaN-boxing)

Vexium uses **NaN-boxing** for efficient value representation:

```c
typedef union {
    double num;                    // IEEE 754 double
    struct {
        uint32_t tag;              // Type tag
        void* ptr;                 // Pointer to data
    } obj;
} Value;

// NaN bits detect type:
// If exponent is 0x7FF and mantissa is non-zero: object
// Otherwise: number (int or float)
```

**Benefits**:
- Single 64-bit representation for all values
- Fast type checking via bit patterns
- No memory overhead per value
- Efficient array/map storage

### Bytecode Instructions

#### Core Opcodes

| Opcode | Stack Effect | Purpose |
|--------|--------------|---------|
| `OP_CONSTANT(idx)` | `→ value` | Push constant |
| `OP_POP` | `value →` | Discard top |
| `OP_DUP` | `a → a a` | Duplicate top |
| `OP_ADD` | `a b → a+b` | Integer add |
| `OP_SUBTRACT` | `a b → a-b` | Integer subtract |
| `OP_MULTIPLY` | `a b → a*b` | Integer multiply |
| `OP_DIVIDE` | `a b → a/b` | Integer divide |
| `OP_MODULO` | `a b → a%b` | Integer modulo |
| `OP_NEGATE` | `a → -a` | Negate number |
| `OP_EQUAL` | `a b → a==b` | Equality |
| `OP_LESS` | `a b → a<b` | Less than |
| `OP_GREATER` | `a b → a>b` | Greater than |
| `OP_AND` | `a b → a&&b` | Logical AND |
| `OP_OR` | `a b → a\|\|b` | Logical OR |
| `OP_NOT` | `a → !a` | Logical NOT |
| `OP_PRINT` | `a →` | Print value |
| `OP_RETURN` | `a →` | Return from function |
| `OP_CALL(args)` | `fn arg... → result` | Function call |
| `OP_JUMP(offset)` | `→` | Unconditional jump |
| `OP_JUMP_IF_FALSE` | `a → a` | Conditional jump |
| `OP_ARRAY(count)` | `v... → [v...]` | Create array |
| `OP_MAP(count)` | `k v... → {k:v...}` | Create map |
| `OP_GET_FIELD` | `obj → obj.field` | Field access |
| `OP_SET_FIELD` | `obj val →` | Field assignment |

### Virtual Machine Stack

```
Stack Layout During Execution:

┌─────────────────────┐
│    (empty)          │  SP (stack pointer)
├─────────────────────┤
│  Local Var 1 (slot) │
├─────────────────────┤
│  Local Var 0        │
├─────────────────────┤
│  Param 2            │
├─────────────────────┤
│  Param 1            │
├─────────────────────┤
│  Param 0            │
├─────────────────────┤
│  Call Frame Info    │  FP (frame pointer)
├─────────────────────┤
│  Previous Frame     │
└─────────────────────┘
```

### Compilation Example

#### Source Code

```vex
fn add(a, b):
    give back a + b

let result be add(5, 3)
display result
```

#### Compiled Bytecode

```
0000  CONSTANT(5)              # Push 5
0001  CONSTANT(3)              # Push 3
0002  CALL(2)                  # Call function with 2 args
0003  SET_GLOBAL("result")     # Store in result
0004  GET_GLOBAL("result")     # Load result
0005  PRINT                    # Print
0006  RETURN
```

### Optimization Techniques

#### Constant Folding

```vex
# Source
let x be 2 + 3 + 4 * 5

# Compiled
CONSTANT(22)               # Pre-computed: 2 + 3 + 4*5 = 22
```

#### Dead Code Elimination

```vex
# Source
let x be 5
let y be 10
display y

# Compiled
CONSTANT(10)
SET_GLOBAL("y")            # x assignment eliminated
```

### Garbage Collection

#### Mark-and-Sweep Algorithm

1. **Mark phase**: Traverse all reachable objects from roots
2. **Sweep phase**: Collect unmarked objects

```c
// Roots (starting points for marking)
- Stack values
- Global variables
- Call frame local variables
- Upvalues in closures
```

#### GC Triggering

```c
// Triggered when:
if (allocated_bytes > threshold) {
    gc_collect_garbage();
    
    // Threshold grows if GC freed < 25% of heap
    if (freed_bytes < allocated_bytes / 4) {
        next_threshold *= 1.5;
    }
}
```

---

## CLI Tooling & Commands

### Command Structure

```bash
vexium <command> [options] [file]
```

### Available Commands

#### run - Interpret & Execute

```bash
# Execute via tree-walk interpreter
vexium run hello.vxm

# With arguments (planned)
vexium run script.vxm arg1 arg2

# With options
vexium run --debug debug.vxm
vexium run --verbose script.vxm
```

#### run-vm - Bytecode Execution

```bash
# Execute via bytecode VM (faster)
vexium run-vm hello.vxm

# Benchmark mode
vexium run-vm --timing hello.vxm
```

#### build - Compile to Bytecode

```bash
# Compile source to bytecode
vexium build hello.vxm          # Creates hello.vxmc

# With options
vexium build --output custom.vxmc hello.vxm
vexium build --optimize hello.vxm
```

#### check - Syntax Validation

```bash
# Check syntax without executing
vexium check hello.vxm

# Detailed type checking (planned)
vexium check --strict hello.vxm
vexium check --type-check hello.vxm
```

#### fmt - Format Code

```bash
# Format source code (planned)
vexium fmt hello.vxm

# Write in-place
vexium fmt --write hello.vxm

# Check formatting
vexium fmt --check hello.vxm
```

#### test - Run Tests

```bash
# Run test suite
vexium test

# Run specific test file
vexium test tests/unit_tests.vxm

# With options
vexium test --verbose
vexium test --coverage
```

#### bench - Performance Benchmark

```bash
# Compare interpreter vs VM
vexium bench fibonacci.vxm

# Detailed timing
vexium bench --iterations 1000 fibonacci.vxm
```

#### disasm - Disassemble Bytecode

```bash
# Show bytecode instructions
vexium disasm hello.vxmc

# Show with constants
vexium disasm --verbose hello.vxmc
```

#### ast - Show Abstract Syntax Tree

```bash
# Display AST
vexium ast hello.vxm

# Pretty-printed
vexium ast --pretty hello.vxm

# To file
vexium ast hello.vxm > ast.txt
```

#### lex - Show Token Stream

```bash
# Display tokens
vexium lex hello.vxm

# Detailed token info
vexium lex --verbose hello.vxm
```

#### repl - Interactive Shell

```bash
# Start interactive REPL
vexium repl

# REPL commands (inside REPL)
> let x be 5
> display x
> fn add(a, b): give back a + b
> display add(3, 4)
> :help          # Show REPL help
> :quit          # Exit REPL
```

#### help - Documentation

```bash
# Show general help
vexium help

# Command-specific help
vexium help run
vexium help build
vexium help --all
```

---

## Implementation Roadmap

### Version 1.0 - Foundation ✅ (COMPLETE)

**Status**: Tree-walk interpreter fully stable

**Completed Features**:
- ✅ Lexer (tokenization)
- ✅ Parser (recursive descent, full AST)
- ✅ Tree-walk interpreter
- ✅ All data types (int, float, string, bool, array, map, nothing)
- ✅ Control flow (if/elif/else, while, for, repeat)
- ✅ Functions (definition, calls, return, recursion)
- ✅ Standard library (math, string, collections)
- ✅ Error messages with line/column info
- ✅ REPL interactive shell
- ✅ CLI commands (run, check, ast, lex)

### Version 2.0 - Performance & Modules 🚧 (35% COMPLETE)

**Current Phase**: MVP (Minimum Viable Product)

**Completed**:
- ✅ Bytecode VM architecture
- ✅ NaN-boxing value representation
- ✅ Core opcodes (40+)
- ✅ Compiler (AST → bytecode)
- ✅ Garbage collector (mark-and-sweep)
- ✅ Closures & upvalues
- ✅ List comprehensions
- ✅ Module system (loader, caching, circular dep detection)
- ✅ Field access & property modification
- ✅ Error handling (try-catch, throw)
- ✅ JSON, Time, HTTP modules
- ✅ Bytecode caching
- ✅ Constant folding
- ✅ Break/skip statements

**In Progress**:
- 🚧 Full compiler optimization pass
- 🚧 Type system (inference, gradual typing)
- 🚧 Default parameters
- 🚧 Multiple return values
- 🚧 Pattern matching (match-when)
- 🚧 Lambda/arrow functions
- 🚧 String formatting (f-strings)
- 🚧 Variadic functions

**Planned**:
- ⏳ Tail call optimization
- ⏳ JIT compilation
- ⏳ FFI (Foreign Function Interface)
- ⏳ Async/await
- ⏳ Multi-threading

### Version 2.1 - OOP Enhancement 🔮

**Planned Features**:
- Multiple inheritance (C3 linearization algorithm)
- Abstract classes & interfaces
- Private/protected members
- Static methods & fields
- Operator overloading
- Properties with getters/setters

### Version 3.0 - AI/ML Native 🔮

**Planned Features**:
- Native tensor type [T]^n
- Neural network operations
- Automatic differentiation
- GPU support
- Vector operations
- Matrix multiplication

### Version 4.0 - Concurrency 🔮

**Planned Features**:
- Green threads (coroutines)
- Async/await syntax
- Channel-based communication
- Mutex & synchronization primitives
- Multi-core scheduling
- Parallel array operations

### Version 5.0 - Self-Hosting 🔮

**Planned Features**:
- Compiler written in Vexium itself
- Language bootstrapping
- Macro system
- Compile-time reflection
- Custom DSLs

---

## Best Practices & Patterns

### Code Style & Naming

#### Naming Conventions

```vex
# Constants: UPPER_CASE
const MAX_RETRIES be 3
const DATABASE_URL be "postgresql://localhost"

# Functions & methods: snake_case
fn calculate_total(items):
    pass

# Variables: snake_case
let user_name be "Alice"
let total_count be 0

# Structs: PascalCase
struct PersonProfile:
    has name
    has email

# Private convention: _leading_underscore (not enforced)
let _internal_state be {}
```

#### Code Formatting

```vex
# Indentation: 4 spaces
if condition:
    let x be 10
    if nested:
        let y be 20

# Spacing around operators
let result be (a + b) * (c - d)

# Line length: aim for < 100 characters
let long_function_call be calculate_complex_result(param1, param2,
                                                    param3, param4)
```

### Common Patterns

#### Guard Clauses

```vex
fn process_order(order):
    # Fail fast with guard clauses
    if order is nothing:
        give back nothing
    
    if order["total"] less than 0:
        throw "Negative total"
    
    if order["items"] is nothing or len(order["items"]) is 0:
        give back nothing
    
    # Main logic
    let total be calculate_tax(order)
    give back total
```

#### Builder Pattern

```vex
struct QueryBuilder:
    has _table: str
    has _where: str
    has _limit: int

    can init(table):
        self._table be table
        self._where be ""
        self._limit be 0

    can where(condition):
        self._where be condition
        give back self

    can limit(n):
        self._limit be n
        give back self

    can build():
        let query be "SELECT * FROM {self._table}"
        if self._where is not "":
            query be query + " WHERE {self._where}"
        if self._limit greater than 0:
            query be query + " LIMIT {self._limit}"
        give back query

# Usage
let query be QueryBuilder("users").where("age > 18").limit(10).build()
display query
```

#### Strategy Pattern

```vex
# Define strategies as functions
fn ascending_sort(a, b):
    if a less than b: give back -1
    if a greater than b: give back 1
    give back 0

fn descending_sort(a, b):
    if a greater than b: give back -1
    if a less than b: give back 1
    give back 0

# Use strategy in generic function
fn sort_by(items, strategy):
    # Implementation uses strategy function
    pass

# Usage
let ascending be sort_by([3, 1, 2], ascending_sort)
let descending be sort_by([3, 1, 2], descending_sort)
```

#### Composition over Inheritance

```vex
# Rather than deep inheritance hierarchies
struct Logger:
    can log(message):
        display "{time()}: {message}"

struct Database:
    can query(sql):
        pass

# Compose behaviors
struct Application:
    has logger: Logger
    has database: Database

    can run():
        self.logger.log("Starting")
        self.database.query("SELECT ...")
        self.logger.log("Done")
```

### Error Handling Patterns

#### Defensive Programming

```vex
fn safe_divide(a, b):
    if b is 0:
        throw "Division by zero"
    give back a / b

fn safe_get_array_item(arr, index):
    if index less than 0 or index greater than or equal len(arr):
        give back nothing
    give back arr[index]
```

#### Try-Catch with Recovery

```vex
fn load_config(filename):
    let config be nothing
    
    attempt:
        let content be read_file(filename)
        config be parse_json(content)
    otherwise:
        display "Config file not found, using defaults"
        config be get_default_config()
    
    give back config
```

### Performance Optimization

#### Avoid Repeated Lookups

```vex
# ❌ Inefficient - looked up 3 times
display map["expensive_key"]
map["expensive_key"] be map["expensive_key"] + 1
let total be map["expensive_key"]

# ✅ Efficient - looked up once
let value be map["expensive_key"]
display value
value be value + 1
map["expensive_key"] be value
let total be value
```

#### Pre-allocate Collections

```vex
# ❌ Inefficient - grows array 100 times
let items be []
for i in 0 to 100:
    items.push(i)

# ✅ More efficient (when available)
let items be array_with_capacity(100)
for i in 0 to 100:
    items.push(i)
```

#### Use Appropriate Data Structures

```vex
# ❌ Inefficient for lookups (linear search)
let ids be [1, 2, 3, 10, 20, 50]
if contains(ids, 20):          # O(n)
    pass

# ✅ More efficient for lookups
let id_set be {1: true, 2: true, 3: true, 10: true, 20: true}
if id_set.contains(20):        # O(1)
    pass
```

### Testing Practices

#### Unit Testing

```vex
fn test_arithmetic():
    assert add(2, 3) is 5
    assert subtract(10, 3) is 7
    assert multiply(4, 5) is 20
    display "✓ Arithmetic tests pass"

fn test_string_operations():
    assert len("hello") is 5
    assert "hello".upper() is "HELLO"
    assert "a,b,c".split(",") is ["a", "b", "c"]
    display "✓ String tests pass"

# Run tests
test_arithmetic()
test_string_operations()
```

#### Test Organization

```vex
struct TestRunner:
    has test_count: int
    has pass_count: int

    can init():
        self.test_count be 0
        self.pass_count be 0

    can run_test(name, test_fn):
        self.test_count be self.test_count + 1
        attempt:
            test_fn()
            self.pass_count be self.pass_count + 1
            display "✓ {name}"
        otherwise err:
            display "✗ {name}: {err.message}"

    can report():
        display "{self.pass_count}/{self.test_count} tests passed"
```

---

## Summary

### What Makes Vexium Unique

| Feature | Benefit |
|---------|---------|
| **Python-like syntax** | Easy to learn, readable code |
| **OS-first design** | Direct hardware access when needed |
| **AI-native features** | Built-in tensor operations |
| **Dual execution modes** | Choose speed (VM) or simplicity (interpreter) |
| **Modern type system** | Gradual typing with inference |
| **Module system** | Code reuse and organization |
| **Comprehensive stdlib** | Math, JSON, HTTP, File I/O built-in |
| **GC memory management** | No manual memory management |
| **Efficient VM** | NaN-boxing optimization |

### Key Files for Understanding

| File | Purpose |
|------|---------|
| `lexer.c` | Tokenization |
| `parser.c` | Syntax analysis |
| `ast.c` | AST node definitions |
| `interpreter.c` | Tree-walk execution |
| `compiler.c` | AST → Bytecode |
| `vm.c` | Stack-based bytecode execution |
| `type_system.c` | Type inference & checking |
| `gc.c` | Garbage collection |
| `module_loader.c` | Module imports |
| `stdlib.c` | Built-in functions |

### Where to Go Next

1. **Learn the Language**: Review `VEXIUM_QUICK_START.md`
2. **Understand OOP**: See `VEXIUM_OOP.md`
3. **Explore Examples**: Check `VEXIUM_EXAMPLES.md`
4. **Deep Dive**: Read `language_spec.md` and `system_design.md`
5. **Implement Features**: Review `IMPLEMENTATION_ROADMAP.md`
6. **Troubleshoot**: Consult `VEXIUM_DEBUGGING.md` and `VEXIUM_FAQ.md`

---

## Conclusion

Vexium is a modern, well-designed programming language that bridges the gap between high-level readability and low-level control. Whether you're building AI/ML applications, systems software, or general-purpose programs, Vexium provides the tools and abstractions you need.

The language continues to evolve with planned features like multiple inheritance, improved tooling, and AI-native operations in the roadmap. The community and implementation are active, with comprehensive documentation and examples available.

For more information, see the supporting documentation files in the repository.

---

*Document Generated: 2026-03-05*  
*Vexium Version: 2.0 (MVP)*  
*Status: Comprehensive Guide Complete*
