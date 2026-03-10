# 📦 Vexium Standard Library Reference

Complete reference for Vexium's built-in modules and functions.

---

## Table of Contents

1. [Math Module](#math-module)
2. [String Module](#string-module)
3. [Collections Module](#collections-module)
4. [File System Module](#file-system-module)
5. [System Module](#system-module)
6. [JSON Module](#json-module)
7. [Common Patterns](#common-patterns)
8. [Error Handling](#error-handling)

---

## Math Module

Import with: `use math`

### Constants

```vex
use math

display PI                    # 3.14159...
display E                     # 2.71828...
display TAU                   # 6.28318... (2π)
```

### Basic Functions

```vex
use math

display abs(-5)               # 5
display abs(-3.14)            # 3.14

display sqrt(16)              # 4.0
display sqrt(2)               # 1.41421...

display pow(2, 3)             # 8
display pow(10, 2)            # 100

display min(5, 3)             # 3
display max(5, 3)             # 5
```

### Trigonometry

```vex
use math

display sin(0)                # 0.0
display cos(0)                # 1.0
display tan(0)                # 0.0

# Angles in radians
display sin(PI / 2)           # 1.0
display cos(PI)               # -1.0
```

### Rounding

```vex
use math

display floor(3.7)            # 3
display ceil(3.2)             # 4
display round(3.5)            # 4 or 3 (depends on implementation)
```

### Random Numbers

```vex
use math

display random()              # Random float between 0 and 1
display random() * 100        # Random number 0-100
```

### Logarithm

```vex
use math

display log(10)               # Natural logarithm
display log(2.71828)          # ≈ 1.0
```

---

## String Module

Import with: `use string`

### Case Operations

```vex
use string

display upper("hello")        # "HELLO"
display lower("HELLO")        # "hello"
display lower("HeLLo")        # "hello"
```

### Discovery

```vex
use string

let text be "hello world"

display len(text)             # 11
display text contains "world" # true
display text contains "xyz"   # false

# Position (if implemented)
display index_of(text, "world")  # 6
```

### Manipulation

```vex
use string

let text be "  hello world  "

# Trim whitespace
display trim(text)            # "hello world"

# Replace
display replace("hello world", "hello", "bye")  # "bye world"

# Substring
display substr("hello", 1, 3) # "ell"
```

### Splitting & Joining

```vex
use string

let csv be "apple,banana,cherry"
let fruits be split(csv, ",")
# fruits = ["apple", "banana", "cherry"]

let words be ["Hello", "World"]
let sentence be join(words, " ")
# sentence = "Hello World"
```

---

## Collections Module

Import with: `use collections`

### Array Operations

```vex
use collections

let arr be [1, 2, 3, 4, 5]

# Add to end
push(arr, 6)                  # [1, 2, 3, 4, 5, 6]

# Remove from end
let last be pop(arr)          # last = 6

# Get length
display len(arr)              # 5

# Check membership
if 3 in arr:
    display "Found"

# Sorting
let unsorted be [3, 1, 4, 1, 5, 9]
let sorted be sort(unsorted)  # [1, 1, 3, 4, 5, 9]
```

### Finding Elements

```vex
use collections

let nums be [1, 2, 3, 4, 5]

# Find first index
let idx be index_of(nums, 3)  # 2

# Check if exists
if 4 in nums:
    display "Found"

# Find with condition
fn first_even(items):
    for each item in items:
        if item % 2 is 0:
            give back item
    give back nothing

display first_even(nums)      # 2
```

### Transforming Arrays

```vex
use collections

let nums be [1, 2, 3, 4, 5]

# Built-in map (if available)
let doubled be [x * 2 for x in nums]
# [2, 4, 6, 8, 10]

# Filter
let evens be [x for x in nums if x % 2 is 0]
# [2, 4]

# Reduce/Fold
let sum be 0
for each n in nums:
    sum be sum + n
display sum                   # 15
```

### Map Operations

```vex
use collections

let dict be {"a": 1, "b": 2, "c": 3}

# Get value
display dict.a                # 1

# Set value
dict.d be 4

# Check key exists
if "b" in dict:
    display "Key exists"

# Get all keys
let keys be []
for each key in dict:
    push(keys, key)
# keys = ["a", "b", "c", "d"]

# Get length
display len(dict)             # 4
```

---

## File System Module

Import with: `use fs`

### Reading Files

```vex
use fs

# Read entire file
let content be read_file("data.txt")
display content

# Read with error handling
attempt:
    let data be read_file("missing.txt")
otherwise err:
    display "Error: " + err
```

### Writing Files

```vex
use fs

# Write to file (overwrites if exists)
write_file("output.txt", "Hello, World!")

# Append to file
append_file("log.txt", "New entry")

# Write with error handling
attempt:
    write_file("/root/file.txt", "data")
otherwise err:
    display "Permission denied"
```

### File Information

```vex
use fs

# Check if file exists
if file_exists("data.txt"):
    display "File found"

# Delete file
delete_file("temp.txt")

# Create directory
create_directory("my_folder")
```

### Working with Paths

```vex
use fs

let path be "folder/subfolder/file.txt"

# Get directory
let dir be dirname(path)      # "folder/subfolder"

# Get filename
let name be basename(path)    # "file.txt"

# Get extension
let ext be extension(path)    # ".txt"

# Join paths
let full be join_path("data", "users.txt")
# "data/users.txt"
```

---

## System Module

Import with: `use sys`

### Environment

```vex
use sys

# Get environment variable
let home be getenv("HOME")
display home

# Set environment variable
setenv("MY_VAR", "value")

# Get current working directory
let cwd be get_cwd()
display cwd

# Change directory
change_cwd("/home/user")
```

### Process Information

```vex
use sys

# Get command-line arguments
let args be get_args()
display args.0                # program name
display args.1                # first argument

# Exit program
# exit(0)                      # Exit with code 0
```

### System Information

```vex
use sys

# Get current time (Unix timestamp)
let now be current_time()
display now

# Sleep for milliseconds
sleep(1000)                   # Sleep 1 second

# Execute system command
let result be exec("echo hello")
display result                # "hello"
```

---

## JSON Module

Import with: `use json`

### Parsing JSON

```vex
use json

let json_str be '{"name": "Alice", "age": 30}'
let data be parse_json(json_str)

display data.name             # "Alice"
display data.age              # 30
```

### Converting to JSON

```vex
use json

let user be {
    "name": "Alice",
    "age": 30,
    "active": true
}

let json_str be stringify_json(user)
display json_str
# {"name": "Alice", "age": 30, "active": true}
```

### Complex JSON

```vex
use json

let complex be {
    "users": [
        {"id": 1, "name": "Alice"},
        {"id": 2, "name": "Bob"}
    ],
    "metadata": {
        "version": "1.0",
        "count": 2
    }
}

let json be stringify_json(complex)

# Parse it back
let parsed be parse_json(json)
display parsed.users.0.name   # "Alice"
display parsed.metadata.version # "1.0"
```

---

## Common Patterns

### Reading and Parsing a Data File

```vex
use fs
use json

attempt:
    let content be read_file("data.json")
    let data be parse_json(content)
    
    for each item in data:
        display item.name + ": " + str(item.value)
otherwise err:
    display "Error reading file: " + err
```

### Processing a List of Numbers

```vex
use math

let numbers be [3, 7, 2, 9, 1]

# Find max
let maximum be 0
for each n in numbers:
    if n is greater than maximum:
        maximum be n

display maximum               # 9

# Find min
let minimum be numbers.0
for each n in numbers:
    if n is less than minimum:
        minimum be n

display minimum               # 1

# Calculate average
let sum be 0
for each n in numbers:
    sum be sum + n
let average be sum / len(numbers)
display average               # 4.4
```

### Working with File Lines

```vex
use fs

let content be read_file("data.txt")

# Split into lines
let lines be split(content, "\n")

# Process each line
for each line in lines:
    if len(line) is greater than 0:
        display line
```

### Creating CSV

```vex
use fs

let records be [
    {"name": "Alice", "age": 30},
    {"name": "Bob", "age": 25},
    {"name": "Charlie", "age": 35}
]

let csv be "name,age\n"

for each record in records:
    csv be csv + record.name + "," + str(record.age) + "\n"

write_file("output.csv", csv)
```

---

## Error Handling

### With Try-Catch

```vex
attempt:
    let file be read_file("important.txt")
    display file
otherwise error:
    display "Could not read file"
```

### With Validation

```vex
use fs

fn safe_read(filename):
    if not file_exists(filename):
        give back nothing
    
    attempt:
        give back read_file(filename)
    otherwise err:
        display "Error: " + err
        give back nothing

let content be safe_read("data.txt")
if content is not nothing:
    display content
else:
    display "Failed to read file"
```

---

## Module Import Reference

All available modules:

```vex
use math              # Math functions and constants
use string            # String operations
use collections       # Array and map operations
use fs                # File system operations
use sys               # System operations
use json              # JSON parsing and generation
```

You can also create and import user-defined modules. See `VEXIUM_MODULES.md` for details.

---

This stdlib reference covers built-in functionality. For more advanced topics, check other guides.
