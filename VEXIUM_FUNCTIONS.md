# 📚 Vexium Functions Guide

Everything you need to know about functions in Vexium.

---

## Table of Contents

1. [Basic Functions](#basic-functions)
2. [Parameters & Arguments](#parameters--arguments)
3. [Return Values](#return-values)
4. [Scope & Closures](#scope--closures)
5. [Lambda Functions](#lambda-functions)
6. [Higher-Order Functions](#higher-order-functions)
7. [Recursion](#recursion)
8. [Best Practices](#best-practices)

---

## Basic Functions

### Defining Functions

```vex
# Syntax:
# fn function_name(parameters):
#     function body
#     give back value

fn hello():
    display "Hello!"

fn add(a, b):
    give back a + b

fn greet(name):
    display "Hello, " + name
```

### Calling Functions

```vex
hello()                       # Call with no arguments
display add(5, 3)             # Call with arguments
display add(10, 20)           # Returns 30

greet("Alice")
```

### Functions without Return

```vex
fn print_info(name, age):
    display "Name: " + name
    display "Age: " + str(age)

print_info("Bob", 30)
# Output:
# Name: Bob
# Age: 30

# implicitly returns nothing
```

### Functions with Return

```vex
fn square(x):
    give back x * x

let result be square(5)       # result = 25

fn divide(a, b):
    if b is 0:
        give back nothing     # Return early
    give back a / b

display divide(10, 2)         # 5
display divide(10, 0)         # nothing
```

---

## Parameters & Arguments

### Required Parameters

```vex
fn greet(greeting, name):
    give back greeting + ", " + name

display greet("Hello", "Alice")   # "Hello, Alice"
display greet("Hi", "Bob")        # "Hi, Bob"

# Missing argument: ERROR
# display greet("Hello")          # Wrong number of args
```

### Default Parameters

```vex
fn power(base, exponent be 2):
    let result be 1
    for i in 1 to exponent + 1:
        result be result * base
    give back result

display power(5)              # 5^2 = 25
display power(3, 3)           # 3^3 = 27
display power(2, 5)           # 2^5 = 32
```

### Variable Arguments (Multiple Defaults)

```vex
fn create_greeting(greeting be "Hello", name be "World", punctuation be "!"):
    give back greeting + ", " + name + punctuation

display create_greeting()                          # Hello, World!
display create_greeting("Hi")                      # Hi, World!
display create_greeting("Hey", "Alice")            # Hey, Alice!
display create_greeting("Welcome", "Bob", "?")    # Welcome, Bob?
```

### Parameter Order

```vex
# Keep required params first, defaults at end
fn send_email(to, subject, body be "", cc be ""):
    # Good: to and subject required, others optional
    display "Sending to: " + to

# ❌ Avoid
# fn bad(param be "default", required):
#     # Can't have required after default!
```

---

## Return Values

### Early Returns

```vex
fn is_valid(name):
    if name is nothing:
        give back false
    
    if len(name) is 0:
        give back false
    
    if len(name) is greater than 100:
        give back false
    
    give back true

display is_valid("Alice")         # true
display is_valid("")              # false
```

### Multiple Returns (via tuple-like approach)

```vex
# Return a map (dictionary)
fn divide_with_remainder(a, b):
    if b is 0:
        give back nothing
    
    let quotient be a / b
    let remainder be a % b
    
    give back {
        "quotient": quotient,
        "remainder": remainder
    }

let result be divide_with_remainder(17, 5)
display result.quotient           # 3
display result.remainder          # 2
```

---

## Scope & Closures

### Local vs Global Scope

```vex
let global_var be "I'm global"

fn test_scope():
    let local_var be "I'm local"
    
    display global_var            # ✅ Can access global
    display local_var             # ✅ Can access local
    
    local_var be "Changed"        # Modifies local copy

fn test_scope2():
    # display local_var           # ❌ ERROR: undefined
    display global_var            # ✅ Still visible

test_scope()
test_scope2()
```

### Function Scope Inside Functions

```vex
fn outer():
    let x be 10
    
    fn inner():
        display x             # Can access outer's x
    
    inner()                   # Prints 10

outer()
# inner() here would be ERROR - inner is local to outer
```

### Closures with Lambdas

```vex
fn make_multiplier(factor):
    # Returns a function that "remembers" factor
    give back (x) => x * factor

let double be make_multiplier(2)
let triple be make_multiplier(3)

display double(5)             # 10
display triple(5)             # 15

let multiply_by_10 be make_multiplier(10)
display multiply_by_10(7)     # 70
```

### Closure Gotcha

```vex
let funcs be []

for i in 1 to 3:
    # Each lambda captures 'i', but be careful!
    push(funcs, (x) => x * i)

display funcs.0(5)            # 15 (5 * 3, not 5 * 1!)
# All functions see the final value of i
```

---

## Lambda Functions

### Basic Lambdas

```vex
# Syntax: (params) => expression

let add be (a, b) => a + b
display add(3, 4)             # 7

let square be (x) => x * x
display square(5)             # 25

let greet be (name) => "Hello, " + name
display greet("Alice")        # "Hello, Alice"
```

### Lambda with No Parameters

```vex
let get_pi be () => 3.14159
display get_pi()              # 3.14159

let get_random be () => 42
display get_random()          # 42
```

### Lambda with Multiple Statements

```vex
let process be (x) => {
    let doubled be x * 2
    let plus_one be doubled + 1
    give back plus_one
}

display process(5)            # 11
```

### Lambda as Default Parameter

```vex
fn apply(func, value):
    give back func(value)

# Using with inline lambda
display apply((x) => x * 2, 5)        # 10
display apply((x) => x + 10, 5)       # 15
display apply((x) => x ** 2, 5)       # 25
```

### Storing Lambdas

```vex
let operations be {
    "add": (a, b) => a + b,
    "subtract": (a, b) => a - b,
    "multiply": (a, b) => a * b,
    "divide": (a, b) => a / b
}

display operations.add(10, 5)             # 15
display operations.multiply(10, 5)        # 50
```

---

## Higher-Order Functions

### Functions as Parameters

```vex
fn apply_twice(func, value):
    let once be func(value)
    give back func(once)

let double be (x) => x * 2
display apply_twice(double, 5)        # 20 (5 → 10 → 20)

display apply_twice((x) => x + 1, 5)  # 7 (5 → 6 → 7)
```

### Functions Returning Functions

```vex
fn make_adder(amount):
    give back (x) => x + amount

let add_5 be make_adder(5)
let add_10 be make_adder(10)

display add_5(20)             # 25
display add_10(20)            # 30
```

### Filtering with Functions

```vex
fn filter(list, predicate):
    let result be []
    for each item in list:
        if predicate(item):
            push(result, item)
    give back result

let numbers be [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

let evens be filter(numbers, (n) => n % 2 is 0)
display evens                 # [2, 4, 6, 8, 10]

let big be filter(numbers, (n) => n is greater than 5)
display big                   # [6, 7, 8, 9, 10]
```

### Mapping with Functions

```vex
fn map(list, transform):
    let result be []
    for each item in list:
        push(result, transform(item))
    give back result

let numbers be [1, 2, 3, 4, 5]

let doubled be map(numbers, (x) => x * 2)
display doubled               # [2, 4, 6, 8, 10]

let squares be map(numbers, (x) => x * x)
display squares               # [1, 4, 9, 16, 25]
```

### Reducing (Folding)

```vex
fn reduce(list, initial, accumulator):
    let result be initial
    for each item in list:
        result be accumulator(result, item)
    give back result

let numbers be [1, 2, 3, 4, 5]

# Sum
let sum be reduce(numbers, 0, (acc, n) => acc + n)
display sum                   # 15

# Product
let product be reduce(numbers, 1, (acc, n) => acc * n)
display product               # 120

# Concatenate
let words be ["Hello", " ", "World"]
let sentence be reduce(words, "", (acc, w) => acc + w)
display sentence              # "Hello World"
```

---

## Recursion

### Simple Recursion

```vex
fn factorial(n):
    if n is at most 1:
        give back 1
    give back n * factorial(n - 1)

display factorial(5)          # 120
display factorial(10)         # 3628800
```

### Fibonacci Series

```vex
fn fib(n):
    if n is at most 1:
        give back n
    give back fib(n - 1) + fib(n - 2)

display fib(10)               # 55
display fib(15)               # 610
```

### Binary Search

```vex
fn binary_search(arr, target, left, right):
    if left is greater than right:
        give back -1          # Not found
    
    let mid be (left + right) / 2
    let mid_val be arr.mid
    
    if mid_val is target:
        give back mid
    elif mid_val is less than target:
        give back binary_search(arr, target, mid + 1, right)
    else:
        give back binary_search(arr, target, left, mid - 1)

let numbers be [1, 3, 5, 7, 9, 11, 13, 15]
display binary_search(numbers, 7, 0, 7)   # 3
display binary_search(numbers, 11, 0, 7)  # 5
```

### Tree Traversal

```vex
struct TreeNode:
    has value
    has left
    has right

    can init(val):
        self.value be val
        self.left be nothing
        self.right be nothing

fn inorder(node):
    if node is nothing:
        give back
    
    inorder(node.left)
    display node.value
    inorder(node.right)

# Build tree
let root be TreeNode(1)
root.left be TreeNode(2)
root.right be TreeNode(3)
root.left.left be TreeNode(4)
root.left.right be TreeNode(5)

# Traverse
inorder(root)                 # 4, 2, 5, 1, 3
```

---

## Best Practices

### 1. Use Meaningful Names

```vex
# ✅ Good
fn calculate_total_price(items, tax_rate):
    let subtotal be 0
    for each item in items:
        subtotal be subtotal + item.price
    give back subtotal * (1 + tax_rate)

# ❌ Avoid
fn calc(i, t):
    let s be 0
    for each x in i:
        s be s + x.p
    give back s * (1 + t)
```

### 2. Single Responsibility

```vex
# ✅ Good: Each function does one thing
fn is_valid_email(email):
    # Just validates
    give back email contains "@"

fn send_email(to, subject, body):
    # Just sends

fn validate_and_send(email, subject, body):
    # Combines the two
    if is_valid_email(email):
        send_email(email, subject, body)

# ❌ Avoid: Functions that do too much
fn send_if_valid(email, subject, body):
    # Mixes validation and sending
    if email contains "@":
        display "Sending..."
        # ... lots of complex logic
    else:
        display "Invalid"
```

### 3. Use Default Parameters

```vex
# ✅ Good: Flexible interface
fn create_item(name, category be "General", priority be 5):
    give back {"name": name, "category": category, "priority": priority}

# ❌ Avoid: Overloading functions
fn create_item_basic(name):
    give back {"name": name, "category": "General", "priority": 5}

fn create_item_detailed(name, category):
    give back {"name": name, "category": category, "priority": 5}
```

### 4. Return Early

```vex
# ✅ Good: Early returns reduce nesting
fn process(value):
    if value is nothing:
        give back nothing
    
    if value is less than 0:
        give back 0
    
    # Main logic at top level
    give back value * 2

# ❌ Avoid: Deep nesting
fn process_bad(value):
    if value is not nothing:
        if value is greater than 0:
            give back value * 2
        else:
            give back 0
    else:
        give back nothing
```

### 5. Use Lambdas for Simple Operations

```vex
# ✅ Good: Inline lambdas for one-liners
let doubled be [x * 2 for x in numbers]
let evens be [x for x in numbers if x % 2 is 0]

# ✅ Also good: Named functions for complex logic
fn process_item(item):
    let scored be item.score * 2
    let adjusted be scored - item.penalty
    give back max(0, adjusted)

let processed be [process_item(x) for x in items]
```

### 6. Document Complex Functions

```vex
# Calculate compound interest
# Principal: initial amount
# Rate: annual interest rate (0.05 = 5%)
# Time: years
# Compounds: times per year
fn compound_interest(principal, rate, time, compounds):
    let factor be rate / compounds
    let exponent be compounds * time
    give back principal * ((1 + factor) ** exponent)

display compound_interest(1000, 0.05, 10, 4)
```

---

## Common Patterns

### Memoization

```vex
let cache be {}

fn fibonacci_memo(n):
    if n in cache:
        give back cache.n
    
    if n is at most 1:
        cache.n be n
        give back n
    
    let result be fibonacci_memo(n - 1) + fibonacci_memo(n - 2)
    cache.n be result
    give back result
```

### Pipeline / Chain

```vex
fn pipeline(value, funcs):
    let result be value
    for each func in funcs:
        result be func(result)
    give back result

let add_5 be (x) => x + 5
let double be (x) => x * 2
let square be (x) => x * x

let result be pipeline(3, [add_5, double, square])
# 3 → 8 → 16 → 256
display result
```

---

This guide covers functions comprehensively. For more on other topics, check other guides in the documentation.
