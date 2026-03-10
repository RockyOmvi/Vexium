# 🚀 Vexium Quick Start Guide

> Learn Vexium in 30 minutes — Write your first program in 5 minutes.

---

## Installation & Setup

### Get Vexium

```bash
# Clone the repository
git clone https://github.com/vexium/vex.git
cd vex

# Build from source
make

# Run Hello World
./vexium.exe run hello.vxm
```

### Create Your First File

Create `hello.vxm`:
```vex
display "Hello, Vexium!"
```

Run it:
```bash
./vexium.exe run hello.vxm
```

Output:
```
Hello, Vexium!
```

---

## 5-Minute Rundown

### 1. Variables (2 min)

```vex
# Create a variable
let name be "Vexium"
let age be 25
let score be 95.5

# Constants (can't change)
const PI be 3.14159

# Display output
display "Name: " + name
display "Age: " + age
```

### 2. Basic Math (1 min)

```vex
let x be 10
let y be 3

display x + y       # 13 (addition)
display x - y       # 7  (subtraction)
display x * y       # 30 (multiplication)
display x / y       # 3  (division)
display x % y       # 1  (remainder)
display 2 ** 3      # 8  (exponent)
```

### 3. Control Flow (1 min)

```vex
let score be 85

if score is greater than 90:
    display "Grade A"
elif score is greater than 80:
    display "Grade B"      # This runs
else:
    display "Grade C"

# Loop
for i in 1 to 5:
    display i              # 1, 2, 3, 4, 5
```

### 4. Functions (1 min)

```vex
fn greet(name):
    give back "Hello, " + name + "!"

display greet("World")     # Hello, World!

fn add(a, b):
    give back a + b

display add(5, 3)          # 8
```

---

## Core Concepts

### Display vs Return

```vex
# display: Print to console
display "Hello"            # Output: Hello

# give back: Return from function
fn double(x):
    give back x * 2        # Return the value
    
let result be double(5)    # result = 10
```

### String Interpolation

```vex
let name be "Vexium"
let age be 25

display "Name: " + name                    # Concatenation
display "I am {name} and I'm {age}"       # Interpolation
```

### English-like Syntax

Vexium lets you write code that reads like English:

```vex
let age be 25

if age is greater than 18:
    display "Adult"

if age is at least 18 and age is at most 65:
    display "Working age"
    
if age is not 0:
    display "Age is set"
```

---

## Common Patterns

### Loop Through Items

```vex
let fruits be ["apple", "banana", "cherry"]

for each fruit in fruits:
    display fruit
```

### Create a List

```vex
let numbers be [1, 2, 3, 4, 5]
let mixed be [1, "two", 3.0, true]

display numbers.0        # First item: 1
display len(numbers)     # Length: 5
```

### Use Built-in Functions

```vex
# Math
display sqrt(16)         # 4.0
display abs(-5)          # 5
display pow(2, 3)        # 8

# String
display upper("hello")   # HELLO
display lower("HELLO")   # hello
display len("hello")     # 5

# Convert types
display int("123")       # 123
display float("3.14")    # 3.14
display str(42)          # "42"
```

---

## Next: Learn the Full Language

Once you finish this 5-minute intro:

1. **Basic Syntax** → Read `VEXIUM_SYNTAX.md`
2. **Data Types** → Read `VEXIUM_DATA_TYPES.md`
3. **Functions** → Read `VEXIUM_FUNCTIONS.md`
4. **OOP** → Read `VEXIUM_OOP.md`
5. **Advanced** → Read `VEXIUM_ADVANCED.md`

---

## Code Snippets for Copy-Paste

### Calculator
```vex
fn calculate(a, op, b):
    if op is "+":
        give back a + b
    elif op is "-":
        give back a - b
    elif op is "*":
        give back a * b
    elif op is "/":
        give back a / b
    give back 0

display calculate(10, "+", 5)      # 15
display calculate(10, "*", 3)      # 30
```

### Factorial
```vex
fn factorial(n):
    if n is at most 1:
        give back 1
    give back n * factorial(n - 1)

display factorial(5)               # 120
```

### Sum a List
```vex
let numbers be [1, 2, 3, 4, 5]
let total be 0

for each num in numbers:
    total be total + num

display total                       # 15
```

### Check if String Contains
```vex
let text be "hello world"
let search be "world"

if text contains search:
    display "Found!"
```

---

## Common Gotchas

### Assignment with `be`
```vex
# ✅ Correct
let x be 5

# ❌ Wrong
let x = 5      # This won't work
```

### String vs Number
```vex
# ✅ Correct
let age be 25          # Number
let name be "Alice"    # String

# These are different
display "25"           # String "25"
display 25             # Number 25
```

### Loop Range
```vex
# Includes both 1 and 5
for i in 1 to 5:
    display i          # 1, 2, 3, 4, 5
```

---

## Get Help

### Print Variable Type
```vex
let x be 42
display type(x)        # "int"
```

### Check Length
```vex
let list be [1, 2, 3]
display len(list)      # 3

let text be "hello"
display len(text)      # 5
```

### Read Input
```vex
let name be input("What's your name? ")
display "Hello, " + name
```

---

## You're Ready!

You now know enough to write basic Vexium programs. Next steps:
- Try the examples in `examples/` directory
- Read detailed guides for advanced features
- Join the community for help

Happy coding! 🚀
