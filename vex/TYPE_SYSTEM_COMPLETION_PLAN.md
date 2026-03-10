# Vexium Type System Completion Plan

## Current Status (As of Analysis)

The type system foundation exists in `vex/src/type_system.c` and `vex/src/type_system.h` with:
- ✅ Type representation (Type struct with all TypeKinds)
- ✅ Basic type constructors (int, float, bool, string, nothing, dynamic)
- ✅ Generic type constructors (Array, Map, Option, Function, Tuple, Union)
- ✅ Type context for variable bindings
- ✅ Type equality checking (`type_equals`)
- ✅ Basic inference from literals and simple expressions
- ✅ Type to string conversion
- ✅ Type application (substitution for type variables)

## Missing Components (Critical for v2.0)

### 1. Core Hindley-Milner Unification Engine

#### 1.1 Occurs Check (CRITICAL)
**Location**: `vex/src/type_system.c`
**Function Needed**: `type_occurs_in(Type* var, Type* type)`

```c
/* Check if type variable occurs in type (prevents infinite types) */
bool type_occurs_in(Type* var, Type* type) {
    if (!var || !type) return false;
    if (type->kind == TYPE_VARIABLE) {
        return type->id == var->id;
    }
    /* Check recursively in composite types */
    switch (type->kind) {
        case TYPE_ARRAY:
        case TYPE_OPTION:
            return type_occurs_in(var, type->element_type);
        case TYPE_MAP:
            return type_occurs_in(var, type->key_type) || 
                   type_occurs_in(var, type->value_type);
        case TYPE_FUNCTION:
            for (int i = 0; i < type->param_count; i++) {
                if (type_occurs_in(var, type->param_types[i])) return true;
            }
            return type_occurs_in(var, type->return_type);
        /* ... other composite types */
        default:
            return false;
    }
}
```

#### 1.2 Type Unification (CRITICAL)
**Location**: `vex/src/type_system.c`
**Function Needed**: `type_unify(Type* a, Type* b)`

```c
/* Unify two types, updating type variable bindings.
 * Returns true if unification succeeds, false on conflict.
 */
bool type_unify(Type* a, Type* b) {
    /* Dereference any bound type variables */
    while (a->kind == TYPE_VARIABLE && a->binding) a = a->binding;
    while (b->kind == TYPE_VARIABLE && b->binding) b = b->binding;
    
    if (type_equals(a, b)) return true;
    
    /* Variable binding */
    if (a->kind == TYPE_VARIABLE) {
        if (type_occurs_in(a, b)) return false; /* Occurs check fails */
        a->binding = b;
        return true;
    }
    if (b->kind == TYPE_VARIABLE) {
        if (type_occurs_in(b, a)) return false; /* Occurs check fails */
        b->binding = a;
        return true;
    }
    
    /* Same kind required for unification */
    if (a->kind != b->kind) return false;
    
    /* Unify component types */
    switch (a->kind) {
        case TYPE_ARRAY:
        case TYPE_OPTION:
            return type_unify(a->element_type, b->element_type);
            
        case TYPE_MAP:
            return type_unify(a->key_type, b->key_type) &&
                   type_unify(a->value_type, b->value_type);
                   
        case TYPE_FUNCTION:
            if (a->param_count != b->param_count) return false;
            for (int i = 0; i < a->param_count; i++) {
                if (!type_unify(a->param_types[i], b->param_types[i])) 
                    return false;
            }
            return type_unify(a->return_type, b->return_type);
            
        /* ... other composite types */
        default:
            return true;
    }
}
```

### 2. Type Parser for Annotations

**Location**: New file `vex/src/type_parser.c` or extend parser

The parser currently stores type annotations as strings (`type_name`). Need to convert these to Type structures.

