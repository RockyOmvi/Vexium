# Standard Library Implementation Guide

> **For developers implementing stdlib modules in C**

---

## Overview

The Vex stdlib provides 50+ built-in functions across 8 modules. This guide specifies function signatures, behavior, and implementation guidance.

---

## Module: Math

**File:** `src/stdlib_math.c`

### Constants

```c
#include <math.h>

void math_module_init(VM* vm) {
    // Numeric constants
    vm_define_native(vm, "PI", native_math_pi, 0);
    vm_define_native(vm, "E", native_math_e, 0);
    vm_define_native(vm, "INFINITY", native_math_inf, 0);
    vm_define_native(vm, "NAN", native_math_nan, 0);
}

// Implementation
static Value native_math_pi(int argc, Value* args) {
    if (argc != 0) return VALUE_ERROR("PI takes 0 arguments");
    return VALUE_FLOAT(3.14159265358979323846);
}

static Value native_math_e(int argc, Value* args) {
    if (argc != 0) return VALUE_ERROR("E takes 0 arguments");
    return VALUE_FLOAT(2.71828182845904523536);
}

static Value native_math_inf(int argc, Value* args) {
    if (argc != 0) return VALUE_ERROR("INFINITY takes 0 arguments");
    return VALUE_FLOAT(INFINITY);
}

static Value native_math_nan(int argc, Value* args) {
    if (argc != 0) return VALUE_ERROR("NAN takes 0 arguments");
    return VALUE_FLOAT(NAN);
}
```

### Trigonometric Functions

```c
// sin(x: float) -> float
// Returns sine of x (x in radians)
static Value native_sin(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("sin() takes 1 argument");
    if (!is_float(args[0]) && !is_int(args[0])) {
        return VALUE_ERROR("sin() requires numeric argument");
    }
    double x = as_float(args[0]);
    return VALUE_FLOAT(sin(x));
}

vm_define_native(vm, "sin", native_sin, 1);

// cos(x: float) -> float
// Returns cosine of x (x in radians)
static Value native_cos(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("cos() takes 1 argument");
    double x = as_float(args[0]);
    return VALUE_FLOAT(cos(x));
}

vm_define_native(vm, "cos", native_cos, 1);

// tan(x: float) -> float
// Returns tangent of x (x in radians)
static Value native_tan(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("tan() takes 1 argument");
    double x = as_float(args[0]);
    return VALUE_FLOAT(tan(x));
}

vm_define_native(vm, "tan", native_tan, 1);

// asin(x: float) -> float
// Returns arcsine of x (result in radians)
static Value native_asin(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("asin() takes 1 argument");
    double x = as_float(args[0]);
    if (x < -1.0 || x > 1.0) {
        return VALUE_ERROR("asin() domain error: x must be in [-1, 1]");
    }
    return VALUE_FLOAT(asin(x));
}

vm_define_native(vm, "asin", native_asin, 1);

// acos(x: float) -> float
// Returns arccosine of x (result in radians)
static Value native_acos(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("acos() takes 1 argument");
    double x = as_float(args[0]);
    if (x < -1.0 || x > 1.0) {
        return VALUE_ERROR("acos() domain error: x must be in [-1, 1]");
    }
    return VALUE_FLOAT(acos(x));
}

vm_define_native(vm, "acos", native_acos, 1);

// atan(x: float) -> float
// Returns arctangent of x (result in radians)
static Value native_atan(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("atan() takes 1 argument");
    double x = as_float(args[0]);
    return VALUE_FLOAT(atan(x));
}

vm_define_native(vm, "atan", native_atan, 1);

// degrees(radians: float) -> float
// Converts radians to degrees
static Value native_degrees(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("degrees() takes 1 argument");
    double rad = as_float(args[0]);
    return VALUE_FLOAT(rad * 180.0 / 3.14159265358979323846);
}

vm_define_native(vm, "degrees", native_degrees, 1);

// radians(degrees: float) -> float
// Converts degrees to radians
static Value native_radians(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("radians() takes 1 argument");
    double deg = as_float(args[0]);
    return VALUE_FLOAT(deg * 3.14159265358979323846 / 180.0);
}

vm_define_native(vm, "radians", native_radians, 1);
```

