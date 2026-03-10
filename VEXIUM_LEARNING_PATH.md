# 🎯 Vexium Learning Path

A structured guide to learning Vexium from beginner to advanced.

---

## Quick Navigation

- **Complete Beginner?** → Start with [5-Minute Quick Start](#level-1-your-first-vexium-program)
- **Know Programming?** → Start with [Syntax Fundamentals](#level-2-syntax-fundamentals)
- **Intermediate?** → Start with [Advanced Features](#level-3-advanced-features)
- **Building Large Projects?** → Go to [Professional Development](#level-4-professional-development)

---

## Level 1: Your First Vexium Program

**Time: 30 minutes**

### What You'll Learn
- How to install and run Vexium
- Basic output with `display`
- Variables with `let` and `const`
- Simple arithmetic

### Resources
- [VEXIUM_QUICK_START.md](VEXIUM_QUICK_START.md) - Read this first!
- `examples/hello.vxm` - Simple example

### Practice Exercises

1. **Hello Vexium**
```vex
display "Hello, Vexium!"
```

2. **Simple Math**
```vex
let x be 5
let y be 3
display x + y
display x * y
display x ** y
```

3. **String Concatenation**
```vex
let name be "World"
display "Hello, " + name + "!"
```

### Check Your Understanding
- [ ] Can run a .vxm file
- [ ] Understand variable declaration
- [ ] Can perform basic math
- [ ] Know the difference between `let` and `const`

---

## Level 2: Syntax Fundamentals

**Time: 2-3 hours**

### What You'll Learn
- All data types (int, float, string, bool, arrays, maps)
- Control flow (if/elif/else, loops)
- Functions
- Collections (arrays and maps)

### Resources
- [VEXIUM_SYNTAX.md](VEXIUM_SYNTAX.md) - Comprehensive syntax reference
- [VEXIUM_FUNCTIONS.md](VEXIUM_FUNCTIONS.md) - Function guide
- `examples/test_interpreter.vxm` - Syntax examples

### Core Topics

#### 1. Data Types (30 min)
```vex
# Integers
let age be 25

# Floats
let height be 5.9

# Strings
let name be "Alice"

# Booleans
let is_active be true

# Arrays
let numbers be [1, 2, 3, 4, 5]

# Maps
let person be {"name": "Alice", "age": 25}
```

#### 2. Control Flow (45 min)
```vex
# If statement
if age is greater than 18:
    display "Adult"

# Loops
for i in 1 to 10:
    display i

let list be [1, 2, 3]
for each item in list:
    display item

while count is less than 5:
    count be count + 1
```

#### 3. Functions (45 min)
```vex
fn add(a, b):
    give back a + b

fn greet(name):
    display "Hello, " + name

fn power(base, exp be 2):
    let result be 1
    for i in 1 to exp + 1:
        result be result * base
    give back result
```

### Practice Exercises

1. **Calculator Function**
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

display calculate(10, "+", 5)
```

2. **Grade Assigner**
```vex
fn get_grade(score):
    if score is at least 90:
        give back "A"
    elif score is at least 80:
        give back "B"
    elif score is at least 70:
        give back "C"
    elif score is at least 60:
        give back "D"
    give back "F"

display get_grade(85)  # "B"
```

3. **Sum a List**
```vex
fn sum_list(numbers):
    let total be 0
    for each num in numbers:
        total be total + num
    give back total

let scores be [85, 90, 78, 92]
display sum_list(scores)
```

### Check Your Understanding
- [ ] Understand all data types
- [ ] Can write if/else statements
- [ ] Can write for and while loops
- [ ] Can write and call functions
- [ ] Can work with arrays and maps

---

## Level 3: Advanced Features

**Time: 3-4 hours**

### What You'll Learn
- Lambda functions
- Higher-order functions
- Object-oriented programming (structs)
- Inheritance
- List comprehensions
- Error handling

### Resources
- [VEXIUM_FUNCTIONS.md](VEXIUM_FUNCTIONS.md#lambda-functions) - Lambda guide
- [VEXIUM_OOP.md](VEXIUM_OOP.md) - OOP guide
- [VEXIUM_SYNTAX.md](VEXIUM_SYNTAX.md#advanced-features) - Advanced syntax
- `examples/test_phase6.vxm` - Advanced examples

### Core Topics

#### 1. Lambda Functions (45 min)
```vex
# Basic lambda
let square be (x) => x * x
display square(5)

# Lambda in higher-order function
fn apply_twice(func, value):
    give back func(func(value))

display apply_twice((x) => x + 1, 5)  # 7
```

#### 2. Structs & OOP (90 min)
```vex
struct Rectangle:
    has width
    has height

    can init(w, h):
        self.width be w
        self.height be h

    can area():
        give back self.width * self.height

let rect be Rectangle(5, 10)
display rect.area()
```

#### 3. Inheritance (45 min)
```vex
struct Animal:
    has name

struct Dog extends Animal:
    has breed

let dog be Dog()
dog.name be "Rex"
dog.breed be "Labrador"
```

#### 4. List Comprehensions & Error Handling (30 min)
```vex
let nums be [1, 2, 3, 4, 5]
let evens be [x for x in nums if x % 2 is 0]

attempt:
    let result be 10 / 0
otherwise err:
    display "Error: " + err
```

### Practice Exercises

1. **Bank Account System**
```vex
struct BankAccount:
    has balance

    can init(initial):
        self.balance be initial

    can deposit(amount):
        if amount is greater than 0:
            self.balance be self.balance + amount

    can withdraw(amount):
        if amount is greater than 0 and amount is at most self.balance:
            self.balance be self.balance - amount
            give back true
        give back false

let acc be BankAccount(1000)
acc.deposit(500)
acc.withdraw(200)
display acc.balance
```

2. **Student Grade System**
```vex
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
        for each s in self.scores:
            sum be sum + s
        give back sum / len(self.scores)

    can grade():
        let avg be self.average()
        if avg is at least 90:
            give back "A"
        elif avg is at least 80:
            give back "B"
        give back "C"
```

3. **Filter and Map**
```vex
let numbers be [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

# Filter evens
let evens be [x for x in numbers if x % 2 is 0]

# Map squares
let squares be [x * x for x in numbers]

# Complex operation
let result be [x * 2 for x in numbers if x is greater than 5]
```

### Check Your Understanding
- [ ] Can write lambda functions
- [ ] Can use higher-order functions
- [ ] Can define and use structs
- [ ] Can use inheritance properly
- [ ] Can write list comprehensions
- [ ] Understand error handling

---

## Level 4: Professional Development

**Time: 4-6 hours**

### What You'll Learn
- Code organization and design patterns
- Using modules
- Building larger programs
- Performance considerations
- Best practices

### Resources
- [VEXIUM_MODULES.md](VEXIUM_MODULES.md) - Module guide
- [VEXIUM_STDLIB.md](VEXIUM_STDLIB.md) - Standard library
- [VEXIUM_BEST_PRACTICES.md](VEXIUM_BEST_PRACTICES.md) - Best practices

### Core Topics

#### 1. Modules (90 min)
```vex
# Create lib/math_lib.vxm
const PI be 3.14159

fn circle_area(radius):
    give back PI * radius * radius

# Use in main.vxm
use math_lib
display circle_area(5)
```

#### 2. Standard Library (90 min)
```vex
use math
use string
use fs
use json

# Math functions
display sqrt(16)        # 4.0

# String manipulation
display upper("hello")  # "HELLO"

# File operations
let content be read_file("data.txt")

# JSON
let data be parse_json(content)
```

#### 3. Design Patterns (90 min)
```vex
# Builder Pattern
let query be QueryBuilder()
    .where("age > 18")
    .order_by("name")
    .build()

# Factory Pattern
let dog be AnimalFactory("dog")

# Strategy Pattern
let processor be PaymentProcessor(credit_card_strategy)
```

#### 4. Large Project Structure (45 min)
```
my_project/
├── main.vxm
├── lib/
│   ├── models.vxm
│   ├── validators.vxm
│   ├── services/
│   │   ├── user_service.vxm
│   │   └── payment_service.vxm
│   └── utils/
│       ├── string_utils.vxm
│       └── math_utils.vxm
└── data/
    └── config.vxm
```

### Practice Projects

#### Project 1: Todo List Manager
```vex
struct Todo:
    has id
    has title
    has completed

struct TodoList:
    has items

    can init():
        self.items be []

    can add(title):
        let todo be Todo()
        todo.id be len(self.items) + 1
        todo.title be title
        todo.completed be false
        push(self.items, todo)

    can complete(id):
        for each item in self.items:
            if item.id is id:
                item.completed be true

    can get_pending():
        give back [t for t in self.items if t.completed is false]

let todos be TodoList()
todos.add("Buy milk")
todos.add("Write code")
```

#### Project 2: Configuration Manager
```vex
# lib/config.vxm
const DATABASE_URL be "localhost:5432"
const API_KEY be "secret123"

fn get_database_url():
    give back DATABASE_URL

# main.vxm
use config
use fs

attempt:
    let db_url be config.get_database_url()
    display "Connecting to: " + db_url
otherwise err:
    display "Error: " + err
```

#### Project 3: Data Processing Pipeline
```vex
fn load_data(filename):
    use fs, json
    let content be read_file(filename)
    give back parse_json(content)

fn filter_active(items):
    give back [x for x in items if x.active is true]

fn calculate_statistics(items):
    let count be len(items)
    let sum be 0
    for each item in items:
        sum be sum + item.value
    give back {"count": count, "average": sum / count}

# Pipeline
let data be load_data("users.json")
let active be filter_active(data)
let stats be calculate_statistics(active)
```

### Check Your Understanding
- [ ] Can organize code into modules
- [ ] Can use standard library effectively
- [ ] Know common design patterns
- [ ] Can structure large projects
- [ ] Write maintainable code
- [ ] Know performance best practices

---

## Project Ideas by Level

### Level 1-2 Projects
- [ ] Calculator
- [ ] Grade converter
- [ ] Temperature converter
- [ ] Simple game (guessing game)
- [ ] Word counter

### Level 2-3 Projects
- [ ] Todo list
- [ ] Contact manager
- [ ] Student grade tracker
- [ ] Expense tracker
- [ ] Simple note-taking app

### Level 3-4 Projects
- [ ] Blog system
- [ ] User authentication system
- [ ] API client
- [ ] Data analysis tool
- [ ] Configuration manager

---

## Learning Tips

### 1. Practice Incrementally
- Master one concept before moving to next
- Complete practice exercises
- Test your understanding

### 2. Write Real Code
- Create projects, not just exercises
- Build things you're interested in
- Refactor and improve

### 3. Read Documentation
- Reference guides when needed
- Look at examples
- Study standard library

### 4. Debug Effectively
```vex
# Use display for debugging
display "Debug: " + str(variable)

# Test edge cases
# What if array is empty?
# What if number is negative?
# What if string is empty?
```

### 5. Join Community
- Share projects
- Ask for feedback
- Help others learn

---

## Time Estimates

| Level | Topics | Time | Project Complexity |
|-------|--------|------|-------------------|
| 1 | Basics | 30 min | Trivial |
| 2 | Syntax | 2-3 hrs | Simple (1-2 hrs) |
| 3 | Advanced | 3-4 hrs | Medium (2-4 hrs) |
| 4 | Professional | 4-6 hrs | Complex (4+ hrs) |

**Total: ~10-15 hours to intermediate proficiency**

---

## Progression Checklist

### Level 1 ✅
- [ ] Installation complete
- [ ] Can run first program
- [ ] Understand variables
- [ ] Can do math operations

### Level 2 ✅
- [ ] Know all data types
- [ ] Can write control flow
- [ ] Can write functions
- [ ] Can use arrays and maps

### Level 3 ✅
- [ ] Can write lambdas
- [ ] Understand OOP
- [ ] Can use inheritance
- [ ] Know error handling

### Level 4 ✅
- [ ] Can create modules
- [ ] Use standard library
- [ ] Apply design patterns
- [ ] Structure large projects

---

## Common Mistakes to Avoid

### Level 1-2
- ❌ Mixing up `let` and `const`
- ❌ String concatenation vs arithmetic
- ❌ Off-by-one errors in loops
- ✅ Use `display` to debug

### Level 2-3
- ❌ Forgetting to initialize variables
- ❌ Modifying loop variable inside loop
- ❌ Returning early from nested functions
- ✅ Test edge cases

### Level 3-4
- ❌ Deep inheritance hierarchies
- ❌ Circular module dependencies
- ❌ Not handling errors
- ✅ Keep it simple

---

## Next Steps

Once you complete Level 4:

1. **Build Projects** - Create applications you care about
2. **Read Code** - Study well-written Vexium code
3. **Contribute** - Help improve Vexium
4. **Educate Others** - Teach Vexium to friends
5. **Stay Updated** - Follow release notes and new features

---

## Additional Resources

- **Official Documentation** - [github.com/vexium/docs](https://github.com)
- **Examples** - `examples/` directory in repository
- **Community** - [vexium.dev/community](https://vexium.dev)
- **FAQ** - [VEXIUM_FAQ.md](VEXIUM_FAQ.md)

---

Happy learning! 🚀

Feel free to skip around if you already know some concepts, but we recommend following the learning path for best retention.