#### 2.1 Type Name Parser
```c
/* Parse a type name string into a Type structure.
 * Handles: int, str, bool, float, nothing, dynamic
 *           Array<T>, Map<K,V>, T?, (T1, T2, T3)
 *           fn(T1, T2) -> R
 */
Type* type_parse_string(const char* type_str, TypeContext* ctx) {
    /* Parse simple types */
    if (strcmp(type_str, "int") == 0) return TYPE_INT_SINGLETON;
    if (strcmp(type_str, "str") == 0) return TYPE_STRING_SINGLETON;
    if (strcmp(type_str, "bool") == 0) return TYPE_BOOL_SINGLETON;
    if (strcmp(type_str, "float") == 0) return TYPE_FLOAT_SINGLETON;
    if (strcmp(type_str, "nothing") == 0) return TYPE_NOTHING_SINGLETON;
    if (strcmp(type_str, "dynamic") == 0) return TYPE_DYNAMIC_SINGLETON;
    
    /* Parse generic types: Array<int>, Map<str, int> */
    char* bracket = strchr(type_str, '<');
    if (bracket) {
        char base[64];
        strncpy(base, type_str, bracket - type_str);
        base[bracket - type_str] = '\0';
        
        if (strcmp(base, "Array") == 0) {
            Type* elem = type_parse_simple(bracket + 1, ctx);
            return type_create_array(elem);
        }
        /* ... Map, Option, etc. */
    }
    
    /* Parse optional types: int? */
    char* question = strchr(type_str, '?');
    if (question && question[1] == '\0') {
        char inner[64];
        strncpy(inner, type_str, question - type_str);
        inner[question - type_str] = '\0';
        Type* inner_type = type_parse_simple(inner, ctx);
        return type_create_option(inner_type);
    }
    
    /* Create type variable for unknown names (generics) */
    return type_fresh_variable(ctx, type_str);
}
```

### 3. Enhanced Type Inference

#### 3.1 Function Type Inference
```c
Type* infer_function(ASTNode* node, TypeContext* ctx) {
    /* Create fresh type variables for parameters */
    Type** param_types = NULL;
    int param_count = node->as.fn_decl.params.count;
    
    if (param_count > 0) {
        param_types = malloc(sizeof(Type*) * param_count);
        for (int i = 0; i < param_count; i++) {
            Param* p = &node->as.fn_decl.params.items[i];
            if (p->type_name) {
                /* Use explicit annotation */
                param_types[i] = type_parse_string(p->type_name, ctx);
            } else {
                /* Create fresh type variable */
                param_types[i] = type_fresh_variable(ctx, "t");
            }
            /* Bind parameter name to its type in context */
            type_context_bind(ctx, p->name, param_types[i]);
        }
    }
    
    /* Infer or use explicit return type */
    Type* return_type;
    if (node->as.fn_decl.return_type) {
        return_type = type_parse_string(node->as.fn_decl.return_type, ctx);
    } else {
        return_type = type_fresh_variable(ctx, "r");
    }
    
    /* Infer body type and unify with return type */
    Type* body_type = infer_from_node(node->as.fn_decl.body, ctx);
    type_unify(body_type, return_type);
    
    return type_create_function(param_types, param_count, 
                                 type_apply(return_type, ctx));
}
```

#### 3.2 Call Expression Inference
```c
Type* infer_call(ASTNode* node, TypeContext* ctx) {
    Type* callee_type = infer_from_node(node->as.call.callee, ctx);
    
    /* Create fresh type variable for result */
    Type* result_type = type_fresh_variable(ctx, "call");
    
    /* Build expected function type from arguments */
    int arg_count = node->as.call.args.count;
    Type** arg_types = NULL;
    if (arg_count > 0) {
        arg_types = malloc(sizeof(Type*) * arg_count);
        for (int i = 0; i < arg_count; i++) {
            arg_types[i] = infer_from_node(node->as.call.args.items[i], ctx);
        }
    }
    
    /* Expected function type: (arg_types) -> result_type */
    Type* expected = type_create_function(arg_types, arg_count, result_type);
    
    /* Unify callee with expected function type */
    if (!type_unify(callee_type, expected)) {
        ctx->had_error = true;
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Type mismatch in function call");
    }
    
    /* Return the unified result type */
    return type_apply(result_type, ctx);
}
```