### Basic Math Functions

```c
// abs(x: float|int) -> float|int
// Returns absolute value
static Value native_abs(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("abs() takes 1 argument");
    if (is_int(args[0])) {
        int64_t val = as_int(args[0]);
        return VALUE_INT(val < 0 ? -val : val);
    } else {
        double val = as_float(args[0]);
        return VALUE_FLOAT(val < 0 ? -val : val);
    }
}

// sqrt(x: float) -> float
// Returns square root
static Value native_sqrt(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("sqrt() takes 1 argument");
    double x = as_float(args[0]);
    if (x < 0) return VALUE_ERROR("sqrt() domain error: x must be >= 0");
    return VALUE_FLOAT(sqrt(x));
}

// pow(x: float, y: float) -> float
// Returns x raised to power y
static Value native_pow(int argc, Value* args) {
    if (argc != 2) return VALUE_ERROR("pow() takes 2 arguments");
    double x = as_float(args[0]);
    double y = as_float(args[1]);
    return VALUE_FLOAT(pow(x, y));
}

// floor(x: float) -> float
// Returns largest integer <= x
static Value native_floor(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("floor() takes 1 argument");
    return VALUE_FLOAT(floor(as_float(args[0])));
}

// ceil(x: float) -> float
// Returns smallest integer >= x
static Value native_ceil(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("ceil() takes 1 argument");
    return VALUE_FLOAT(ceil(as_float(args[0])));
}

// round(x: float) -> float
// Returns x rounded to nearest integer
static Value native_round(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("round() takes 1 argument");
    return VALUE_FLOAT(round(as_float(args[0])));
}

// log(x: float, base: float) -> float
// Returns logarithm of x with given base
static Value native_log(int argc, Value* args) {
    if (argc != 2) return VALUE_ERROR("log() takes 2 arguments");
    double x = as_float(args[0]);
    double base = as_float(args[1]);
    if (x <= 0) return VALUE_ERROR("log() domain error: x must be > 0");
    if (base <= 0 || base == 1.0) {
        return VALUE_ERROR("log() error: base must be > 0 and != 1");
    }
    return VALUE_FLOAT(log(x) / log(base));
}

// ln(x: float) -> float
// Returns natural logarithm of x
static Value native_ln(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("ln() takes 1 argument");
    double x = as_float(args[0]);
    if (x <= 0) return VALUE_ERROR("ln() domain error: x must be > 0");
    return VALUE_FLOAT(log(x));
}

// exp(x: float) -> float
// Returns e^x
static Value native_exp(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("exp() takes 1 argument");
    return VALUE_FLOAT(exp(as_float(args[0])));
}
```

### Number Theory

```c
// gcd(a: int, b: int) -> int
// Greatest common divisor
static Value native_gcd(int argc, Value* args) {
    if (argc != 2) return VALUE_ERROR("gcd() takes 2 arguments");
    int64_t a = labs(as_int(args[0]));
    int64_t b = labs(as_int(args[1]));
    
    while (b != 0) {
        int64_t temp = b;
        b = a % b;
        a = temp;
    }
    return VALUE_INT(a);
}

// lcm(a: int, b: int) -> int
// Least common multiple
static Value native_lcm(int argc, Value* args) {
    if (argc != 2) return VALUE_ERROR("lcm() takes 2 arguments");
    int64_t a = as_int(args[0]);
    int64_t b = as_int(args[1]);
    
    Value gcd_result = native_gcd(2, args);
    int64_t gcd_val = as_int(gcd_result);
    
    return VALUE_INT((a / gcd_val) * b);
}

// factorial(n: int) -> int
static Value native_factorial(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("factorial() takes 1 argument");
    int64_t n = as_int(args[0]);
    if (n < 0) return VALUE_ERROR("factorial() error: n must be >= 0");
    if (n > 20) return VALUE_ERROR("factorial() error: result too large (n > 20)");
    
    int64_t result = 1;
    for (int64_t i = 2; i <= n; i++) {
        result *= i;
    }
    return VALUE_INT(result);
}
```

