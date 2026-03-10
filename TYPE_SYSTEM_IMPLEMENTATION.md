# Type System Implementation Guide

> **For developers implementing gradual typing and type checking in Vex v2**

---

## Overview

Vex v2 uses **gradual typing** with inference:
- Optional type annotations (`:` syntax)
- Automatic type inference from initialization and usage
- Strict mode type checking (`vex check`)
- Type narrowing in control flow
- Generics with constraints

This guide specifies implementation details for the type system.

---

## Type Representation in Runtime

```c
// Type descriptor (used for checking and inference)
typedef enum {
    TYPE_DYNAMIC,      // Any type (untyped values)
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_STRING,
    TYPE_ARRAY,        // Generic: Array<T>
    TYPE_MAP,          // Generic: Map<K, V>
    TYPE_TUPLE,        // Fixed tuple: (int, str, bool)
    TYPE_STRUCT,       // Custom struct
    TYPE_FUNCTION,     // Function type: (int, str) -> bool
    TYPE_NOTHING,      // void / nil
    TYPE_OPTION,       // Optional<T>
    TYPE_UNION,        // Type A | Type B
    TYPE_TENSOR,       // n-dimensional array
} ValueType;

// Full type descriptor with generics
typedef struct Type {
    ValueType base;
    
    // For generics
    struct Type* element_type;      // For Array<T>
    struct Type* key_type;          // For Map<K,V>
    struct Type* value_type;        // For Map<K,V>
    
    // For functions
    struct Type** param_types;      // Parameter types
    int param_count;
    struct Type* return_type;
    
    // For structs
    char* struct_name;
    
    // For tagged unions
    struct Type** union_types;
    int union_count;
} Type;

// Type inference context
typedef struct {
    Table<string, Type*> bindings;           // name → type
    Table<string, Type*> generic_bindings;   // T → concrete type
    bool is_strict;                          // strict mode enabled?
} TypeContext;
```

---

## Type Inference Rules

### Hindley-Milner Style Inference

```c
// Infer type from literal expression
Type* infer_literal(Expr* expr, TypeContext* ctx) {
    switch (expr->type) {
        case EXPR_INT:
            return type_create_int();
        case EXPR_FLOAT:
            return type_create_float();
        case EXPR_BOOL:
            return type_create_bool();
        case EXPR_STRING:
            return type_create_string();
        case EXPR_ARRAY: {
            // Infer element type from first element
            if (expr->as_array.count == 0) {
                return type_create_array(type_create_dynamic());
            }
            Type* elem_type = infer_expression(expr->as_array.elements[0], ctx);
            return type_create_array(elem_type);
        }
        case EXPR_MAP: {
            // [[key_type, value_type]] from first pair
            if (expr->as_map.count == 0) {
                return type_create_map(
                    type_create_dynamic(),
                    type_create_dynamic()
                );
            }
            Type* key_type = infer_expression(expr->as_map.keys[0], ctx);
            Type* val_type = infer_expression(expr->as_map.values[0], ctx);
            return type_create_map(key_type, val_type);
        }
        default:
            return type_create_dynamic();
    }
}

// Infer type from variable usage
Type* infer_variable(Expr* expr, TypeContext* ctx) {
    char* var_name = expr->as_identifier.name;
    Type* bound_type = table_get(&ctx->bindings, var_name);
    
    if (bound_type) {
        return bound_type;
    }
    
    // Unbound variable in strict mode
    if (ctx->is_strict) {
        error("Unbound variable: %s", var_name);
    }
    
    return type_create_dynamic();
}

// Infer type from binary operation
Type* infer_binary(Expr* expr, TypeContext* ctx) {
    Type* left = infer_expression(expr->as_binary.left, ctx);
    Type* right = infer_expression(expr->as_binary.right, ctx);
    
    switch (expr->as_binary.op) {
        // Arithmetic returns float or int
        case OP_PLUS:
        case OP_MINUS:
        case OP_STAR:
        case OP_SLASH:
            if (left->base == TYPE_INT && right->base == TYPE_INT) {
                return type_create_int();
            }
            return type_create_float();
        
        // Comparison returns bool
        case OP_LT:
        case OP_GT:
        case OP_LTE:
        case OP_GTE:
        case OP_EQ:
        case OP_NEQ:
            return type_create_bool();
        
        // Logical returns bool
        case OP_AND:
        case OP_OR:
            return type_create_bool();
        
        default:
            return type_create_dynamic();
    }
}

// Infer type from function call
Type* infer_function_call(Expr* expr, TypeContext* ctx) {
    Type* func_type = infer_expression(expr->as_call.callee, ctx);
    
    if (func_type->base == TYPE_FUNCTION) {
        return func_type->return_type;
    }
    
    // Dynamic function type
    return type_create_dynamic();
}
```

