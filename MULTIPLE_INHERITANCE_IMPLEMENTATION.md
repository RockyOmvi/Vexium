# Multiple Inheritance Implementation Guide

Developer guide for implementing multiple inheritance in Vexium.

---

## Overview

This document provides detailed implementation guidance for adding multiple inheritance support to Vexium's VM and runtime.

**Status:** Pre-implementation planning  
**Target Version:** v2.0  
**Priority:** High (Critical for AI/ML adoption)

---

## Architecture Changes

### Phase 1: AST & Parser Changes

#### Current Structure (src/ast.h)

```c
struct {
    char* name;
    char* parent_name;           /* Single parent */
    StructField* fields;
    int field_count;
    int field_capacity;
    StructMethod* methods;
    int method_count;
    int method_capacity;
} struct_decl;
```

#### New Structure

```c
struct {
    char* name;
    char** parent_names;         /* Multiple parents */
    int parent_count;
    StructField* fields;
    int field_count;
    int field_capacity;
    StructMethod* methods;
    int method_count;
    int method_capacity;
    
    /* MRO (Method Resolution Order) - cached */
    StructDef** mro_list;
    int mro_count;
    int mro_computed;            /* Flag: whether MRO calculated */
} struct_decl;
```

### Phase 2: Parser Implementation (src/parser.c)

#### Current Implementation

```c
static ASTNode* parse_struct(Parser* p) {
    Token name = advance(p);
    char* struct_name = token_text(name);
    
    /* Optional parent */
    char* parent_name = NULL;
    if (match_token(p, TOKEN_EXTENDS)) {
        Token parent = advance(p);
        parent_name = token_text(parent);
    }
    
    /* Parse fields and methods */
    ...
    
    node->as.struct_decl.parent_name = parent_name;
    return node;
}
```

#### New Implementation

```c
static ASTNode* parse_struct(Parser* p) {
    Token name = advance(p);
    char* struct_name = token_text(name);
    
    /* Parse parent list */
    char** parent_names = NULL;
    int parent_count = 0;
    int parent_capacity = 10;
    
    if (match_token(p, TOKEN_EXTENDS)) {
        parent_names = malloc(parent_capacity * sizeof(char*));
        
        do {
            Token parent = advance(p);
            
            /* Validate parent name */
            if (!is_valid_identifier(parent.lexeme)) {
                error(p, "Invalid parent struct name");
                break;
            }
            
            /* Add to list */
            if (parent_count >= parent_capacity) {
                parent_capacity *= 2;
                parent_names = realloc(parent_names, parent_capacity * sizeof(char*));
            }
            
            parent_names[parent_count++] = token_text(parent);
            
        } while (match_token(p, TOKEN_COMMA));
    }
    
    /* Parse fields and methods */
    ...
    
    node->as.struct_decl.parent_names = parent_names;
    node->as.struct_decl.parent_count = parent_count;
    return node;
}
```

#### New Validation Function

```c
bool validate_inheritance(Parser* p, ASTNode* struct_node) {
    const char* struct_name = struct_node->as.struct_decl.name;
    int parent_count = struct_node->as.struct_decl.parent_count;
    char** parent_names = struct_node->as.struct_decl.parent_names;
    
    /* Check 1: No circular inheritance */
    if (is_parent_of(struct_name, struct_name, p->defined_structs)) {
        error(p, "Circular inheritance: struct cannot inherit from itself");
        return false;
    }
    
    /* Check 2: All parents must be defined */
    for (int i = 0; i < parent_count; i++) {
        if (!is_struct_defined(parent_names[i], p->defined_structs)) {
            error(p, "Parent struct '%s' not defined", parent_names[i]);
            return false;
        }
    }
    
    /* Check 3: No duplicate parents */
    for (int i = 0; i < parent_count; i++) {
        for (int j = i + 1; j < parent_count; j++) {
            if (strcmp(parent_names[i], parent_names[j]) == 0) {
                error(p, "Duplicate parent struct: %s", parent_names[i]);
                return false;
            }
        }
    }
    
    /* Check 4: Diamond inheritance (warns but allows) */
    if (has_diamond_inheritance(struct_node, p->defined_structs)) {
        debug_warn("Diamond inheritance detected in %s (using C3 linearization)", 
                   struct_name);
    }
    
    return true;
}
```

---