---

## Module: String

**File:** `src/stdlib_string.c`

```c
void string_module_init(VM* vm) {
    vm_define_native(vm, "ascii_lowercase", native_ascii_lower, 0);
    vm_define_native(vm, "ascii_uppercase", native_ascii_upper, 0);
    vm_define_native(vm, "digits", native_digits, 0);
    vm_define_native(vm, "whitespace", native_whitespace, 0);
    vm_define_native(vm, "is_digit", native_is_digit, 1);
    vm_define_native(vm, "is_alpha", native_is_alpha, 1);
    // ... etc
}

// Constants
static Value native_ascii_lower(int argc, Value* args) {
    if (argc != 0) return VALUE_ERROR("ascii_lowercase takes 0 arguments");
    return VALUE_OBJ((Obj*)copy_string("abcdefghijklmnopqrstuvwxyz", 26));
}

static Value native_ascii_upper(int argc, Value* args) {
    if (argc != 0) return VALUE_ERROR("ascii_uppercase takes 0 arguments");
    return VALUE_OBJ((Obj*)copy_string("ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26));
}

static Value native_digits(int argc, Value* args) {
    if (argc != 0) return VALUE_ERROR("digits takes 0 arguments");
    return VALUE_OBJ((Obj*)copy_string("0123456789", 10));
}

static Value native_whitespace(int argc, Value* args) {
    if (argc != 0) return VALUE_ERROR("whitespace takes 0 arguments");
    return VALUE_OBJ((Obj*)copy_string(" \t\n\r\f\v", 6));
}

// Character type checks
static Value native_is_digit(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("is_digit() takes 1 argument");
    if (!is_string(args[0])) return VALUE_FALSE;
    
    ObjString* str = as_string(args[0]);
    if (str->length != 1) return VALUE_FALSE;
    
    return isdigit((unsigned char)str->chars[0]) ? VALUE_TRUE : VALUE_FALSE;
}

static Value native_is_alpha(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("is_alpha() takes 1 argument");
    if (!is_string(args[0])) return VALUE_FALSE;
    
    ObjString* str = as_string(args[0]);
    if (str->length != 1) return VALUE_FALSE;
    
    return isalpha((unsigned char)str->chars[0]) ? VALUE_TRUE : VALUE_FALSE;
}

static Value native_is_alnum(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("is_alphanumeric() takes 1 argument");
    if (!is_string(args[0])) return VALUE_FALSE;
    
    ObjString* str = as_string(args[0]);
    if (str->length != 1) return VALUE_FALSE;
    
    return isalnum((unsigned char)str->chars[0]) ? VALUE_TRUE : VALUE_FALSE;
}

// String transformations
static Value native_capitalize(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("capitalize() takes 1 argument");
    if (!is_string(args[0])) return VALUE_ERROR("capitalize() requires string");
    
    ObjString* str = as_string(args[0]);
    if (str->length == 0) return args[0];
    
    char* result = malloc(str->length + 1);
    strcpy(result, str->chars);
    result[0] = toupper((unsigned char)result[0]);
    
    Value ret = VALUE_OBJ((Obj*)copy_string(result, str->length));
    free(result);
    return ret;
}
```

---

## Module: filesystem (fs)

**File:** `src/stdlib_fs.c`

