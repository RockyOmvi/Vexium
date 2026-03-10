# Error Handling Implementation Guide

> **For developers implementing and extending Vex v2 error handling system**

---

## Overview

Vex v2 provides structured error handling with:
- **try-catch blocks** (`attempt...otherwise`)
- **Error objects** with messages and type information
- **Stack traces** for debugging
- **Error propagation** through function calls
- **Custom error types** (user-defined)
- **Error recovery** patterns

---

## Error Representation

### Error Value Type

```c
typedef struct {
    char* message;              /* Error message */
    char* error_type;           /* Error category: "TypeError", "ValueError", etc. */
    VexValue details;           /* Additional error info */
    int line;                   /* Source code line number */
    char* file;                 /* Source file name */
    StackFrame* stack_trace;    /* Call stack at error time */
} ErrorValue;

typedef struct {
    ValueType type;             /* VAL_ERROR */
    union {
        ErrorValue error;
    } as;
} VexError;
```

### Predefined Error Types

```c
enum ErrorType {
    ERR_TYPE_ERROR,            /* Type mismatch: got X, want Y */
    ERR_VALUE_ERROR,           /* Invalid value: X is out of range */
    ERR_RUNTIME_ERROR,         /* Div by zero, null access, etc. */
    ERR_ASSERTION_ERROR,       /* assert() failed */
    ERR_IO_ERROR,              /* File not found, permission denied */
    ERR_SYNTAX_ERROR,          /* parse error during eval */
    ERR_NAME_ERROR,            /* Undefined variable */
    ERR_ATTRIBUTE_ERROR,       /* No such property on object */
    ERR_INDEX_ERROR,           /* Index out of bounds */
    ERR_KEY_ERROR,             /* Key not found in map */
    ERR_CUSTOM,                /* User-defined error type */
};
```

---

## Syntax: Try-Catch Blocks

### Basic Syntax

```vex
attempt {
    # Code that might raise an error
    let result = risky_operation()
    display result
} otherwise error {
    # Handle the error
    display "Error occurred: " + error.message
}
```

### Error Object

```vex
attempt {
    divide(10, 0)  # Will raise error
} otherwise err {
    display err.message      # "Division by zero" 
    display err.type         # "ValueError"
    display err.line         # 2 (line number)
}
```

### Without Error Variable

```vex
attempt {
    file_read("missing.txt")
} otherwise {
    # Just ignore the error
    display "File not found"
}
```

### Nested Try-Catch

```vex
attempt {
    attempt {
        operation_a()
    } otherwise inner {
        # Handle inner error
        operation_b()  # Might also error
    }
} otherwise outer {
    # Handle outer error
    display "Final error: " + outer.message
}
```

---

## Creating and Throwing Errors

### Throw Statement

```vex
# Basic error
throw "Something went wrong"

# With error type
throw ValueError("Expected integer, got string")

# With structured message
fn validate_age(age: int) {
    if age < 0 {
        throw ValueError("Age cannot be negative: " + age)
    }
    if age > 150 {
        throw ValueError("Age unrealistic: " + age)
    }
}
```

### Error Constructors

```vex
# Built-in error constructors
TypeError(message: str)
ValueError(message: str)
RuntimeError(message: str)
IOError(message: str)
AssertionError(message: str)

# Example
let err = TypeError("Expected int, got str")
throw err
```

### Error Propagation

```vex
# Errors automatically propagate up the call stack
fn level3() {
    throw ValueError("Something failed")   # Unhandled - propagates
}

fn level2() {
    level3()  # Error propagates through
}

fn level1() {
    attempt {
        level2()  # Error caught here
    } otherwise err {
        display err.message
    }
}
```

---

## Error Object Structure

### Properties

```vex
attempt {
    divide(10, 0)
} otherwise error {
    # Error properties
    error.message         # str: Human-readable error message
    error.type            # str: Error type name
    error.line            # int: Line number in source
    error.file            # str: Filename
    error.stack_trace     # [stackframe]: Call stack at error time
    
    # Stack frame structure
    frame.function        # str: Function name
    frame.file            # str: Filename
    frame.line            # int: Line number
}
```

### Example