### Unification Algorithm

```c
// Type unification
bool unify(Type* type1, Type* type2, TypeContext* ctx) {
    // Same concrete type
    if (type1->base == type2->base) {
        // For generics, unify elements too
        if (type1->base == TYPE_ARRAY) {
            return unify(type1->element_type, type2->element_type, ctx);
        }
        if (type1->base == TYPE_MAP) {
            return unify(type1->key_type, type2->key_type, ctx) &&
                   unify(type1->value_type, type2->value_type, ctx);
        }
        return true;
    }
    
    // Dynamic unifies with anything
    if (type1->base == TYPE_DYNAMIC || type2->base == TYPE_DYNAMIC) {
        return true;
    }
    
    // Numeric types: int <-> float is ok with casting
    if ((type1->base == TYPE_INT || type1->base == TYPE_FLOAT) &&
        (type2->base == TYPE_INT || type2->base == TYPE_FLOAT)) {
        return true;  // Numeric promotion allowed
    }
    
    return false;
}

// Type inference with constraint solving
Type* infer_with_constraints(Stmt* stmt, TypeContext* ctx) {
    // For function declarations
    if (stmt->type == STMT_FUNCTION) {
        Stmt_Function* func = &stmt->as_function;
        
        // Parameters get explicit or inferred types
        Type** param_types = malloc(sizeof(Type*) * func->params.count);
        for (int i = 0; i < func->params.count; i++) {
            if (func->param_types[i]) {
                param_types[i] = parse_type_annotation(func->param_types[i]);
            } else {
                // Infer from function body usage
                param_types[i] = type_create_dynamic();
            }
        }
        
        // Return type
        Type* return_type = type_create_dynamic();
        if (func->return_type_annotation) {
            return_type = parse_type_annotation(func->return_type_annotation);
        } else {
            // Infer from return statements
            return_type = infer_return_type(func->body, ctx);
        }
        
        return type_create_function(param_types, func->params.count, return_type);
    }
}
```

---

## Type Checking: Strict Mode

```c
// Strict type checking (vex check --strict)
bool check_types_strict(Program* program) {
    TypeContext ctx = {0};
    ctx.is_strict = true;
    
    for (int i = 0; i < program->stmt_count; i++) {
        check_statement(program->stmts[i], &ctx);
    }
    
    return !ctx.has_errors;
}

bool check_statement(Stmt* stmt, TypeContext* ctx) {
    switch (stmt->type) {
        case STMT_VAR_DECL: {
            Stmt_VarDecl* decl = &stmt->as_var_decl;
            
            // Get declared or inferred type
            Type* declared_type = NULL;
            if (decl->type_annotation) {
                declared_type = parse_type_annotation(decl->type_annotation);
            }
            
            // Infer from initializer
            Type* init_type = infer_expression(decl->init_expr, ctx);
            
            // Unify types
            if (declared_type && init_type) {
                if (!unify(declared_type, init_type, ctx)) {
                    error("Type mismatch in variable %s: declared %s, got %s",
                        decl->name,
                        type_to_string(declared_type),
                        type_to_string(init_type));
                    return false;
                }
            }
            
            // Bind the variable
            Type* final_type = declared_type ? declared_type : init_type;
            table_set(&ctx->bindings, decl->name, final_type);
            break;
        }
        
        case STMT_FUNCTION: {
            // Function signature type checking
            // Verify all parameter types
            // Verify return type matches all returns
            break;
        }
        
        case STMT_IF:
        case STMT_WHILE: {
            // Check condition is bool
            Expr* condition = stmt->as_if.condition;
            Type* cond_type = infer_expression(condition, ctx);
            
            if (cond_type->base != TYPE_BOOL && ctx->is_strict) {
                error("Condition must be bool, got %s", type_to_string(cond_type));
                return false;
            }
            break;
        }
        
        case STMT_RETURN: {
            // Check return type matches function context
            Type* return_type = infer_expression(stmt->as_return.value, ctx);
            // Compare against function's declared return type
            break;
        }
        
        default:
            break;
    }
    
    return true;
}

bool check_expression(Expr* expr, TypeContext* ctx, Type* expected_type) {
    Type* actual_type = infer_expression(expr, ctx);
    
    if (!unify(actual_type, expected_type, ctx)) {
        error("Type mismatch: expected %s, got %s",
            type_to_string(expected_type),
            type_to_string(actual_type));
        return false;
    }
    
    return true;
}
```