```c
void fs_module_init(VM* vm) {
    vm_define_native(vm, "read", native_fs_read, 1);
    vm_define_native(vm, "write", native_fs_write, 2);
    vm_define_native(vm, "exists", native_fs_exists, 1);
    vm_define_native(vm, "is_file", native_fs_is_file, 1);
    vm_define_native(vm, "is_dir", native_fs_is_dir, 1);
    // ... etc
}

// read(path: str) -> str
static Value native_fs_read(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("fs.read() takes 1 argument");
    if (!is_string(args[0])) return VALUE_ERROR("read() requires string path");
    
    ObjString* path = as_string(args[0]);
    
    FILE* file = fopen(path->chars, "rb");
    if (file == NULL) {
        return VALUE_ERROR("FileNotFoundError");
    }
    
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);
    
    char* buffer = malloc(file_size + 1);
    size_t bytes_read = fread(buffer, 1, file_size, file);
    buffer[bytes_read] = '\0';
    
    fclose(file);
    
    Value ret = VALUE_OBJ((Obj*)copy_string(buffer, bytes_read));
    free(buffer);
    return ret;
}

// write(path: str, content: str) -> bool
static Value native_fs_write(int argc, Value* args) {
    if (argc != 2) return VALUE_ERROR("fs.write() takes 2 arguments");
    if (!is_string(args[0]) || !is_string(args[1])) {
        return VALUE_ERROR("write() requires string arguments");
    }
    
    ObjString* path = as_string(args[0]);
    ObjString* content = as_string(args[1]);
    
    FILE* file = fopen(path->chars, "wb");
    if (file == NULL) {
        return VALUE_ERROR("PermissionError");
    }
    
    size_t bytes_written = fwrite(content->chars, 1, content->length, file);
    fclose(file);
    
    return bytes_written == content->length ? VALUE_TRUE : VALUE_FALSE;
}

// exists(path: str) -> bool
static Value native_fs_exists(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("fs.exists() takes 1 argument");
    if (!is_string(args[0])) return VALUE_ERROR("exists() requires string path");
    
    ObjString* path = as_string(args[0]);
    FILE* file = fopen(path->chars, "r");
    if (file) {
        fclose(file);
        return VALUE_TRUE;
    }
    return VALUE_FALSE;
}
```

---

## Module: JSON

**File:** `src/stdlib_json.c`

```c
// Minimal JSON parser/writer
static Value native_json_parse(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("json.parse() takes 1 argument");
    if (!is_string(args[0])) return VALUE_ERROR("parse() requires string");
    
    ObjString* json_str = as_string(args[0]);
    
    // Simple JSON parser (for v2-alpha, use minimal parser)
    // Full implementation: parse JSON string into Vex values
    //
    // For MVP:
    // - Parse objects: {"key": value}
    // - Parse arrays: [value, value, ...]
    // - Parse primitives: null, true, false, numbers, strings
    
    // Return parsed value (map for objects, array for arrays, etc.)
    return VALUE_ERROR("json.parse() not yet implemented");
}

static Value native_json_stringify(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("json.stringify() takes 1 argument");
    
    // Convert any Vex value to JSON string
    // - Maps → JSON objects
    // - Arrays → JSON arrays
    // - Strings → JSON strings
    // - Numbers → JSON numbers
    // - Booleans → JSON booleans
    // - nothing → null
    
    return VALUE_ERROR("json.stringify() not yet implemented");
}
```

---

## Module: time

**File:** `src/stdlib_time.c`

```c
void time_module_init(VM* vm) {
    vm_define_native(vm, "now", native_time_now, 0);
    vm_define_native(vm, "sleep", native_time_sleep, 1);
}

// now() -> float
// Returns seconds since Unix epoch
static Value native_time_now(int argc, Value* args) {
    if (argc != 0) return VALUE_ERROR("now() takes 0 arguments");
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    double seconds = ts.tv_sec + ts.tv_nsec / 1e9;
    return VALUE_FLOAT(seconds);
}

// sleep(seconds: float)
static Value native_time_sleep(int argc, Value* args) {
    if (argc != 1) return VALUE_ERROR("sleep() takes 1 argument");
    
    double seconds = as_float(args[0]);
    if (seconds < 0) return VALUE_ERROR("sleep() time cannot be negative");
    
    struct timespec req;
    req.tv_sec = (time_t)seconds;
    req.tv_nsec = (long)((seconds - req.tv_sec) * 1e9);
    
    nanosleep(&req, NULL);
    return VALUE_NOTHING;
}
```