```vex
attempt {
    fn bad_math(x) {
        return 10 / x
    }
    bad_math(0)
} otherwise err {
    display "Error: " + err.type           # "ValueError"
    display err.message                    # "Division by zero"
    display err.line                       # 4
    display "Function: " + err.stack_trace[0].function
}
```

---

## Built-in Error Handling Functions

### Error Checking

```vex
# Check if value is an error
is_error(value: any) -> bool
    
# Get error message
error_message(err: error) -> str

# Get error type
error_type(err: error) -> str

# Create error without throwing
make_error(type: str, message: str) -> error
    
# Format error with stack trace for debugging
error_format(err: error) -> str
```

### Example

```vex
let result = attempt_divide(10, 0)

if is_error(result) {
    display "Division failed: " + error_message(result)
    display error_format(result)  # Full stack trace
} else {
    display result
}
```

---

## Error Handling Patterns

### Return Error Instead of Throwing

```vex
fn safe_divide(a: int, b: int) -> int | error {
    if b == 0 {
        return make_error("ValueError", "Cannot divide by zero")
    }
    return a / b
}

# Usage
let result = safe_divide(10, 0)
if is_error(result) {
    display "Error: " + error_message(result)
} else {
    display result
}
```

### Assert Pattern

```vex
# assert(condition) throws AssertionError if false
fn process(data: any) {
    assert(data != nothing, "Data cannot be nil")
    assert(type(data) == "array", "Data must be array")
    
    # Process data safely
    for item in data {
        display item
    }
}
```

### Recover and Retry

```vex
fn fetch_with_retry(url: str, max_retries: int) -> str {
    let attempt_count = 0
    
    while attempt_count < max_retries {
        attempt {
            return http_get(url)
        } otherwise err {
            attempt_count = attempt_count + 1
            if attempt_count >= max_retries {
                throw err  # Give up and propagate
            }
            display "Retry " + attempt_count
        }
    }
    
    throw RuntimeError("Max retries exceeded")
}
```

### Error Chain Pattern

```vex
fn validate_user(user: any) {
    attempt {
        validate_age(user.age)
    } otherwise age_err {
        throw ValueError(
            "Invalid user: " + age_err.message +
            " (at line " + age_err.line + ")"
        )
    }
    
    attempt {
        validate_email(user.email)
    } otherwise email_err {
        throw ValueError(
            "Invalid user: " + email_err.message +
            " (at line " + email_err.line + ")"
        )
    }
}
```

### Default Value Pattern

```vex
fn parse_int_safe(str: string) -> int {
    let result = 0
    
    attempt {
        result = parse_int(str)  # Might error
    } otherwise {
        # Use default if parsing fails
        result = 0
    }
    
    return result
}
```

---

## Stack Traces

### Automatic Stack Traces

When an error occurs, Vex automatically captures:
- Function call sequence
- Line numbers in each frame
- File names
- Variable scope at each level

```vex
fn level3() {
    throw ValueError("Bottom error")
}

fn level2() {
    level3()
}

fn level1() {
    level2()
}

attempt {
    level1()
} otherwise err {
    # Stack trace shows: level1() -> level2() -> level3()
    display error_format(err)  
}

# Output:
# ValueError: Bottom error
#   at level3() [line 2]
#   at level2() [line 6]
#   at level1() [line 10]
#   at <main> [line 14]
```

### Captured Frame Information

```c
typedef struct {
    char* function_name;
    char* filename;
    int line_number;
    int column_number;
    Table<string, VexValue> local_vars;  /* Locals at time of error */
} StackFrame;
```

---

## Custom Error Types

### Defining Custom Errors

```vex
# Define a custom error struct
struct CustomError {
    message: str
    error_type: str
    severity: int  # 1=low, 2=medium, 3=critical
}

# Throw custom error
fn risky_operation() {
    let err = CustomError(
        message = "Operation failed",
        error_type = "CustomError",
        severity = 2
    )
    throw err
}

# Catch and inspect custom error
attempt {
    risky_operation()
} otherwise err {
    display err.severity  # 2
    if err.severity == 3 {
        display "CRITICAL: " + err.message
    }
}
```

---

## Error Handling in Different Contexts

### In Functions

```vex
fn fetch_data(url: str) -> {data: any, error: str | nothing} {
    attempt {
        let response = http_get(url)
        return {data = response, error = nothing}
    } otherwise err {
        return {data = nothing, error = err.message}
    }
}
```