### Phase 3: Interpreter Changes (src/interpreter.c)

#### Method Resolution Order (MRO) Algorithm

Implement C3 linearization (same as Python):

```c
/**
 * Compute Method Resolution Order using C3 Linearization
 * 
 * Algorithm:
 * 1. L[C] = C + merge(L[B1], L[B2], ..., L[Bn], [B1, B2, ..., Bn])
 * 2. Take first class from first list that doesn't appear in tail of other lists
 * 3. Remove from all lists and repeat
 * 4. Handle diamond problem correctly
 */
typedef struct {
    StructDef** classes;
    int count;
} MROList;

/**
 * Build MRO for a struct and all ancestors
 */
bool build_mro(StructDef* struct_def, Interpreter* interp) {
    if (struct_def->mro_computed) {
        return true;  /* Already computed */
    }
    
    int mro_count = 0;
    StructDef** mro_list = malloc(100 * sizeof(StructDef*));
    
    /* Add self */
    mro_list[mro_count++] = struct_def;
    
    /* Recursively build parent MROs */
    MROList parent_lists[struct_def->parent_count];
    int merged_offset = 0;
    
    for (int i = 0; i < struct_def->parent_count; i++) {
        StructDef* parent = find_struct_def(struct_def->parent_names[i], interp);
        
        if (!parent) {
            error("Parent struct '%s' not found", struct_def->parent_names[i]);
            free(mro_list);
            return false;
        }
        
        /* Recursively compute parent's MRO */
        if (!build_mro(parent, interp)) {
            free(mro_list);
            return false;
        }
        
        /* Add parent's MRO to lists */
        parent_lists[i].classes = parent->mro_list;
        parent_lists[i].count = parent->mro_count;
    }
    
    /* Merge lists: parents + parent list */
    while (true) {
        bool found = false;
        
        /* Try each parent list */
        for (int i = 0; i < struct_def->parent_count; i++) {
            if (parent_lists[i].count == 0) continue;
            
            StructDef* candidate = parent_lists[i].classes[0];
            bool in_tail = false;
            
            /* Check if candidate is in tail of any other list */
            for (int j = 0; j < struct_def->parent_count; j++) {
                for (int k = 1; k < parent_lists[j].count; k++) {
                    if (parent_lists[j].classes[k] == candidate) {
                        in_tail = true;
                        break;
                    }
                }
                if (in_tail) break;
            }
            
            /* If not in tail, add to MRO */
            if (!in_tail) {
                mro_list[mro_count++] = candidate;
                found = true;
                
                /* Remove from all lists */
                for (int j = 0; j < struct_def->parent_count; j++) {
                    if (parent_lists[j].count > 0 && 
                        parent_lists[j].classes[0] == candidate) {
                        parent_lists[j].classes++;
                        parent_lists[j].count--;
                    }
                }
                
                break;
            }
        }
        
        if (!found) break;  /* Done */
    }
    
    struct_def->mro_list = mro_list;
    struct_def->mro_count = mro_count;
    struct_def->mro_computed = 1;
    
    return true;
}
```

#### Method Lookup with MRO

```c
/**
 * Find method using MRO
 * Returns first match found in MRO order
 */
StructMethod* find_method_mro(StructDef* struct_def, const char* method_name) {
    if (!struct_def->mro_computed) {
        debug_error("MRO not computed for struct %s", struct_def->name);
        return NULL;
    }
    
    /* Walk MRO list */
    for (int i = 0; i < struct_def->mro_count; i++) {
        StructDef* in_mro = struct_def->mro_list[i];
        
        /* Search in this struct's methods */
        for (int j = 0; j < in_mro->method_count; j++) {
            if (strcmp(in_mro->methods[j].name, method_name) == 0) {
                return &in_mro->methods[j];
            }
        }
    }
    
    return NULL;  /* Not found in entire MRO */
}
```

#### Field Resolution with Conflict Detection