---

## Module: collections

**File:** `src/stdlib_collections.c`

```c
// Queue implementation
typedef struct {
    Value* items;
    int front, rear, size, capacity;
} Queue;

static Value native_create_queue(int argc, Value* args) {
    if (argc != 0) return VALUE_ERROR("create_queue() takes 0 arguments");
    
    Queue* q = malloc(sizeof(Queue));
    q->capacity = 16;
    q->items = malloc(sizeof(Value) * q->capacity);
    q->front = 0;
    q->rear = -1;
    q->size = 0;
    
    // Wrap in Vex object for lifecycle management
    return VALUE_OBJ((Obj*)wrap_queue(q));
}

// Stack implementation
typedef struct {
    Value* items;
    int top;
    int capacity;
} Stack;

static Value native_create_stack(int argc, Value* args) {
    if (argc != 0) return VALUE_ERROR("create_stack() takes 0 arguments");
    
    Stack* s = malloc(sizeof(Stack));
    s->capacity = 16;
    s->items = malloc(sizeof(Value) * s->capacity);
    s->top = -1;
    
    return VALUE_OBJ((Obj*)wrap_stack(s));
}
```

---

## Native Function Registration

```c
// Central registration in vm.c
void vm_register_stdlib(VM* vm) {
    // Core I/O
    vm_define_native(vm, "display", native_display, 1);
    vm_define_native(vm, "input", native_input, 1);
    
    // Core math
    vm_define_native(vm, "abs", native_abs, 1);
    vm_define_native(vm, "sqrt", native_sqrt, 1);
    vm_define_native(vm, "round", native_round, 1);
    vm_define_native(vm, "floor", native_floor, 1);
    vm_define_native(vm, "ceil", native_ceil, 1);
    
    // Core type checks
    vm_define_native(vm, "type", native_type, 1);
    vm_define_native(vm, "len", native_len, 1);
    
    // Modules (lazy initialize on first 'use')
    // ...
}
```

---

## Implementation Checklist

### Priority 1 (v2.0 MVP)
- [ ] Math: abs, sqrt, floor, ceil, round, pow, sin, cos, tan, min, max
- [ ] String: upper, lower, length, split, contains, replace, substring
- [ ] FS: read, write, exists, append
- [ ] JSON: parse (basic), stringify (basic)
- [ ] Collections: array methods (push, pop, map, filter, sort)
- [ ] I/O: display, input

### Priority 2 (v2.0 release)
- [ ] Math: factorial, gcd, lcm, log, ln, exp, asin, acos, atan
- [ ] String: trim, reverse, index_of, start_with, end_with
- [ ] FS: create_dir, remove, is_file, is_dir, list_dir
- [ ] JSON: full parsing, validation
- [ ] Time: now, sleep
- [ ] Collections: queue, stack, set

### Priority 3 (v2.1+)
- [ ] String: capitalize, title_case, center, ljust, rjust
- [ ] FS: get_size, get_modified_time, copy_file, move
- [ ] HTTP: get, post, create_server
- [ ] Concurrent: task, await, channels

---

## Testing

Each stdlib function MUST have tests:

```vex
# tests/test_stdlib_math.vxm
use math

# Test sqrt
assert_equal(sqrt(4), 2.0)
assert_equal(sqrt(16), 4.0)

# Test abs
assert_equal(abs(-5), 5)
assert_equal(abs(3), 3)

# Test round
assert_equal(round(3.7), 4)
assert_equal(round(3.2), 3)

display "All math tests passed!"
```

Run with:
```bash
$ vex run tests/test_stdlib_math.vxm
```

---