### In Loops

```vex
for item in items {
    attempt {
        process(item)
    } otherwise err {
        # Continue to next item on error
        display "Failed to process: " + err.message
    }
}
```

### In Structs/Methods

```vex
struct Database {
    connection: any
    
    fn query(sql: str) {
        attempt {
            # Execute SQL
            return raw_query(sql)
        } otherwise err {
            # Log and format error for client
            throw ValueError(
                "Database error: " + err.message
            )
        }
    }
}
```

---

## Error Handling Best Practices

### 1. Be Specific with Error Types

```vex
# ❌ Bad: Too generic
throw "Something went wrong"

# ✅ Good: Specific error type
throw ValueError("Expected positive number, got " + value)
```

### 2. Include Context in Messages

```vex
# ❌ Bad: No context
throw ValueError("Invalid age")

# ✅ Good: Include actual value and valid range
throw ValueError("Age must be 0-150, got " + age)
```

### 3. Recover When Possible

```vex
# ❌ Bad: Let errors crash the program
file_read("config.txt")

# ✅ Good: Provide default if file missing
let config = nothing
attempt {
    config = parse_json(file_read("config.txt"))
} otherwise {
    config = {timeout = 30, retries = 3}  # Default config
}
```

### 4. Don't Swallow Errors Silently

```vex
# ❌ Bad: Error hidden
attempt {
    critical_operation()
} otherwise {
    # Just ignoring the error!
}

# ✅ Good: Log or handle properly
attempt {
    critical_operation()
} otherwise err {
    log_error(err.message)  # At least log it
    throw err               # Or re-throw
}
```

### 5. Chain Context Through Errors

```vex
# ❌ Bad: Lose context
attempt {
    validate_user(user)
} otherwise {
    throw ValueError("Validation failed")  # Lost original reason
}

# ✅ Good: Preserve and add context
attempt {
    validate_user(user)
} otherwise original_err {
    throw ValueError(
        "User validation failed: " + original_err.message
    )
}
```

---

## Implementation Checklist

- [x] Parse `attempt...otherwise` statements
- [x] Error value type and representation
- [x] Basic throw and catch mechanism
- [x] Error object with message and type
- [x] Stack trace capture
- [ ] Custom error types (user-defined structs)
- [ ] Error object methods (format, message, type)
- [ ] Error propagation through async operations
- [ ] Finally blocks (`ultimately` keyword)
- [ ] Error filtering (catch specific types only)
- [ ] Standard error library (ValueError, TypeError, etc.)
- [ ] REPL error handling
- [ ] Debugger integration with error info
- [ ] Error logging and reporting utilities

---

## Testing Error Handling

```vex
# tests/test_error_handling.vxm

use test

# Test 1: Basic throw and catch
attempt {
    throw ValueError("Test error")
} otherwise err {
    assert_equal(err.type, "ValueError")
    assert_equal(err.message, "Test error")
}

# Test 2: Error propagation
fn failing_function() {
    throw RuntimeError("Function failed")
}

attempt {
    failing_function()
} otherwise err {
    assert_equal(err.type, "RuntimeError")
}

# Test 3: Return instead of throw
fn safe_divide(a, b) {
    if b == 0 {
        return make_error("ValueError", "Division by zero")
    }
    return a / b
}

let result = safe_divide(10, 0)
assert(is_error(result))

# Test 4: Nested errors
attempt {
    attempt {
        throw ValueError("Inner")
    } otherwise {
        throw RuntimeError("Outer")
    }
} otherwise err {
    assert_equal(err.type, "RuntimeError")
}

display "All error handling tests passed!"
```

---

## Performance Considerations

- **Error capture is expensive** - Stack traces require walking the call stack
- **Use errors for exceptional conditions only** - Don't use as control flow
- **Avoid creating errors in tight loops** - Pre-create error objects instead

```vex
# ❌ Slow: Creates error object in every iteration
for i in 1 to 1000000 {
    if i % 2 == 0 {
        throw ValueError("Even number")
    }
}

# ✅ Fast: Check condition without throwing
for i in 1 to 1000000 {
    if i % 2 == 0 {
        continue
    }
}
```

---

