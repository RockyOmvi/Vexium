# 🚀 Vex v2 Quick Start Guide

> **Get up and running with Vex in 15 minutes**

---

## Installation

```bash
# Download Vex (binary download, homebrew, apt)
$ wget https://releases.vexlang.io/vex-2.0-linux-x64
$ chmod +x vex
$ mv vex /usr/local/bin/

# Verify installation
$ vex --version
Vex v2.0.0
```

---

## Your First Vex Program

Create `hello.vxm`:

```vex
display "Hello, Vex!"
```

Run it:

```bash
$ vex run hello.vxm
Hello, Vex!
```

---

## Core Syntax in 5 Minutes

### Variables & Types

```vex
# Mutable variables (let)
let count be 5
count be count + 1

# Immutable constants (const)
const PI be 3.14159

# Type inference — Vex figures out types automatically
let name be "Alice"      # Inferred: str
let age be 30            # Inferred: int
let height be 5.8        # Inferred: float
let active be true       # Inferred: bool

# Explicit types when needed
let x: float be 42       # Force 42 to be float, not int
```

### Functions

```vex
# Define a function
fn greet(name: str, greeting: str) -> str:
    let message be "{greeting}, {name}!"
    give back message

# Call it
let result be greet("Alice", "Hello")
display result          # Output: Hello, Alice!

# No return value
fn welcome(name: str):
    display "Welcome, {name}!"

welcome("Bob")
```

### Loops & Control Flow

```vex
# for loop (range)
for i in 0 to 5:        # 0, 1, 2, 3, 4 (not including 5)
    display i

# for loop (array)
let fruits be ["apple", "banana", "cherry"]
for fruit in fruits:
    display fruit

# while loop
let x be 0
while x is less than 10:
    display x
    x be x + 1

# if/elif/else
let age be 20
if age is less than 13:
    display "Child"
elif age is less than 18:
    display "Teen"
else:
    display "Adult"

# repeat (count loop)
repeat 3 times:
    display "Hello"
```

### Collections

```vex
# Arrays
let numbers be [1, 2, 3, 4, 5]
display numbers[0]           # 1 (index access)
numbers.push(6)              # Add to end
let first be numbers[0]
let last be numbers[-1]      # -1 = last element

# Maps (dictionaries)
let person be {
    "name": "Alice",
    "age": 30,
    "city": "NYC"
}

display person["name"]       # "Alice"
person["email"] be "alice@example.com"  # Add/update

# Strings
let text be "Hello, World"
display text.upper()         # "HELLO, WORLD"
display text.length()        # 12
display text.contains("World")  # true

# Array methods
let nums be [1, 2, 3, 4, 5]
let doubled be nums.map(fn(x): give back x * 2)
let even be nums.filter(fn(x): give back x % 2 is 0)
let sum be nums.reduce(fn(acc, x): give back acc + x)
```

### Error Handling

```vex
attempt:
    let file_content be read("important.txt")
    display file_content
otherwise error:
    display "Error: {error.message()}"
```

---

## Working with Files

```vex
use fs

# Read file
let content be fs.read("input.txt")
display content

# Read lines
let lines be fs.read_lines("data.txt")
for line in lines:
    display line

# Write file
fs.write("output.txt", "Hello, World!\n")
fs.append("log.txt", "New log entry\n")

# List directory
let files be fs.list_dir(".")
display files

# Check file existence
if fs.exists("config.json"):
    let config be fs.read_json("config.json")
    display config["setting"]
```

---

## Working with JSON

```vex
use json

# Parse JSON
let json_str be '{"name":"Alice","age":30,"city":"NYC"}'
let data be json.parse(json_str)

display data["name"]         # "Alice"
display data["age"]          # 30

# Create JSON
let response be {
    "status": "success",
    "data": ["item1", "item2"],
    "count": 2
}

let json_output be json.stringify(response)
display json_output

# Pretty print
display json.pretty(response, indent: 2)
```

---

## HTTP Requests

```vex
use http

# Make a GET request
let response be http.get("https://api.example.com/users")
display response.status         # 200
display response.json()         # Parse as JSON

# POST request
let response be http.post(
    "https://api.example.com/users",
    body: {"name": "Charlie", "email": "charlie@example.com"}
)

if response.status is 201:
    display "User created successfully"
else:
    display "Error: {response.status}"
```

---

## Building a Simple Web Server

