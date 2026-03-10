# Vexium Type System

## Overview

The Vexium type system implements **gradual typing** - you can write code with or without type annotations, and the compiler will infer types where possible while maintaining runtime compatibility.

## Type Kinds

### Primitive Types

| Type | Description | Example |
|------|-------------|---------|
| `int` | 64-bit signed integer | `42`, `-7`, `0xFF` |
| `float` | 64-bit floating point | `3.14`, `-0.5` |
| `bool` | Boolean value | `true`, `false` |
| `str` | UTF-8 string | `"hello"` |
| `nothing` | Void/nil | `nothing` |
| `dynamic` | Any type | Inferred when no type is known |

### Composite Types

| Type | Syntax | Example |
|------|--------|---------|
| `Array<T>` | `[T]` | `[1, 2, 3]` → `[int]` |
| `Map<K, V>` | `{K: V}` | `{"a": 1}` → `{str: int}` |
| `Tuple` | `(T1, T2, ...)` | `(1, "a", true)` |
| `Option<T>` | `?T` | Option to hold value or nothing |
| `Union` | `A \| B` | `int \| str` |
| `Function` | `(params) -> ret` | `(int, int) -> int` |

## Type Inference

Vexium infers types from context:

```vexium
# Primitive inference
let x be 42           # x: int
let y be 3.14         # y: float
let name be "Vex"     # name: str
let flag be true      # flag: bool

# Array inference
let nums be [1, 2, 3]       # nums: [int]
let words be ["a", "b"]     # words: [str]

# Map inference
let scores be {"A": 95}     # scores: {str: int}

# Expression inference
let sum be 10 + 20          # sum: int
let avg be sum / 2.0        # avg: float
let check be x > 0          # check: bool
```

## Type Context

The `TypeContext` tracks type bindings during inference:

```c
typedef struct TypeContext {
    TypeBinding bindings;           /* Variable name → type */
    TypeBinding generic_bindings;   /* Generic param → concrete type */
    bool strict_mode;               /* Strict type checking? */
    int next_type_var_id;           /* For generating fresh type variables */
    bool had_error;
    char error_msg[512];
} TypeContext;
```

## Generic Types

Generic types allow type-safe collections and functions:

```vexium
# Generic array
let numbers be [1, 2, 3]        # Array<int>
let names be ["A", "B"]         # Array<str>

# Generic map
let scores be {"A": 95}         # Map<str, int>

# Generic option
let maybe_num be ?42            # Option<int>
let maybe_name be ?nothing      # Option<str>
```

## Function Types

Functions can have explicit or inferred types:

```vexium
# Inferred return type
fn add(a, b):
    give back a + b             # (int, int) -> int

fn greet(name):
    give back "Hi, " + name     # (str) -> str

# With explicit type annotations (future)
fn max(a: int, b: int) -> int:
    if a > b:
        give back a
    give back b
```

## API Reference

### Type Creation

```c
Type* type_create_int(void);
Type* type_create_float(void);
Type* type_create_bool(void);
Type* type_create_string(void);
Type* type_create_nothing(void);
Type* type_create_dynamic(void);

Type* type_create_array(Type* element);
Type* type_create_map(Type* key, Type* value);
Type* type_create_option(Type* inner);
Type* type_create_function(Type** params, int count, Type* ret);
Type* type_create_struct(const char* name);
Type* type_create_tuple(Type** types, int count);
Type* type_create_union(Type** types, int count);
Type* type_create_variable(const char* name, int id);
```

### Type Context Operations

```c
void type_context_init(TypeContext* ctx, bool strict);
void type_context_free(TypeContext* ctx);
void type_context_bind(TypeContext* ctx, const char* name, Type* type);
Type* type_context_lookup(TypeContext* ctx, const char* name);
Type* type_fresh_variable(TypeContext* ctx, const char* prefix);
```

### Type Inference

```c
Type* infer_from_node(ASTNode* node, TypeContext* ctx);
Type* infer_literal(ASTNode* node);
Type* infer_binary(ASTNode* node, TypeContext* ctx);
Type* infer_unary(ASTNode* node, TypeContext* ctx);
Type* infer_identifier(ASTNode* node, TypeContext* ctx);
Type* infer_array(ASTNode* node, TypeContext* ctx);
Type* infer_map(ASTNode* node, TypeContext* ctx);
```

### Type Comparison

```c
bool type_equals(Type* a, Type* b);
bool type_unify(Type* a, Type* b);
bool type_is_subtype(Type* sub, Type* super);
bool type_is_assignable(Type* target, Type* source);
bool type_is_concrete(Type* type);
bool type_occurs_in(Type* var, Type* type);
```

### Type Utilities

```c
char* type_to_string(Type* type);
Type* type_copy(Type* type);
void type_free(Type* type);
Type* type_apply(Type* type, TypeContext* ctx);
```

## Usage Example

```c
#include "type_system.h"

// Initialize type system
type_system_init();

// Create a type context for inference
TypeContext ctx;
type_context_init(&ctx, false);  // Non-strict mode

// Infer type of an expression
ASTNode* node = /* parse expression */;
Type* inferred = infer_from_node(node, &ctx);

// Print the inferred type
char* str = type_to_string(inferred);
printf("Inferred type: %s\n", str);
free(str);

// Cleanup
type_context_free(&ctx);
type_system_shutdown();
```

## Implementation Notes

1. **Singleton Types**: Primitive types (`int`, `float`, `bool`, `str`, `nothing`, `dynamic`) are singletons for memory efficiency.

2. **Type Variables**: Used during inference. Variables with `id > 0` are fresh type variables that get unified with concrete types.

3. **Memory Management**: Use `type_free()` to release type structures. Singletons are automatically excluded from freeing.

4. **String Representation**: `type_to_string()` uses a static buffer - call `strdup()` if you need persistence.

5. **Error Handling**: Check `ctx->had_error` after inference operations to detect type errors.

## Future Enhancements

- [ ] Type annotations in function signatures
- [ ] Generic type constraints (`fn sort<T: Comparable>(arr: [T])`)
- [ ] Type aliases (`type ID be int`)
- [ ] Structural typing for interfaces
- [ ] Runtime type checking and reflection