---

## Type Annotations (Parser)

```c
// Parse type annotations in syntax
// Examples: x: int, fn(a: int, b: str) -> bool, let x: [T] = []

Type* parse_type_annotation(const char* annotation) {
    // Simple recursive descent parser for types
    
    // Base types
    if (strcmp(annotation, "int") == 0) return type_create_int();
    if (strcmp(annotation, "float") == 0) return type_create_float();
    if (strcmp(annotation, "bool") == 0) return type_create_bool();
    if (strcmp(annotation, "str") == 0) return type_create_string();
    if (strcmp(annotation, "nothing") == 0) return type_create_nothing();
    
    // Array type: [int], [str], [T]
    if (annotation[0] == '[') {
        const char* end = strchr(annotation, ']');
        if (!end) return NULL;
        
        char elem_type_str[256];
        strncpy(elem_type_str, annotation + 1, end - annotation - 1);
        elem_type_str[end - annotation - 1] = '\0';
        
        Type* elem_type = parse_type_annotation(elem_type_str);
        return type_create_array(elem_type);
    }
    
    // Map type: {int: str}
    if (annotation[0] == '{') {
        // Parse key: value
        const char* colon = strchr(annotation, ':');
        if (!colon) return NULL;
        
        char key_str[128], val_str[128];
        strncpy(key_str, annotation + 1, colon - annotation - 1);
        key_str[colon - annotation - 1] = '\0';
        
        const char* end = strchr(colon, '}');
        strncpy(val_str, colon + 1, end - colon - 1);
        val_str[end - colon - 1] = '\0';
        
        Type* key_type = parse_type_annotation(key_str);
        Type* val_type = parse_type_annotation(val_str);
        return type_create_map(key_type, val_type);
    }
    
    // Optional type: int?
    if (annotation[strlen(annotation)-1] == '?') {
        char base_str[256];
        strncpy(base_str, annotation, strlen(annotation) - 1);
        base_str[strlen(annotation) - 1] = '\0';
        
        Type* base = parse_type_annotation(base_str);
        return type_create_option(base);
    }
    
    return type_create_dynamic();
}
```

---

## Type Display and Errors

```c
// Convert type to human-readable string
char* type_to_string(Type* type) {
    static char buffer[512];
    
    switch (type->base) {
        case TYPE_INT: return "int";
        case TYPE_FLOAT: return "float";
        case TYPE_BOOL: return "bool";
        case TYPE_STRING: return "str";
        case TYPE_NOTHING: return "nothing";
        case TYPE_DYNAMIC: return "dynamic";
        
        case TYPE_ARRAY: {
            char elem_str[256];
            strcpy(elem_str, type_to_string(type->element_type));
            snprintf(buffer, 512, "[%s]", elem_str);
            return buffer;
        }
        
        case TYPE_MAP: {
            char key_str[128], val_str[128];
            strcpy(key_str, type_to_string(type->key_type));
            strcpy(val_str, type_to_string(type->value_type));
            snprintf(buffer, 512, "{%s: %s}", key_str, val_str);
            return buffer;
        }
        
        case TYPE_FUNCTION: {
            strcpy(buffer, "(");
            for (int i = 0; i < type->param_count; i++) {
                if (i > 0) strcat(buffer, ", ");
                strcat(buffer, type_to_string(type->param_types[i]));
            }
            strcat(buffer, ") -> ");
            strcat(buffer, type_to_string(type->return_type));
            return buffer;
        }
        
        case TYPE_OPTION: {
            snprintf(buffer, 512, "%s?", type_to_string(type->element_type));
            return buffer;
        }
        
        default:
            return "unknown";
    }
}

// Type error with context
void type_error(const char* message, Expr* expr, Type* expected, Type* actual) {
    fprintf(stderr, "TypeError at line %d: %s\n", expr->line, message);
    fprintf(stderr, "  Expected: %s\n", type_to_string(expected));
    fprintf(stderr, "  Got: %s\n", type_to_string(actual));
}
```