### 4. Type Checker Integration with Compiler

**Location**: `vex/src/compiler.c`

Add type checking pass before code generation:

```c
/* In compile() or compile_node() */
static bool type_check_node(Compiler* c, ASTNode* node, TypeContext* ctx) {
    Type* inferred = infer_from_node(node, ctx);
    
    /* In strict mode, all types must be concrete */
    if (ctx->strict_mode && !type_is_concrete(inferred)) {
        char* type_str = type_to_string(inferred);
        compile_error(c, node->line, 
                      "Could not infer concrete type: %s", type_str);
        free(type_str);
        return false;
    }
    
    return !ctx->had_error;
}

/* Top-level type check */
bool compile_type_check(Compiler* c, ASTNode* program, bool strict) {
    TypeContext ctx;
    type_context_init(&ctx, strict);
    
    /* Bind built-in functions */
    type_context_bind(&ctx, "display", 
                      type_create_function(NULL, 0, TYPE_NOTHING_SINGLETON));
    /* ... more builtins */
    
    bool ok = type_check_node(c, program, &ctx);
    
    type_context_free(&ctx);
    return ok;
}
```

### 5. CLI Integration

**Location**: `vex/src/main.c`

Add `vex check` command:

```c
case COMMAND_CHECK:
    /* Parse and type check without executing */
    Parser parser;
    parser_init(&parser, source);
    ASTNode* ast = parser_parse(&parser);
    
    if (parser.had_error) {
        fprintf(stderr, "[vex check] Parse errors found\n");
        return 1;
    }
    
    Compiler compiler;
    compile_init(&compiler);
    
    bool strict = flag_strict; /* --strict flag */
    if (!compile_type_check(&compiler, ast, strict)) {
        fprintf(stderr, "[vex check] Type errors found\n");
        return 1;
    }
    
    printf("[vex check] No type errors found.\n");
    if (strict) {
        printf("[vex check] Strict mode: All types verified.\n");
    }
    return 0;
```

## Implementation Priority

### Phase 2A: Core Unification (Week 1)
1. Implement `type_occurs_in()`
2. Implement `type_unify()`
3. Add tests for unification
4. Verify no regressions in existing code

### Phase 2B: Type Parser (Week 1-2)
1. Create `type_parse_string()`
2. Handle all type syntax from v2 spec
3. Add comprehensive parser tests

### Phase 2C: Inference Enhancement (Week 2-3)
1. Complete `infer_call()`
2. Implement `infer_function()`
3. Handle all AST node types
4. Add inference tests

### Phase 2D: Compiler Integration (Week 3)
1. Add type check pass to compiler
2. Implement `vex check` command
3. Add `--strict` flag support
4. Integrate type errors with error reporting

### Phase 2E: Generic Types (Week 4)
1. Implement generic type instantiation
2. Add type constraints (where clauses)
3. Test with Array<T>, Map<K,V>
4. Document generic type usage

## Test Plan

```vex
# test_type_basic.vxm - Basic type annotations
fn add(x: int, y: int) -> int:
    give back x + y

let name: str be "Vexium"
let arr: Array<int> be [1, 2, 3]
let maybe: int? be nothing

# test_type_inference.vxm - Inference
fn identity(x):  # Should infer: fn(T) -> T
    give back x

let a be identity(42)      # a: int
let b be identity("hi")    # b: str

# test_type_generic.vxm - Generics
fn first<T>(arr: Array<T>) -> T:
    give back arr[0]

# test_type_strict.vxm - Strict mode (should fail)
fn bad(x):  # No annotation, can't infer in strict mode
    give back x + 1
```

## Success Criteria

- [ ] `type_unify` passes all test cases
- [ ] Type parser handles all v2 syntax
- [ ] `vex check` works on all example files
- [ ] `--strict` mode catches all untyped code
- [ ] No performance regression in VM
- [ ] All existing tests still pass