```vex
use http

# Create server
let server be http.create_server(port: 8080)

# GET route
server.on_get("/", fn(request):
    give back http.response(200, "Welcome to Vex!")
)

# API route
server.on_get("/api/greeting", fn(request):
    let name be request.query["name"] or "Guest"
    give back http.response(200, json {"message": "Hello, {name}!"})
)

# POST route
server.on_post("/api/users", fn(request):
    let data be request.json()
    let user_id be 42          # Pretend we saved to DB
    
    let response be {
        "id": user_id,
        "name": data["name"],
        "created": true
    }
    
    give back http.response(201, response)
)

# Start server
display "Server running on http://localhost:8080"
server.listen()
```

---

## Working with Data

```vex
use collections

# Process CSV data
use fs
use json

let csv_lines be fs.read_lines("users.csv")
let users be []

for line in csv_lines[1:]:     # Skip header
    let parts be line.split(",")
    let user be {
        "id": parts[0],
        "name": parts[1],
        "email": parts[2]
    }
    users.push(user)

# Filter and transform
let adults be users.filter(fn(u): 
    give back int(u["age"]) >= 18
)

let names be adults.map(fn(u): 
    give back u["name"]
)

display names
```

---

## Concurrency (Parallel Tasks)

```vex
use concurrent

# Define a task
task fetch_url(url: str) -> str:
    let response be http.get(url)
    give back response.body

# Run multiple tasks in parallel
let urls be [
    "https://api1.example.com/data",
    "https://api2.example.com/data",
    "https://api3.example.com/data"
]

let results be await all [fetch_url(url) for url in urls]

for i in 0 to results.length():
    display "Result {i}: {results[i]}"
```

---

## Structures (Custom Types)

```vex
# Define a structure
struct User:
    has id: int
    has name: str
    has email: str
    
    can display_info(self):
        display "ID: {self.id}"
        display "Name: {self.name}"
        display "Email: {self.email}"
    
    can change_email(self, new_email: str):
        self.email be new_email

# Create an instance
let user be User(
    id: 1,
    name: "Alice",
    email: "alice@example.com"
)

# Call methods
user.display_info()
user.change_email("alice.smith@example.com")
user.display_info()
```

---

## Common Patterns

### Process Each Item

```vex
let items be ["a", "b", "c"]

# Imperative (C-style)
for item in items:
    display item

# Functional (modern style)
items.map(fn(item):
    display item
)
```

### Sum/Aggregate

```vex
let numbers be [1, 2, 3, 4, 5]

# Imperative
let total be 0
for num in numbers:
    total be total + num

# Functional (reduces code)
let total be numbers.reduce(fn(acc, x): 
    give back acc + x
)
```

### Find Item in List

```vex
let users be [...]

# Filter for one item
let alice be nothing
for user in users:
    if user["name"] is "Alice":
        alice be user
        break

# Simpler: get first match
let alice be users.filter(fn(u): 
    give back u["name"] is "Alice"
)[0]
```

---

## Debugging

### Print Values

```vex
display "x = {x}"
display "type = {type(x)}"
display "count = {len(array)}"
```

### Check Types

```vex
if is_string(value):
    display "It's a string"
elif is_int(value):
    display "It's an integer"
else:
    display "Unknown type"
```

---

## Next Steps

1. **Read the [Full Vex v2 Language Spec](./language_v2_spec.md)** — comprehensive reference
2. **Explore examples** — check `vex/examples/` in the repo
3. **Build something** — practice with files, HTTP, and concurrency
4. **Join the community** — discuss on GitHub discussions

---

## Compilation & Deployment

```bash
# Run with interpreter (slow, debug)
$ vex run app.vxm

# Compile to bytecode (faster)
$ vex build app.vxm
$ vex app.vxmc           # Run compiled version

# Build native binary
$ vex build app.vxm --target native
$ ./app                   # Run the binary (~3MB, instant startup)

# Cross-compile for another platform
$ vex build app.vxm --target windows-x64
$ vex build app.vxm --target arm64
```

---

## Common Commands

| Command | Purpose |
|---------|---------|
| `vex run file.vex` | Run script directly |
| `vex build file.vex` | Compile to binary |
| `vex check file.vex` | Type check |
| `vex fmt file.vex` | Format code |
| `vex test path/` | Run tests |
| `vex repl` | Interactive shell |
| `vex add package` | Install package |
| `vex --version` | Show version |

---

## Tips & Tricks

### Quick Testing

```bash
$ cat > test.vxm << 'EOF'
display 10 + 20
display "Hello" + " World"
let x be [1,2,3]
display x.length()
EOF

$ vex run test.vxm
30
Hello World
3
```

### Use REPL for Experimenting

```bash
$ vex repl
>> let x be 42
>> display x * 2
84
>> exit
```

### Track Variables While Running

```bash
$ vex run --debug app.vxm
```

---

> **Questions?** See [language_v2_spec.md](./language_v2_spec.md) for the complete reference.