```c
/**
 * Check for field conflicts in multiple inheritance
 * Returns the defining struct, or NULL if ambiguous
 */
StructDef* resolve_field_mro(StructDef* struct_def, const char* field_name) {
    StructDef* defining_struct = NULL;
    
    /* Walk MRO list */
    for (int i = 0; i < struct_def->mro_count; i++) {
        StructDef* in_mro = struct_def->mro_list[i];
        
        /* Check if field is defined in this struct */
        for (int j = 0; j < in_mro->field_count; j++) {
            if (strcmp(in_mro->fields[j].name, field_name) == 0) {
                if (defining_struct != NULL && defining_struct != in_mro) {
                    /* Conflict: field defined in multiple parents */
                    error("Ambiguous field '%s' from multiple parents", field_name);
                    return NULL;
                }
                defining_struct = in_mro;
                break;
            }
        }
    }
    
    return defining_struct;
}
```

#### Support for super.Parent() Syntax

```c
/**
 * Call parent method explicitly
 * Syntax: super.ParentName.method_name(args)
 */
Value call_parent_method(
    StructDef* current_struct,
    const char* parent_name,
    const char* method_name,
    Value* args,
    int arg_count,
    Interpreter* interp
) {
    /* Find parent in MRO */
    StructDef* parent = NULL;
    for (int i = 0; i < current_struct->parent_count; i++) {
        if (strcmp(current_struct->parent_names[i], parent_name) == 0) {
            parent = find_struct_def(parent_name, interp);
            break;
        }
    }
    
    if (!parent) {
        runtime_error("Parent struct '%s' not found", parent_name);
        return MAKE_NULL();
    }
    
    /* Find method in parent */
    StructMethod* method = NULL;
    for (int j = 0; j < parent->method_count; j++) {
        if (strcmp(parent->methods[j].name, method_name) == 0) {
            method = &parent->methods[j];
            break;
        }
    }
    
    if (!method) {
        runtime_error("Method '%s' not found in parent '%s'", 
                      method_name, parent_name);
        return MAKE_NULL();
    }
    
    /* Execute parent method */
    return execute_method(method, args, arg_count, interp);
}
```

---

### Phase 4: Compiler Changes (src/compiler.c)

#### Struct Definition with Multiple Parents

```c
/**
 * Compile struct definition with multiple inheritance
 */
void compile_struct_def(Compiler* comp, ASTNode* struct_node) {
    const char* struct_name = struct_node->as.struct_decl.name;
    char** parent_names = struct_node->as.struct_decl.parent_names;
    int parent_count = struct_node->as.struct_decl.parent_count;
    
    /* Validate inheritance (circular, undefined parents, etc.) */
    if (!validate_inheritance(comp->parser, struct_node)) {
        return;
    }
    
    /* Create struct definition */
    StructDef* struct_def = create_struct_def(struct_name);
    struct_def->parent_names = copy_string_array(parent_names, parent_count);
    struct_def->parent_count = parent_count;
    
    /* Add to registry */
    register_struct(struct_name, struct_def, comp->interp);
    
    /* Emit opcode */
    uint32_t name_idx = add_constant_cstr(comp, struct_name);
    
    emit_byte(comp, OP_STRUCT_DEF);
    emit_uint32(comp, name_idx);
    emit_byte(comp, parent_count);
    
    /* Emit parent names */
    for (int i = 0; i < parent_count; i++) {
        uint32_t parent_idx = add_constant_cstr(comp, parent_names[i]);
        emit_uint32(comp, parent_idx);
    }
    
    /* Compile fields and methods */
    compile_struct_fields(comp, struct_node);
    compile_struct_methods(comp, struct_node);
}
```

---

## Testing Strategy

### Unit Tests

Create `test/test_multiple_inheritance.c`:

```c
#include "minunit.h"
#include "vexium.h"

/* Test 1: Basic multiple inheritance */
MU_TEST(test_basic_multiple_inheritance) {
    const char* code = R"(
        struct A: 
            can foo(): give back "A"
        
        struct B:
            can bar(): give back "B"
        
        struct C extends A, B:
            can baz(): give back "C"
        
        let c be C()
        display c.foo()    # Should work
        display c.bar()    # Should work
        display c.baz()    # Should work
    )";
    
    Value result = execute_vexium(code);
    mu_assert_int_eq(result.type, TYPE_NULL);
    /* Check output contains all three values */
}

/* Test 2: MRO with diamond inheritance */
MU_TEST(test_mro_diamond) {
    const char* code = R"(
        struct A:
            has value
            can init(): self.value be "A"
        
        struct B extends A:
            can b_method(): give back "B"
        
        struct C extends A:
            can c_method(): give back "C"
        
        struct D extends B, C:
            can d_method(): give back "D"
        
        let d be D()
        # Should not have conflicting 'value' field
        display d.value    # "A" - from A, not duplicated
    )";
    
    Value result = execute_vexium(code);
    mu_assert_int_eq(result.type, TYPE_NULL);
}

/* Test 3: Method override with multiple parents */
MU_TEST(test_method_override_multiple) {
    const char* code = R"(
        struct Parent1:
            can speak(): give back "Parent1"
        
        struct Parent2:
            can speak(): give back "Parent2"
        
        struct Child extends Parent1, Parent2:
            can speak(): give back "Child"
        
        let c be Child()
        display c.speak()  # "Child" - child's method takes precedence
    )";
    
    Value result = execute_vexium(code);
    mu_assert_int_eq(result.type, TYPE_NULL);
}

/* Test 4: Explicit parent method calls */
MU_TEST(test_super_parent_call) {
    const char* code = R"(
        struct Parent1:
            can greet(): display "Hello from Parent1"
        
        struct Parent2:
            can greet(): display "Hello from Parent2"
        
        struct Child extends Parent1, Parent2:
            can greet():
                super.Parent1.greet()       # Parent1's version
                super.Parent2.greet()       # Parent2's version
                display "Hello from Child"
        
        let c be Child()
        c.greet()
    )";
    
    Value result = execute_vexium(code);
    mu_assert_int_eq(result.type, TYPE_NULL);
}

/* Test 5: Field inheritance from multiple parents */
MU_TEST(test_field_inheritance_multiple) {
    const char* code = R"(
        struct Parent1:
            has x
        
        struct Parent2:
            has y
        
        struct Child extends Parent1, Parent2:
            has z
        
        let c be Child()
        c.x be 1
        c.y be 2
        c.z be 3
        
        display c.x + c.y + c.z  # 6
    )";
    
    Value result = execute_vexium(code);
    mu_assert_int_eq(result.type, TYPE_NULL);
}

/* Test 6: Error - circular inheritance */
MU_TEST(test_circular_inheritance_error) {
    const char* code = R"(
        struct A extends B
        struct B extends A
    )";
    
    /* Should produce compile error */
    Value result = execute_vexium(code);
    mu_assert_int_eq(result.type, TYPE_ERROR);
}

/* Test 7: Error - undefined parent */
MU_TEST(test_undefined_parent_error) {
    const char* code = R"(
        struct Child extends NonExistent:
            ...
    )";
    
    /* Should produce compile error */
    Value result = execute_vexium(code);
    mu_assert_int_eq(result.type, TYPE_ERROR);
}

MU_TEST_SUITE(multiple_inheritance_suite) {
    MU_RUN_TEST(test_basic_multiple_inheritance);
    MU_RUN_TEST(test_mro_diamond);
    MU_RUN_TEST(test_method_override_multiple);
    MU_RUN_TEST(test_super_parent_call);
    MU_RUN_TEST(test_field_inheritance_multiple);
    MU_RUN_TEST(test_circular_inheritance_error);
    MU_RUN_TEST(test_undefined_parent_error);
}
```

### Integration Tests

Run on real-world patterns:

```vex
# Test: Data Pipeline (AI use case)
struct DataLoader:
    can load(path):
        display "Loading " + path

struct DataValidator:
    can validate(data):
        display "Validating"

struct DataTransformer:
    can transform(data):
        display "Transforming"

struct ETLPipeline extends DataLoader, DataValidator, DataTransformer:
    can execute(path):
        let data be self.load(path)
        self.validate(data)
        let result be self.transform(data)
        give back result

let pipeline be ETLPipeline()
pipeline.execute("data.csv")
```

---

## Performance Considerations

### MRO Caching

```c
/**
 * Cache MRO after first computation
 * Avoid recomputation on every method call
 */
typedef struct {
    StructDef* struct_def;
    StructDef** mro_list;
    int mro_count;
    uint64_t cache_key;  /* Hash of parent configuration */
} MROCache;

/* Global MRO cache */
static MROCache mro_cache[MAX_STRUCTS];
static int mro_cache_count = 0;

StructDef** get_mro_cached(StructDef* struct_def) {
    /* Check cache first */
    for (int i = 0; i < mro_cache_count; i++) {
        if (mro_cache[i].struct_def == struct_def) {
            return mro_cache[i].mro_list;
        }
    }
    
    /* Compute and cache */
    build_mro(struct_def, ...);
    mro_cache[mro_cache_count].struct_def = struct_def;
    mro_cache[mro_cache_count].mro_list = struct_def->mro_list;
    mro_cache[mro_cache_count].mro_count = struct_def->mro_count;
    mro_cache_count++;
    
    return struct_def->mro_list;
}
```

