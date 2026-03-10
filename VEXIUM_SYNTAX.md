# 📖 Vexium Complete Syntax Reference

A comprehensive guide to all Vexium syntax and language features.

---

## Table of Contents

1. [Variables](#variables)
2. [Data Types](#data-types)
3. [Operators](#operators)
4. [Control Flow](#control-flow)
5. [Functions](#functions)
6. [Data Structures](#data-structures)
7. [Object-Oriented Programming](#object-oriented-programming)
8. [Advanced Features](#advanced-features)
9. [Built-in Functions](#built-in-functions)
10. [Modules](#modules)

---

## Variables

### Declaration

```vex
# Mutable variable
let name be "Alice"           # Can be reassigned
let count be 0
count be count + 1            # 1

# Constant
const PI be 3.14159           # Cannot be reassigned
const AUTHOR be "Vex Team"
```

### Naming Rules

- Start with letter or underscore: `_private`, `myVar`
- Contains letters, digits, underscores: `var_name`, `var123`
- Case-sensitive: `name` ≠ `Name` ≠ `NAME`

### Scope

```vex
let global_var be 10

fn test():
    let local_var be 5        # Local to test()
    display global_var        # Can see global

fn test2():
    # display local_var       # ERROR: not visible here
    display global_var        # OK: still visible

# Block scope
if true:
    let block_var be 20
    display block_var         # OK inside block

# display block_var          # ERROR: outside block
```

---

## Data Types

### Primitives

```vex
# Integer
let age be 25
let count be -10
let large be 1000000

# Float
let pi be 3.14159
let ratio be 0.5
let scientific be 1.5e10

# String
let greeting be "Hello"
let empty be ""
let quote be 'Single quoted works too'

# Boolean
let is_active be true
let is_empty be false

# Nothing
let result be nothing           # Absence of value
```

### Composite Types

```vex
# Array
let numbers be [1, 2, 3, 4, 5]
let mixed be [1, "two", 3.0, true]
let empty_list be []

# Associative Array (Map)
let person be {"name": "Alice", "age": 25}
let settings be {"debug": true, "level": 5}
let empty_map be {}
```

### Type Checking

```vex
let x be 42
display type(x)               # "int"

let y be 3.14
display type(y)               # "float"

let s be "hello"
display type(s)               # "string"

let arr be [1, 2, 3]
display type(arr)             # "array"

let b be true
display type(b)               # "bool"

let n be nothing
display type(n)               # "nothing"
```

---

## Operators

### Arithmetic

```vex
let a be 10
let b be 3

display a + b                 # 13 (addition)
display a - b                 # 7  (subtraction)
display a * b                 # 30 (multiplication)
display a / b                 # 3  (division)
display a % b                 # 1  (modulo/remainder)
display a ** b                # 1000 (exponentiation)
```

### Comparison

```vex
display 5 is equal to 5       # true
display 5 is not 3            # true
display 5 is greater than 3   # true
display 5 is less than 3      # false
display 5 is at least 5       # true
display 5 is at most 10       # true
```

### Logical

```vex
display true and true         # true
display true and false        # false
display true or false         # true
display not true              # false
```

### String Operations

```vex
let s1 be "Hello"
let s2 be " World"

display s1 + s2               # "Hello World" (concatenation)
display "ha" * 3              # "hahaha" (repetition)

# Comparison
display "apple" is "apple"    # true
display "apple" is not "banana" # true
```

### Array/Map Operations

```vex
let arr be [1, 2, 3]
let map be {"a": 1, "b": 2}

# Access
display arr.0                 # 1 (first element)
display arr.2                 # 3 (third element)
display map.a                 # 1
display map.b                 # 2

# Length
display len(arr)              # 3
display len(map)              # 2
```

### Membership

```vex
let fruits be ["apple", "banana", "cherry"]

if "apple" in fruits:
    display "Found!"          # Prints

if "orange" not in fruits:
    display "Not found"       # Prints
```

---

## Control Flow

### If / Elif / Else

```vex
let age be 25

if age is less than 13:
    display "Child"
elif age is less than 18:
    display "Teen"
elif age is less than 65:
    display "Adult"           # This runs
else:
    display "Senior"

# Single line
if age is greater than 18:
    display "Adult"
```

### While Loop

```vex
let count be 0

while count is less than 5:
    display count
    count be count + 1
    # Output: 0, 1, 2, 3, 4
```

### For Range Loop

```vex
# Loop from 1 to 5 (inclusive)
for i in 1 to 5:
    display i                 # 1, 2, 3, 4, 5

# With step (advanced)
for i in 1 to 10 step 2:
    display i                 # 1, 3, 5, 7, 9
```

### For Each Loop

```vex
let fruits be ["apple", "banana", "cherry"]

for each fruit in fruits:
    display fruit
    # Output: apple, banana, cherry

let person be {"name": "Alice", "age": 25}

for each key in person:
    display key               # name, age
```

### Repeat

```vex
repeat 3 times:
    display "Vex!"
    # Output: Vex!, Vex!, Vex!

# Repeat with variable
let n be 5
repeat n times:
    display "x"               # x printed 5 times
```

### Break & Skip

```vex
for i in 1 to 10:
    if i is 5:
        break                 # Exit loop
    display i                 # 1, 2, 3, 4

display "---"

for i in 1 to 10:
    if i is 5:
        skip                  # Skip this iteration
    display i                 # 1, 2, 3, 4, 6, 7, 8, 9, 10
```

### Match Statement

```vex
let day be 3

match day:
    1 => display "Monday"
    2 => display "Tuesday"
    3 => display "Wednesday"  # This runs
    4 => display "Thursday"
    5 => display "Friday"
    nothing => display "Invalid"
```

---

## Functions

### Basic Function

```vex
fn add(a, b):
    give back a + b

display add(3, 4)             # 7
```

### Function without Return

```vex
fn greet(name):
    display "Hello, " + name

greet("Alice")                # Output: Hello, Alice
```

### Default Parameters

```vex
fn power(base, exp be 2):
    let result be 1
    for i in 1 to exp + 1:
        result be result * base
    give back result

display power(5)              # 25 (5^2)
display power(3, 3)           # 27 (3^3)
```

### Recursive Functions

```vex
fn factorial(n):
    if n is at most 1:
        give back 1
    give back n * factorial(n - 1)

display factorial(5)          # 120
display factorial(10)         # 3628800

fn fibonacci(n):
    if n is at most 1:
        give back n
    give back fibonacci(n - 1) + fibonacci(n - 2)

display fibonacci(10)         # 55
```

### Lambda Functions (Anonymous)

```vex
# Simple lambda
let add be (a, b) => a + b
display add(3, 4)             # 7

# One parameter
let square be (x) => x * x
display square(5)             # 25

# No parameters
let get_pi be () => 3.14159
display get_pi()              # 3.14159

# Multi-line lambda (assign to variable)
let process be (x) => {
    let result be x * 2
    give back result + 1
}
display process(5)            # 11
```

### Higher-Order Functions

```vex
fn apply_twice(func, value):
    let first be func(value)
    give back func(first)

let double be (x) => x * 2

display apply_twice(double, 5) # 20 (5 → 10 → 20)

# Or with lambda inline
display apply_twice((x) => x + 1, 5) # 7
```

---

## Data Structures

### Arrays

```vex
let nums be [1, 2, 3, 4, 5]

# Access
display nums.0                # 1
display nums.4                # 5

# Modify
nums.2 be 10                  # Now [1, 2, 10, 4, 5]

# Length
display len(nums)             # 5

# Add to end
push(nums, 6)                 # [1, 2, 10, 4, 5, 6]

# Get last and remove
let last be pop(nums)         # last = 6, nums = [1, 2, 10, 4, 5]

# Check membership
if 2 in nums:
    display "Found"
```

### Maps/Dictionaries

```vex
let person be {
    "name": "Alice",
    "age": 25,
    "city": "New York"
}

# Access
display person.name           # "Alice"
display person.age            # 25

# Modify
person.age be 26

# Add new field
person.job be "Engineer"

# Check membership
if "name" in person:
    display "Name exists"

# Length
display len(person)           # 4
```

### Nested Structures

```vex
let company be {
    "name": "TechCorp",
    "employees": [
        {"name": "Alice", "role": "Engineer"},
        {"name": "Bob", "role": "Designer"}
    ],
    "location": {
        "city": "San Francisco",
        "country": "USA"
    }
}

display company.name          # "TechCorp"
display company.employees.0   # {"name": "Alice", "role": "Engineer"}
display company.employees.0.name # "Alice"
display company.location.city # "San Francisco"
```

---

## Object-Oriented Programming

### Structs (Classes)

```vex
struct Point:
    has x
    has y

    can init(x, y):
        self.x be x
        self.y be y

    can display_info():
        display "Point: (" + str(self.x) + ", " + str(self.y) + ")"

    can distance_to_origin():
        give back sqrt(self.x * self.x + self.y * self.y)

let p1 be Point(3, 4)
p1.display_info()             # Point: (3, 4)
display p1.distance_to_origin()  # 5.0
```

### Inheritance

```vex
struct Animal:
    has name
    has sound

    can init(name, sound):
        self.name be name
        self.sound be sound

    can speak():
        give back self.name + " says " + self.sound

struct Dog extends Animal:
    has breed

    can init(name, breed):
        self.name be name
        self.sound be "Woof"
        self.breed be breed

    can fetch():
        give back self.name + " fetches the ball!"

let dog be Dog("Rex", "German Shepherd")
display dog.speak()           # Rex says Woof
display dog.fetch()           # Rex fetches the ball!
display dog.breed             # German Shepherd
```

### Polymorphism

```vex
struct Shape:
    can area():
        give back 0

struct Rectangle extends Shape:
    has width
    has height

    can init(w, h):
        self.width be w
        self.height be h

    can area():
        give back self.width * self.height

struct Circle extends Shape:
    has radius

    can init(r):
        self.radius be r

    can area():
        give back 3.14159 * self.radius * self.radius

let rect be Rectangle(5, 10)
let circle be Circle(5)

display rect.area()           # 50
display circle.area()         # 78.53975
```

---

## Advanced Features

### List Comprehensions

```vex
let nums be [1, 2, 3, 4, 5]

# Map (transform each element)
let doubled be [x * 2 for x in nums]
# doubled = [2, 4, 6, 8, 10]

# Filter
let evens be [x for x in nums if x % 2 is 0]
# evens = [2, 4]

# Combine
let filtered_doubled be [x * 2 for x in nums if x is greater than 2]
# filtered_doubled = [6, 8, 10]

# Nested
let matrix be [[1, 2], [3, 4], [5, 6]]
let flat be [x for row in matrix for x in row]
# flat = [1, 2, 3, 4, 5, 6]
```

### Error Handling

```vex
attempt:
    let result be 10 / 0      # Might error
    display result
otherwise err:
    display "Error occurred: " + err
```

### String Interpolation

```vex
let name be "Alice"
let age be 25

# Old way
display "Name: " + name + ", Age: " + str(age)

# New way (interpolation)
display "Name: {name}, Age: {age}"

# With expressions
display "Next year: {age + 1}"
```

---

## Built-in Functions

### Type Conversion

```vex
display int("123")            # 123
display float("3.14")         # 3.14
display str(42)               # "42"
display str(3.14)             # "3.14"
```

### String Functions

```vex
let text be "hello world"

display upper(text)           # "HELLO WORLD"
display lower(text)           # "hello world"
display len(text)             # 11
display text contains "world" # true
```

### Math Functions

```vex
display abs(-5)               # 5
display sqrt(16)              # 4.0
display pow(2, 3)             # 8
display pow(10, 2)            # 100

# Trigonometry
display sin(0)                # 0.0
display cos(0)                # 1.0
```

### Array Functions

```vex
let arr be [1, 2, 3]

push(arr, 4)                  # [1, 2, 3, 4]
let last be pop(arr)          # last = 4, arr = [1, 2, 3]

display len(arr)              # 3
display 2 in arr              # true
```

### I/O Functions

```vex
# Display (print)
display "Hello"               # Output: Hello

# Input (read from user)
let name be input("Your name: ")
display "Hello, " + name

# Type
let x be 42
display type(x)               # "int"
```

---

## Modules

### Using Standard Modules

```vex
use math
display PI                    # 3.14159
display sqrt(16)              # 4.0

use string
display upper("hello")        # HELLO

use collections
let arr be [1, 2, 3]
display len(arr)              # 3
```

### Using User-Defined Modules

```vex
# See math_lib.vxm in examples

# Full module import
use math_lib

# Selective import
from math_lib use square
display square(5)             # 25
```

### Creating Modules

See `VEXIUM_MODULES.md` for creating your own modules.

---

## Comments

```vex
# Single line comment

# Multi-line comment
# can span multiple
# lines

display "Code here"           # Inline comment
```

---

## Naming Conventions

```vex
# Variables and functions: snake_case
let current_count be 0
fn calculate_total(values):
    give back 0

# Constants: UPPER_CASE
const MAX_RETRIES be 5
const API_URL be "https://api.example.com"

# Classes/Structs: PascalCase
struct UserAccount:
    has username
```

---

## Style Guide

```vex
# ✅ Good
fn calculate_average(numbers):
    let sum be 0
    for each num in numbers:
        sum be sum + num
    give back sum / len(numbers)

# ❌ Avoid
fn calcAverage(nums):
    s be 0
    for each n in nums:
        s be s + n
    give back s / len(nums)
```

---

## Complete Example

```vex
# Grade calculator
struct Student:
    has name
    has scores

    can init(name):
        self.name be name
        self.scores be []

    can add_score(score):
        push(self.scores, score)

    can average():
        let sum be 0
        for each score in self.scores:
            sum be sum + score
        give back sum / len(self.scores)

    can grade():
        let avg be self.average()
        if avg is at least 90:
            give back "A"
        elif avg is at least 80:
            give back "B"
        elif avg is at least 70:
            give back "C"
        elif avg is at least 60:
            give back "D"
        give back "F"

let alice be Student("Alice")
alice.add_score(95)
alice.add_score(87)
alice.add_score(92)

display "Student: " + alice.name
display "Average: " + str(alice.average())
display "Grade: " + alice.grade()
```

---

This reference covers all major Vexium syntax. For more details on specific features, see the specialized guides.