---

## Gradual Typing: Dynamic ↔ Static

```c
// Gradual typing allows mixing dynamic and static types
// Key principle: dynamic types accept any value, but strict mode warns

// Example 1: Dynamic variable accepting any type
let x = 42;           // Inferred: int
x = "hello";          // In dynamic mode: fine, x becomes string
                      // In strict mode: error

// Example 2: Annotated type is enforced
let y: int = 42;
y = "hello";          // Error in any mode

// Example 3: Untyped function accepts any arguments
fn process(data) {
    display(data)
}
process(42)
process("hello")      // Both fine

// Example 4: Typed function enforces contract
fn add(a: int, b: int) -> int {
    return a + b
}
add(1, 2)             // OK
add("x", "y")         // Error

void check_gradual_compatibility(Type* declared, Type* actual) {
    // If target is dynamic, anything is OK
    if (declared->base == TYPE_DYNAMIC) return true;
    
    // If source is dynamic, warn in strict mode
    if (actual->base == TYPE_DYNAMIC) {
        if (strict_mode) {
            warning("Dynamic value assigned to typed variable");
        }
        return true;
    }
    
    // Both typed: must unify
    return unify(declared, actual);
}
```

---

## Type Narrowing

```c
// Type narrowing in control flow (improve type inference locally)
void check_if_narrowing(Stmt* if_stmt, TypeContext* ctx) {
    // if (x is T) { ... } narrows x to T in the then branch
    
    // Pattern: is_string(x), is_int(x), etc.
    bool is_type_check = is_type_guard_expression(if_stmt->as_if.condition);
    
    if (is_type_check) {
        Type* narrowed_type = extract_narrowed_type(if_stmt->as_if.condition);
        char* narrowed_var = extract_narrowed_var(if_stmt->as_if.condition);
        
        // Push narrowed type binding
        Type* original = table_get(&ctx->bindings, narrowed_var);
        table_set(&ctx->bindings, narrowed_var, narrowed_type);
        
        // Check then branch
        check_block(if_stmt->as_if.then_branch, ctx);
        
        // Restore original type
        table_set(&ctx->bindings, narrowed_var, original);
        
        // Check else branch with original type
        if (if_stmt->as_if.else_branch) {
            check_block(if_stmt->as_if.else_branch, ctx);
        }
    }
}
```

---

## Type System CLI

```bash
# Type checking modes

# 1. Dynamic (default) - No type checking
$ vex run script.vxm

# 2. Infer - Reports type inference (no errors)
$ vex check script.vxm

# 3. Strict - Full static type checking with annotations
$ vex check --strict script.vxm

# 4. Report  - Full report of inferred types
$ vex check --report script.vxm

# Output example:
# script.vxm:
#   Line 5: x : int (inferred from 42)
#   Line 7: y : str (inferred from "hello")
#   Line 10: fn process(data) : (dynamic) -> dynamic
#   ...
```

---

## Testing Type System

```vex
# tests/test_types.vxm

# Test type inference
let x = 42          # inferred: int
let y = 3.14        # inferred: float
let z = [1, 2, 3]   # inferred: [int]

# Test type annotations
let a: int = 10
let b: str = "hello"
let c: [float] = [1.0, 2.0]

# Test strict mode
fn add(x: int, y: int) -> int {
    return x + y
}

assert_equal(add(1, 2), 3)

# Test type narrowing
let maybe_num: int? = 42
if maybe_num {
    display(maybe_num)
}

# Test union types
fn process(data: str | int) {
    if is_string(data) {
        display("String: " + data)
    } else {
        display("Int: " + data)
    }
}

display "Type system tests passed!"
```

---

## Implementation Checklist

- [ ] Type representation and constructors
- [ ] Type inference engine (literal, variable, binary, unary)
- [ ] Unification algorithm
- [ ] Type annotation parser (`:` syntax)
- [ ] Strict mode type checking
- [ ] Type narrowing in control flow (if/match)
- [ ] Error messages with type context
- [ ] Gradual typing support (dynamic/static mix)
- [ ] Generic types ([T], {K:V})
- [ ] Optional types (T?)
- [ ] Union types (T | U)
- [ ] Function type inference and checking
- [ ] Struct type checking
- [ ] CLI: `vex check`, `vex check --strict`, `vex check --report`
- [ ] Integration with editor (hover type info, go to definition)

---