### Method Table Optimization

```c
/**
 * Pre-compute method table from MRO
 * Fast O(1) method lookup after initialization
 */
typedef struct {
    char* method_name;
    StructMethod* method;
    StructDef* defining_struct;
} MethodTableEntry;

typedef struct {
    MethodTableEntry* entries;
    int count;
    int capacity;
} MethodTable;

void build_method_table(StructDef* struct_def) {
    MethodTable table = {0};
    
    /* Walk MRO and add methods to table */
    for (int i = 0; i < struct_def->mro_count; i++) {
        StructDef* in_mro = struct_def->mro_list[i];
        
        for (int j = 0; j < in_mro->method_count; j++) {
            StructMethod* method = &in_mro->methods[j];
            
            /* Skip if already in table (parent overridden) */
            if (!method_in_table(&table, method->name)) {
                add_method_to_table(&table, method->name, method, in_mro);
            }
        }
    }
    
    struct_def->method_table = table.entries;
    struct_def->method_table_count = table.count;
}

StructMethod* find_method_fast(StructDef* struct_def, const char* method_name) {
    /* O(1) hash table lookup */
    for (int i = 0; i < struct_def->method_table_count; i++) {
        if (strcmp(struct_def->method_table[i].method_name, method_name) == 0) {
            return struct_def->method_table[i].method;
        }
    }
    return NULL;
}
```

---

## Migration & Backward Compatibility

### Compatibility Check

```c
/**
 * Ensure existing single-inheritance code still works
 */
void test_backward_compatibility() {
    const char* old_code = R"(
        struct Animal:
            has name
        
        struct Dog extends Animal:
            has breed
        
        let dog be Dog()
        dog.name be "Rex"
        dog.breed be "Labrador"
        display dog.name
    )";
    
    /* Should work unchanged */
    execute_vexium(old_code);
}
```

### Migration Path

1. **Phase 1:** Add AST & parser support (backward compatible)
2. **Phase 2:** Implement MRO algorithm
3. **Phase 3:** Add compiler support
4. **Phase 4:** Testing & optimization
5. **Phase 5:** Release with migration guide

---

## Documentation to Update

### For End Users
1. [VEXIUM_OOP.md](VEXIUM_OOP.md) - Multiple inheritance section
2. [VEXIUM_EXAMPLES.md](VEXIUM_EXAMPLES.md) - Add mixin examples
3. [VEXIUM_SYNTAX.md](VEXIUM_SYNTAX.md) - Update struct syntax
4. [VEXIUM_BEST_PRACTICES.md](VEXIUM_BEST_PRACTICES.md) - When to use multiple inheritance

### For Developers
1. This file (implementation details)
2. Architecture documentation
3. Code comments in source files
4. Test documentation

---

## Deployment Checklist

- [ ] All unit tests passing (95%+ coverage)
- [ ] All integration tests passing
- [ ] Performance benchmarks acceptable
- [ ] Backward compatibility verified
- [ ] Documentation complete and reviewed
- [ ] Code review approved
- [ ] Security audit completed
- [ ] Release notes prepared
- [ ] Migration guide available
- [ ] Beta testing period completed

---

## References

### Similar Implementations
- **Python MRO (C3 Linearization)** - https://docs.python.org/3/howto/mro.html
- **Ruby Method Resolution** - https://ruby-doc.org/
- **Scala Mixin** - https://docs.scala-lang.org/

### Papers & Resources
- C3 Linearization Algorithm
- Multiple Dispatch Patterns
- OOP Design Patterns

---

## Questions & Discussion

For questions or contributions:
1. Review [MULTIPLE_INHERITANCE_PROPOSAL.md](MULTIPLE_INHERITANCE_PROPOSAL.md)
2. Check existing GitHub issues
3. Propose changes in discussions
4. Submit PRs with tests

---

**Document Version:** 1.0  
**Last Updated:** March 4, 2026  
**Status:** Ready for implementation review
