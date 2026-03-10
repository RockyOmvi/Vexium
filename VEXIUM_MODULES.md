# 📦 Vexium Modules & Imports Guide

Learn how to organize code and create reusable modules.

---

## Table of Contents

1. [Built-in Modules](#built-in-modules)
2. [User-Defined Modules](#user-defined-modules)
3. [Module Organization](#module-organization)
4. [Import Patterns](#import-patterns)
5. [Best Practices](#best-practices)
6. [Examples](#examples)

---

## Built-in Modules

### Available Modules

Vexium comes with these standard modules:

```vex
use math              # Math functions and constants
use string            # String manipulation
use collections       # Array and map operations
use fs                # File system access
use sys               # System operations
use json              # JSON parsing
```

### Importing

```vex
# Import entire module (all symbols available)
use math
display PI            # 3.14159
display sqrt(16)      # 4.0

# Selective import
from math use sqrt
display sqrt(25)      # 5.0
```

### Import Styles

```vex
# Style 1: Full module use
use math
let value be sqrt(16)

# Style 2: Selective import
from math use sqrt
let value be sqrt(16)

# Style 3: Multiple selective imports
from math use sqrt, pow
from string use upper
```

---

## User-Defined Modules

### Creating a Module

Create a file named `mymodule.vxm`:

```vex
# mymodule.vxm
# This is a reusable module

let VERSION be "1.0.0"

fn greet(name):
    give back "Hello, " + name

fn farewell(name):
    give back "Goodbye, " + name

struct Greeter:
    has greeting

    can init(greeting):
        self.greeting be greeting

    can say(name):
        give back self.greeting + ", " + name
```

### Using Your Module

In another file or program:

```vex
# main.vxm

# Import entire module
use mymodule
display mymodule.greet("Alice")

# Or selective import
from mymodule use greet, farewell
display greet("Bob")
display farewell("Charlie")

# Import a class
from mymodule use Greeter
let g be Greeter("Howdy")
display g.say("Partner")
```

### Module File Locations

Modules are searched in these locations (in order):

1. Current directory
2. `lib/` subdirectory
3. Standard library directory

```vex
# These all work:
use mymodule          # Looks for mymodule.vxm in current dir
                      # Then looks in lib/mymodule.vxm

use utilities.helpers # Looks for lib/utilities/helpers.vxm
```

---

## Module Organization

### Flat Structure

```
project/
├── main.vxm
├── utils.vxm
├── helpers.vxm
└── models.vxm
```

Usage:
```vex
use utils
use helpers
use models
```

### Nested Structure

```
project/
├── main.vxm
└── lib/
    ├── math_lib.vxm
    ├── string_lib.vxm
    └── utilities/
        ├── helpers.vxm
        └── validators.vxm
```

Usage:
```vex
use math_lib
use utilities.helpers
use utilities.validators
```

---

## Import Patterns

### Full Import

```vex
use math_lib

# All public symbols available
display PI
display square(5)
```

### Selective Import

```vex
from math_lib use PI, square

# Only these symbols available
display PI
display square(5)

# This won't work anymore:
# display cube(5)    # Not imported
```

### Aliasing (if supported in future)

```vex
# Future: use math_lib as ml
# Then: display ml.square(5)
```

---

## Common Module Patterns

### Utility Module

```vex
# lib/math_utils.vxm

fn is_even(n):
    give back n % 2 is 0

fn is_odd(n):
    give back n % 2 is not 0

fn sum_list(numbers):
    let total be 0
    for each num in numbers:
        total be total + num
    give back total

fn average(numbers):
    give back sum_list(numbers) / len(numbers)
```

Usage:
```vex
from math_utils use is_even, sum_list

if is_even(4):
    display "Number is even"

let numbers be [1, 2, 3, 4, 5]
display sum_list(numbers)      # 15
```

### Data Models Module

```vex
# lib/models.vxm

struct User:
    has id
    has name
    has email

    can init(id, name, email):
        self.id be id
        self.name be name
        self.email be email

    can display_info():
        display "User #" + str(self.id) + ": " + self.name

struct Post:
    has id
    has author_id
    has title
    has content

    can init(id, author_id, title, content):
        self.id be id
        self.author_id be author_id
        self.title be title
        self.content be content
```

Usage:
```vex
from models use User, Post

let alice be User(1, "Alice", "alice@example.com")
alice.display_info()

let post be Post(1, 1, "Hello", "First post!")
```

### Configuration Module

```vex
# lib/config.vxm

const DB_HOST be "localhost"
const DB_PORT be 5432
const DB_NAME be "myapp"

const API_URL be "https://api.example.com"
const API_TIMEOUT be 30

fn get_db_url():
    give back "postgresql://" + DB_HOST + ":" + str(DB_PORT) + "/" + DB_NAME

fn get_api_endpoint(path):
    give back API_URL + path
```

Usage:
```vex
from config use DB_HOST, API_URL, get_db_url

display DB_HOST
display get_db_url()
```

### Validation Module

```vex
# lib/validators.vxm

fn is_valid_email(email):
    give back email contains "@" and email contains "."

fn is_valid_username(username):
    if len(username) is less than 3:
        give back false
    if len(username) is greater than 20:
        give back false
    give back true

fn is_valid_password(password):
    give back len(password) is greater than 8
```

Usage:
```vex
from validators use is_valid_email, is_valid_password

let email be "user@example.com"
let password be "SecurePass123"

if is_valid_email(email) and is_valid_password(password):
    display "Valid credentials"
```

---

## Best Practices

### 1. Clear Module Names

```vex
# ✅ Good
use user_service         # Service for user operations
use payment_processor    # Handles payments
use validation           # Validation utilities

# ❌ Avoid
use utils
use helpers
use stuff
```

### 2. Organize by Feature

```
project/
└── lib/
    ├── auth/           # Authentication
    │   ├── login.vxm
    │   └── signup.vxm
    ├── users/          # User management
    │   ├── models.vxm
    │   └── validators.vxm
    └── payments/       # Payment processing
        ├── processors.vxm
        └── validators.vxm
```

### 3. Clear Exports

```vex
# math_lib.vxm
# Public API
const PI be 3.14159

fn square(x):
    give back x * x

fn cube(x):
    give back x * x * x

# ✅ Users know what to import
# from math_lib use PI, square, cube
```

### 4. Document Modules

```vex
# lib/user_service.vxm
#
# User Service Module
# Provides user-related operations
#
# Public API:
#   - User (struct)
#   - create_user(name, email) -> User
#   - delete_user(id) -> bool
#   - find_user(id) -> User

struct User:
    has id
    has name
    has email

fn create_user(name, email):
    give back User()

fn delete_user(id):
    give back true
```

### 5. Avoid Circular Imports

```vex
# ❌ Avoid
# users.vxm imports posts.vxm
# posts.vxm imports users.vxm

# ✅ Better: Create intermediate module
# models.vxm has both User and Post
# users.vxm uses models
# posts.vxm uses models
```

---

## Examples

### Example 1: Math Library

```vex
# lib/math_lib.vxm
const PI be 3.14159265358979
const E be 2.71828182845904

fn square(x):
    give back x * x

fn cube(x):
    give back x * x * x

fn double_value(x):
    give back x * 2
```

Usage:
```vex
from math_lib use square, cube, PI

display square(5)             # 25
display cube(3)               # 27
display PI                    # 3.14159...
```

### Example 2: Web Framework

```vex
# lib/web.vxm
struct Request:
    has method
    has path
    has body

struct Response:
    has status
    has body

    can init(status, body):
        self.status be status
        self.body be body

fn handle_request(request, handler):
    give back handler(request)
```

Usage:
```vex
from web use Request, Response, handle_request

let req be Request()
req.method be "POST"
req.path be "/api/users"

let res be Response(200, "Created")
display res.status
```

### Example 3: Data Validation

```vex
# lib/validators.vxm
fn validate_name(name):
    if name is nothing or len(name) is 0:
        give back false
    give back true

fn validate_age(age):
    give back age is at least 0 and age is at most 150

fn validate_person(person):
    if not validate_name(person.name):
        give back false
    if not validate_age(person.age):
        give back false
    give back true
```

Usage:
```vex
from validators use validate_person

let alice be {"name": "Alice", "age": 30}
if validate_person(alice):
    display "Valid person"
```

---

## Troubleshooting

### Module Not Found

```
Error: Module not found: mymodule.vxm
```

Solutions:
1. Check filename matches import name
2. Check file is in current directory or `lib/` subdirectory
3. Check file has `.vxm` extension

### Circular Import Error

```
Error: Circular module dependency: a -> b -> a
```

Solution: Reorganize code to eliminate the cycle

### Symbol Not Found

```
Error: 'square' not found in module 'math_lib'
```

Solution: Check the symbol is defined in the module

---

## Module System Internals

### How Modules Work

1. **Parse**: Module source file is parsed into AST
2. **Execute**: Code executed in isolated environment
3. **Extract**: Public symbols collected from execution
4. **Cache**: Module stored to prevent re-parsing
5. **Import**: Symbols added to calling environment

### Performance Notes

- Modules are cached after first load
- Importing same module twice uses cache
- Large modules only parsed once

---

## See Also

- [VEXIUM_QUICK_START.md](VEXIUM_QUICK_START.md) - Getting started
- [VEXIUM_SYNTAX.md](VEXIUM_SYNTAX.md) - Language syntax
- [VEXIUM_STDLIB.md](VEXIUM_STDLIB.md) - Standard library reference

---

You now know how to use and create modules! Next, learn about advanced patterns.
