# ❓ Vexium FAQ (Frequently Asked Questions)

Quick answers to common questions about Vexium.

---

## Table of Contents

- [Getting Started](#getting-started)
- [Basics](#basics)
- [Functions & Structures](#functions--structures)
- [Collections](#collections)
- [Object-Oriented Programming](#object-oriented-programming)
- [Advanced Topics](#advanced-topics)
- [Troubleshooting](#troubleshooting)
- [Performance](#performance)
- [Modules & Organization](#modules--organization)

---

## Getting Started

### Q: How do I install Vexium?
**A:** See [VEXIUM_INSTALL.md](VEXIUM_INSTALL.md) for detailed installation instructions.

### Q: How do I run a Vexium program?
**A:** 
```bash
vexium filename.vxm
```

### Q: What's the file extension for Vexium files?
**A:** `.vxm` - Vexium eXecutable Module

### Q: Can I use Vexium on Windows/Mac/Linux?
**A:** Yes, Vexium runs on all major platforms. Installation may vary - check [VEXIUM_INSTALL.md](VEXIUM_INSTALL.md).

### Q: Where can I find examples?
**A:** Check the `examples/` directory in the repository, or see [VEXIUM_EXAMPLES.md](VEXIUM_EXAMPLES.md).

---

## Basics

### Q: What's the difference between `let` and `const`?
**A:** 
- **`let`** - Variable that can be reassigned
- **`const`** - Constant that cannot be changed once set

```vex
let x be 5
x be 10  # OK

const PI be 3.14159
PI be 3.14  # ERROR - cannot reassign constant
```

### Q: What data types does Vexium have?
**A:** 
- **Integer** - `5`, `-10`, `0`
- **Float** - `3.14`, `-2.5`, `1.0`
- **String** - `"hello"`, `'world'`
- **Boolean** - `true`, `false`
- **Array** - `[1, 2, 3]`, `["a", "b"]`
- **Map** - `{"key": value, "name": "Alice"}`
- **Null** - `null`

### Q: How do I print to output?
**A:** Use `display`:
```vex
display "Hello, World!"
display 42
display true
```

### Q: How do I read user input?
**A:** Use `input`:
```vex
let name be input("What's your name? ")
display "Hello, " + name
```

### Q: How do I concatenate strings?
**A:** Use `+` operator:
```vex
let greeting be "Hello, " + "World"
let message be "My age is " + str(25)
```

### Q: How do I convert between types?
**A:** Use conversion functions:
```vex
str(42)        # "42"
int("42")      # 42
float("3.14")  # 3.14
bool(1)        # true
```

### Q: What are the comparison operators?
**A:**
```vex
a is b              # Equal
a is not b          # Not equal
a is greater than b # >
a is at least b     # >=
a is less than b    # <
a is at most b      # <=
```

### Q: How do I combine conditions?
**A:** Use `and`, `or`, `not`:
```vex
if age is at least 18 and has_license is true:
    display "Can drive"

if status is "active" or status is "pending":
    process()

if not is_empty:
    process()
```

---

## Functions & Structures

### Q: How do I define a function?
**A:**
```vex
fn function_name(parameter1, parameter2):
    let result be parameter1 + parameter2
    give back result
```

### Q: How do I return from a function?
**A:** Use `give back`:
```vex
fn add(a, b):
    give back a + b
```

### Q: Can functions return multiple values?
**A:** Yes, return a map or array:
```vex
fn get_coordinates():
    give back {"x": 10, "y": 20}

let coords be get_coordinates()
display coords.x  # 10
display coords.y  # 20
```

### Q: What are default parameters?
**A:** Provide a default value:
```vex
fn greet(name, greeting be "Hello"):
    display greeting + ", " + name

greet("Alice")           # Hello, Alice
greet("Bob", "Hi")       # Hi, Bob
```

### Q: What are lambda functions?
**A:** Anonymous functions:
```vex
let square be (x) => x * x
display square(5)  # 25

let add be (a, b) => a + b
display add(3, 4)  # 7
```

### Q: How do I define a struct?
**A:**
```vex
struct Person:
    has name
    has age
    has email

let person be Person()
person.name be "Alice"
person.age be 30
```

### Q: How do I initialize a struct with values?
**A:** Define an `init` method:
```vex
struct Person:
    has name
    has age

    can init(n, a):
        self.name be n
        self.age be a

let person be Person("Alice", 30)
```

### Q: Can structs have methods?
**A:** Yes, use `can`:
```vex
struct Rectangle:
    has width
    has height

    can area():
        give back self.width * self.height

    can perimeter():
        give back 2 * (self.width + self.height)

let rect be Rectangle()
rect.width be 5
rect.height be 10
display rect.area()       # 50
display rect.perimeter()  # 30
```

### Q: What's the difference between functions and methods?
**A:**
- **Functions** - Standalone, called directly
- **Methods** - Belong to struct, called on instance with `.`

```vex
fn add(a, b):
    give back a + b

struct Calculator:
    can add(a, b):
        give back a + b

add(5, 3)                      # Call function
let calc be Calculator()
calc.add(5, 3)                 # Call method
```

---

## Collections

### Q: How do I create an array?
**A:**
```vex
let numbers be [1, 2, 3, 4, 5]
let strings be ["a", "b", "c"]
let mixed be [1, "two", 3.0, true]
let empty be []
```

### Q: How do I access array elements?
**A:** Use index (0-based):
```vex
let arr be [10, 20, 30]
display arr[0]  # 10
display arr[1]  # 20
display arr[2]  # 30
```

### Q: How do I modify array elements?
**A:** Reassign by index:
```vex
let arr be [1, 2, 3]
arr[0] be 10
arr[1] be 20
display arr  # [10, 20, 3]
```

### Q: How do I add/remove from arrays?
**A:** Use built-in functions:
```vex
let arr be [1, 2, 3]

# Add
push(arr, 4)     # [1, 2, 3, 4]

# Remove
pop(arr)         # [1, 2, 3] (removes last)

# Get length
let length be len(arr)  # 3
```

### Q: How do I check if value is in array?
**A:**
```vex
let arr be [1, 2, 3]

fn is_in_list(list, value):
    for each item in list:
        if item is value:
            give back true
    give back false

if is_in_list(arr, 2):
    display "Found it"
```

### Q: How do I create a map?
**A:**
```vex
let person be {"name": "Alice", "age": 30, "city": "NYC"}
let empty be {}
```

### Q: How do I access map values?
**A:** Use key:
```vex
let person be {"name": "Alice", "age": 30}
display person.name     # Alice
display person["age"]   # 30 (bracket notation also works)
```

### Q: How do I modify map values?
**A:**
```vex
let person be {"name": "Alice"}
person.name be "Bob"
person.age be 30
display person  # {"name": "Bob", "age": 30}
```

### Q: Are maps ordered?
**A:** Yes, maps preserve insertion order.

### Q: How do I get map keys?
**A:** Use `keys()`:
```vex
let person be {"name": "Alice", "age": 30}
let keys be keys(person)  # ["name", "age"]
```

### Q: How do I loop through arrays?
**A:**
```vex
let arr be [1, 2, 3]

# For-each loop
for each item in arr:
    display item

# Index-based loop
for i in 0 to len(arr):
    display arr[i]
```

### Q: How do I loop through maps?
**A:**
```vex
let person be {"name": "Alice", "age": 30}

# Loop through keys
for key in keys(person):
    display key + ": " + str(person[key])

# Or manually
for each key in keys(person):
    display key + ": " + str(person[key])
```

### Q: What's a list comprehension?
**A:** A concise way to create lists:
```vex
let numbers be [1, 2, 3, 4, 5]
let squares be [x * x for x in numbers]        # [1, 4, 9, 16, 25]
let filtered be [x for x in numbers if x > 2]  # [3, 4, 5]
let doubled be [x * 2 for x in numbers]        # [2, 4, 6, 8, 10]
```

---

## Object-Oriented Programming

### Q: Can I inherit from a struct?
**A:** Yes, use `extends`:
```vex
struct Dog extends Animal:
    ...
```

### Q: Can I inherit from multiple structs?
**A:** ❌ **Not yet** - Vexium v1.0 only supports single inheritance.

**Example that does NOT work:**
```vex
# Multiple inheritance NOT supported in v1.0
struct SearchEngine extends Indexable, Rankable:
    ...
```

**Why This Matters:**
Multiple inheritance is critical for AI/ML/data science:
- Composing behaviors (mixins for data pipelines)
- Building framework-like utilities
- Reusing code patterns

**See:** [MULTIPLE_INHERITANCE_PROPOSAL.md](MULTIPLE_INHERITANCE_PROPOSAL.md) - Planned for v2.0!

**Current Solution: Composition**
```vex
struct SearchEngine:
    has indexer
    has ranker
    
    can search(query):
        let indexed be self.indexer.index(query)
        let ranked be self.ranker.rank(indexed)
        give back ranked
```

---

## Object-Oriented Programming

### Q: Can I inherit from a struct?
**A:** Yes, use `extends`:
```vex
struct Animal:
    has name

    can speak():
        display self.name + " makes a sound"

struct Dog extends Animal:
    has breed

    can speak():
        display self.name + " barks"

let dog be Dog()
dog.name be "Rex"
dog.breed be "Labrador"
dog.speak()  # Rex barks
```

### Q: Can I use multiple inheritance?
**A:** No, Vexium supports single inheritance only.

### Q: How do I call parent methods?
**A:** Use `super`:
```vex
struct Animal:
    can speak():
        display "Generic sound"

struct Dog extends Animal:
    can speak():
        super.speak()
        display "Woof!"
```

### Q: How do I check if an object is instance of a type?
**A:** Use `is_instance`:
```vex
struct Dog:
    ...

let dog be Dog()
if is_instance(dog, Dog):
    display "It's a dog!"
```

### Q: Can I have private properties?
**A:** Vexium doesn't have access modifiers. Use naming conventions:
```vex
struct BankAccount:
    has _balance  # Underscore indicates private by convention

    can get_balance():
        give back self._balance
```

---

## Advanced Topics

### Q: What are lambda functions useful for?
**A:** Passing functions as arguments:
```vex
fn apply_operation(a, b, operation):
    give back operation(a, b)

display apply_operation(5, 3, (x, y) => x + y)      # 8
display apply_operation(5, 3, (x, y) => x * y)      # 15
display apply_operation(5, 3, (x, y) => x - y)      # 2
```

### Q: Can I store functions in variables?
**A:** Yes:
```vex
let add be (a, b) => a + b
let subtract be (a, b) => a - b

let operations be [add, subtract]
display operations[0](5, 3)  # 8
display operations[1](5, 3)  # 2
```

### Q: How do I handle errors?
**A:** Use try-except (attempt-otherwise):
```vex
attempt:
    let result be 10 / 0
otherwise err:
    display "Error: " + err
```

### Q: Can I nest try-except?
**A:** Yes:
```vex
attempt:
    attempt:
        ...
    otherwise inner_err:
        display "Inner: " + inner_err
otherwise outer_err:
    display "Outer: " + outer_err
```

### Q: How do I create custom error messages?
**A:** 
```vex
fn validate_age(age):
    if age is less than 0:
        give back {"success": false, "error": "Age cannot be negative"}
    if age is greater than 150:
        give back {"success": false, "error": "Age seems unrealistic"}
    give back {"success": true}
```

---

## Troubleshooting

### Q: I get "undefined variable" error
**A:** Make sure variable is declared before use:
```vex
# ❌ Wrong
display x
let x be 5

# ✅ Right
let x be 5
display x
```

### Q: Array index out of bounds
**A:** Check array length before accessing:
```vex
let arr be [1, 2, 3]

if index is at least 0 and index is less than len(arr):
    display arr[index]
else:
    display "Index out of bounds"
```

### Q: Function not found error
**A:** Make sure function is defined:
```vex
# Must define BEFORE calling
fn add(a, b):
    give back a + b

display add(5, 3)  # OK
```

### Q: Can't concatenate string and number
**A:** Convert number to string:
```vex
# ❌ Wrong
display "Count: " + 5  # Error

# ✅ Right
display "Count: " + str(5)
```

### Q: Struct method not found
**A:** Use correct syntax:
```vex
struct Math:
    can add(a, b):
        give back a + b

let m be Math()
display m.add(5, 3)  # OK

# ❌ Wrong
display Math.add(5, 3)  # Wrong syntax
```

### Q: Program runs but produces no output
**A:** Use `display` for output:
```vex
# ❌ No output
5 + 3

# ✅ Shows output
display 5 + 3
```

### Q: Infinite loop
**A:** Check loop condition:
```vex
# ❌ Infinite
let i be 0
while i is less than 10:
    display i
    # Missing i be i + 1

# ✅ Correct
let i be 0
while i is less than 10:
    display i
    i be i + 1
```

### Q: Variable changes unexpectedly
**A:** Check for variable shadowing:
```vex
let x be 5
if true:
    let x be 10  # Creates new local x
    display x    # 10
display x        # 5 (original x unchanged)
```

---

## Performance

### Q: Is Vexium fast?
**A:** Vexium is moderately fast. It's a bytecode-compiled language suitable for most tasks. For performance-critical code, consider:
- Avoiding nested loops
- Caching expensive calculations
- Using appropriate data structures

### Q: What causes slow performance?
**A:**
- Nested loops (O(n²) or worse)
- Recalculating in loops
- Inefficient string concatenation
- Large data without optimization

### Q: How do I optimize loops?
**A:**
```vex
# ❌ Slow
for i in 0 to len(list):
    let item be list[i]
    let count be len(list)  # Recalculates each time!

# ✅ Fast
let list_len be len(list)
for i in 0 to list_len:
    let item be list[i]
```

### Q: How do I build strings efficiently?
**A:**
```vex
# ❌ Slow
let result be ""
for i in 0 to 1000:
    result be result + i

# ✅ Better - build array then join
let parts be []
for i in 0 to 1000:
    push(parts, str(i))
let result be join(parts, "")
```

---

## Modules & Organization

### Q: How do I create a module?
**A:** Just create a `.vxm` file:
```vex
# lib/math_utils.vxm
const PI be 3.14159

fn circle_area(radius):
    give back PI * radius * radius
```

### Q: How do I use a module?
**A:** Use `use`:
```vex
use math_utils

display circle_area(5)
```

### Q: Can I organize modules in folders?
**A:** Yes:
```
lib/
├── math_utils.vxm
├── string_utils.vxm
└── utils/
    ├── validators.vxm
    └── helpers.vxm

# Import with path
use lib.utils.validators
```

### Q: How do I avoid naming conflicts?
**A:** Use namespaces or prefixes:
```vex
# In module
const MATH_PI be 3.14159
fn math_sum(arr):
    ...

# Or alias on import
use math_utils as math
display math.PI
```

### Q: Can I import specific functions?
**A:** Currently you import entire modules. Use naming to avoid conflicts.

---

## Miscellaneous

### Q: What's the difference between `is` and ==?
**A:** Vexium uses `is` for comparison (no `==` operator):
```vex
if x is 5:
    ...
```

### Q: How do I increment a variable?
**A:**
```vex
let x be 5
x be x + 1   # Now x is 6
x be x - 1   # Now x is 5
```

### Q: Can I use comments?
**A:** Yes, use `#`:
```vex
# This is a comment
let x be 5  # Inline comment
```

### Q: Is Vexium case-sensitive?
**A:** Yes:
```vex
let Name be "Alice"
let name be "Bob"
# Name and name are different variables
```

### Q: How do I check if array is empty?
**A:**
```vex
let arr be []
if len(arr) is 0:
    display "Empty"
```

### Q: How do I check if string is empty?
**A:**
```vex
let text be ""
if len(text) is 0:
    display "Empty"

# Or use trim
if len(trim(text)) is 0:
    display "Empty or whitespace"
```

### Q: How do I reverse a string?
**A:**
```vex
fn reverse_string(s):
    let result be ""
    for i in len(s) - 1 to -1:
        result be result + s[i]
    give back result

display reverse_string("hello")  # "olleh"
```

### Q: Can I sort arrays?
**A:** Use built-in `sort()`:
```vex
let numbers be [3, 1, 4, 1, 5, 9, 2, 6]
let sorted be sort(numbers)  # [1, 1, 2, 3, 4, 5, 6, 9]
```

### Q: How do I get unique elements?
**A:**
```vex
fn get_unique(arr):
    let seen be {}
    let result be []
    for each item in arr:
        if not has_key(seen, str(item)):
            push(result, item)
            seen[str(item)] be true
    give back result
```

---

## Tips & Tricks

1. **Use meaningful variable names** - `user_age` not `ua`
2. **Test edge cases** - empty arrays, negative numbers, etc.
3. **Use constants for magic numbers** - `const MAX_RETRIES be 3`
4. **Comment complex logic** - explain WHY, not WHAT
5. **Keep functions small** - easier to test and understand
6. **Handle errors explicitly** - use try-except wisely
7. **Use type conversion** - `str()`, `int()`, `float()`
8. **Organize code in modules** - better maintainability
9. **Follow naming conventions** - easier to read
10. **Test your code** - prevents bugs

---

## Still Have Questions?

- Check [VEXIUM_SYNTAX.md](VEXIUM_SYNTAX.md) for syntax reference
- See [VEXIUM_QUICK_START.md](VEXIUM_QUICK_START.md) for basics
- Look at examples in `examples/` directory
- Read [VEXIUM_BEST_PRACTICES.md](VEXIUM_BEST_PRACTICES.md) for best practices
- Check [VEXIUM_LEARNING_PATH.md](VEXIUM_LEARNING_PATH.md) for structured learning

Happy coding! 🎉
