# Module System Implementation Guide

> **For developers implementing the Vex v2 module system and package management**

---

## Overview

Vex v2 module system enables:
- **File-based modules**: Each `.vxm` file is a module
- **Import statements**: `use module_name` loads external code
- **Symbol visibility**: Public (default) vs private (underscore prefix)
- **Circular dependency detection**
- **Lazy loading**: Modules load only when imported
- **Namespace isolation**: Module code runs in isolated scope

---

## Module Structure

### File Organization

```
project/
  main.vxm          # Entry point, imports from lib/
  lib/
    math.vxm        # math module
    string.vxm      # string module
    io.vxm          # input/output
  utils/
    helpers.vxm     # utility functions
    constants.vxm   # constants
```

### Module syntax

```vex
# lib/math.vxm

# Public constants and functions
PI = 3.14159

fn quadratic(a: float, b: float, c: float, x: float) -> float {
    return a * x**2 + b * x + c
}

# Private functions (start with underscore)
fn _validate_coeff(x: float) -> bool {
    return x != 0
}

# Public struct
struct Point {
    x: float
    y: float
}
```

### Using modules

```vex
# main.vxm

# Import entire module
use lib.math

# Import specific symbol
use lib.math.quadratic

# Import with alias
use lib.string as str

# Access members
let p = lib.math.Point(1.0, 2.0)
let y = lib.math.quadratic(1, 2, 3, 4)
let num = str.to_integer("42")
```

---

## Module AST Node

```c
typedef struct {
    char* module_name;      // e.g., "lib.math" or "string"
    char* import_path;      // Resolved file path
    char** symbols;         // Specific imports (NULL = all)
    int symbol_count;
    char* alias;            // Alias name (optional)
} UseStatement;

typedef enum {
    IMPORT_TYPE_MODULE,     // use lib.math
    IMPORT_TYPE_SYMBOL,     // use lib.math.PI
    IMPORT_TYPE_WILDCARD,   // use lib.math.*
} ImportType;
```

---

## Module Loader Implementation

```c
typedef struct ModuleCache {
    Table<string, Module*> modules;         // Loaded modules
    StringArray loading_stack;              // Detect cycles
} ModuleCache;

typedef struct {
    char* filename;                         // Source file path
    AstProgram* ast;                        // Parsed AST
    Table<string, Value> public_symbols;    // Public exports
    bool is_loaded;
} Module;

// Module loading
Module* module_load(const char* module_name, ModuleCache* cache) {
    // 1. Check cache
    Module* cached = table_get(&cache->modules, module_name);
    if (cached) {
        return cached;
    }
    
    // 2. Detect circular dependencies
    for (int i = 0; i < cache->loading_stack.count; i++) {
        if (strcmp(cache->loading_stack.strings[i], module_name) == 0) {
            error("Circular module dependency: %s", module_name);
            return NULL;
        }
    }
    
    // 3. Resolve module file path
    char* file_path = module_resolve_path(module_name);
    if (!file_path) {
        error("Module not found: %s", module_name);
        return NULL;
    }
    
    // 4. Read and parse module source
    char* source = read_file(file_path);
    if (!source) {
        error("Cannot read module file: %s", file_path);
        return NULL;
    }
    
    Lexer lexer = lexer_init(source);
    Parser parser = parser_init(lexer);
    AstProgram* ast = parser_parse(&parser);
    
    if (parser.error_count > 0) {
        error("Module parse error in %s", module_name);
        return NULL;
    }
    
    // 5. Create module
    Module* module = malloc(sizeof(Module));
    module->filename = strdup(file_path);
    module->ast = ast;
    module->is_loaded = false;
    
    // 6. Cache before executing (prevents re-loading during recursive imports)
    table_set(&cache->modules, module_name, module);
    
    // 7. Push onto loading stack
    string_array_push(&cache->loading_stack, module_name);
    
    // 8. Execute module to extract public symbols
    module_execute(module, cache);
    
    // 9. Pop from loading stack
    string_array_pop(&cache->loading_stack);
    
    module->is_loaded = true;
    return module;
}

// Path resolution: "lib.math" → "lib/math.vxm"
char* module_resolve_path(const char* module_name) {
    // Replace dots with path separators
    char* path = malloc(strlen(module_name) + 10);
    strcpy(path, "");
    
    char temp[256];
    strcpy(temp, module_name);
    
    char* token = strtok(temp, ".");
    while (token) {
        strcat(path, token);
        token = strtok(NULL, ".");
        if (token) strcat(path, "/");
    }
    
    strcat(path, ".vxm");
    
    // Check if file exists
    if (file_exists(path)) {
        return path;
    }
    
    free(path);
    return NULL;
}

// Execute module code and collect public symbols
void module_execute(Module* module, ModuleCache* cache) {
    // Create isolated scope for module execution
    Scope* module_scope = scope_create();
    
    // Execute all statements
    for (int i = 0; i < module->ast->stmt_count; i++) {
        Stmt* stmt = module->ast->stmts[i];
        
        // Execute statement (function defs, variable decls, etc.)
        interpret_statement(stmt, module_scope);
    }
    
    // Extract public symbols (those not starting with underscore)
    for (int i = 0; i < module_scope->bindings.capacity; i++) {
        TableEntry* entry = &module_scope->bindings.entries[i];
        if (entry->key && entry->key[0] != '_') {
            // Public symbol
            table_set(&module->public_symbols, entry->key, entry->value);
        }
    }
}

// Retrieve public symbol from module
Value module_get_symbol(Module* module, const char* symbol) {
    Value* result = table_get(&module->public_symbols, symbol);
    if (!result) {
        error("Module %s does not export symbol: %s", module->filename, symbol);
        return VALUE_NOTHING();
    }
    return *result;
}
```

---

## Import Resolution in Compiler

```c
// Compile use statement
void compile_use_statement(Compiler* compiler, UseStatement* use_stmt) {
    // 1. Load module
    Module* module = module_load(use_stmt->module_name, &compiler->module_cache);
    if (!module) {
        compiler_error(compiler, "Failed to load module: %s", use_stmt->module_name);
        return;
    }
    
    // 2. Add module to namespace
    if (use_stmt->symbols == NULL) {
        // Import entire module with optional alias
        const char* namespace = use_stmt->alias ? use_stmt->alias : use_stmt->module_name;
        
        // Create module object in current scope
        for (int i = 0; i < module->public_symbols.capacity; i++) {
            TableEntry* entry = &module->public_symbols.entries[i];
            if (entry->key) {
                char qualified_name[256];
                snprintf(qualified_name, 256, "%s.%s", namespace, entry->key);
                
                scope_define(compiler->scope, qualified_name, entry->value);
            }
        }
    } else {
        // Import specific symbols
        for (int i = 0; i < use_stmt->symbol_count; i++) {
            Value symbol = module_get_symbol(module, use_stmt->symbols[i]);
            
            const char* binding_name = use_stmt->symbols[i];
            if (use_stmt->alias) {
                // "use lib.math.quadratic as quad"
                binding_name = use_stmt->alias;
            }
            
            scope_define(compiler->scope, binding_name, symbol);
        }
    }
}
```

---

## Symbol Table Management

```c
typedef struct {
    Table<string, Value> bindings;          // name → value
    struct Scope* parent;                   // Enclosing scope
    int depth;                              // Nesting level (0 = global)
} Scope;

// Scope stack for nested functions/blocks
Scope* scope_create(void) {
    Scope* scope = malloc(sizeof(Scope));
    table_init(&scope->bindings);
    scope->parent = NULL;
    scope->depth = 0;
    return scope;
}

Scope* scope_push(Scope* parent) {
    Scope* child = scope_create();
    child->parent = parent;
    child->depth = parent->depth + 1;
    return child;
}

void scope_pop(Scope* scope) {
    table_free(&scope->bindings);
    free(scope);
}

// Define variable in current scope
void scope_define(Scope* scope, const char* name, Value value) {
    table_set(&scope->bindings, name, value);
}

// Look up variable (search up scope chain)
Value* scope_get(Scope* scope, const char* name) {
    while (scope) {
        Value* result = table_get(&scope->bindings, name);
        if (result) return result;
        scope = scope->parent;
    }
    return NULL;  // Not found
}

// Set variable (search up scope chain, error if not found)
bool scope_set(Scope* scope, const char* name, Value value) {
    while (scope) {
        if (table_contains(&scope->bindings, name)) {
            table_set(&scope->bindings, name, value);
            return true;
        }
        scope = scope->parent;
    }
    return false;  // Not found
}
```

---

## Namespace Syntax

```vex
# Using modules as namespaces

use lib.math
use lib.string
use lib.io

# Access via dot notation
let result = lib.math.quadratic(1, 2, 3, 4)
let text = lib.string.upper("hello")
lib.io.display(result)

# OR import specific symbols
use lib.math.quadratic
use lib.string.upper
use lib.io.display

# Now use directly
let result = quadratic(1, 2, 3, 4)
let text = upper("hello")
display(result)

# OR with aliases
use lib.math as m
use lib.string.upper as uppercase

let result = m.quadratic(1, 2, 3, 4)
let text = uppercase("hello")
```

---

## Module Caching and Reloading

```c
// Single module instance cache
typedef struct {
    Table<string, Module*> loaded;          // Already loaded modules
    StringArray loading;                    // Currently loading (cycle detection)
} ModuleCache;

// Prevent re-parsing/re-executing same module
Module* module_get_cached(const char* name, ModuleCache* cache) {
    return table_get(&cache->loaded, name);
}

// Invalidate module (for REPL or hot reload)
void module_reload(const char* name, ModuleCache* cache) {
    Module* module = table_get(&cache->loaded, name);
    if (module) {
        // Re-parse and re-execute
        module_execute(module, cache);
    }
}
```

---

## Standard Library Modules

Builtin modules (pre-loaded, no file):

```c
// Register builtin modules
void register_builtin_modules(ModuleCache* cache) {
    // math module
    Module* math_mod = malloc(sizeof(Module));
    math_mod->filename = "<builtin:math>";
    math_mod->is_loaded = true;
    
    // Add public symbols
    scope_define_native(&math_mod->public_symbols, "PI", native_math_pi);
    scope_define_native(&math_mod->public_symbols, "E", native_math_e);
    scope_define_native(&math_mod->public_symbols, "sqrt", native_sqrt);
    scope_define_native(&math_mod->public_symbols, "sin", native_sin);
    // ... etc
    
    table_set(&cache->modules, "math", math_mod);
    
    // string module
    Module* string_mod = malloc(sizeof(Module));
    string_mod->filename = "<builtin:string>";
    string_mod->is_loaded = true;
    
    scope_define_native(&string_mod->public_symbols, "upper", native_upper);
    scope_define_native(&string_mod->public_symbols, "lower", native_lower);
    scope_define_native(&string_mod->public_symbols, "length", native_length);
    // ... etc
    
    table_set(&cache->modules, "string", string_mod);
    
    // ... other builtin modules
}
```

---

## Module Testing

```vex
# tests/test_modules.vxm

use lib.math
use lib.string

# Test module imports work
assert_equal(lib.math.PI > 3.1, true)
assert_equal(lib.string.upper("hello"), "HELLO")

# Test circular dependency detection
# (should error with clear message)

# Test private symbols not exported
# (accessing lib.math._validate should fail)

display "All module tests passed!"
```

---

## Developer Workflow with Modules

### Writing Reusable Libraries

```vex
# lib/vector.vxm - Reusable vector math

struct Vector {
    x: float
    y: float
    z: float
}

fn magnitude(v: Vector) -> float {
    return sqrt(v.x**2 + v.y**2 + v.z**2)
}

fn dot_product(a: Vector, b: Vector) -> float {
    return a.x * b.x + a.y * b.y + a.z * b.z
}

fn cross_product(a: Vector, b: Vector) -> Vector {
    return Vector(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    )
}

# Private helper (not exported)
fn _normalize_checked(v: Vector) -> Vector? {
    let mag = magnitude(v)
    if mag == 0 {
        return nothing
    }
    return Vector(v.x / mag, v.y / mag, v.z / mag)
}

# Public API
fn normalize(v: Vector) -> Vector {
    return _normalize_checked(v) || Vector(0, 0, 0)
}
```

### Using in Application

```vex
# main.vxm

use lib.vector

let v1 = vector.Vector(1, 2, 3)
let v2 = vector.Vector(4, 5, 6)

let mag = vector.magnitude(v1)
let dot = vector.dot_product(v1, v2)
let cross = vector.cross_product(v1, v2)

display("Magnitude: " + dot)
```

---

## Implementation Checklist

- [ ] Use statement parsing (use, import keywords)
- [ ] Module path resolution (dots to file paths)
- [ ] Module file loading and caching
- [ ] Module code execution and symbol extraction
- [ ] Public/private symbol filtering (underscore convention)
- [ ] Circular dependency detection
- [ ] Symbol table management across scopes
- [ ] Namespace isolation (module-level scope)
- [ ] Scope chain lookup (local → module → builtin)
- [ ] Builtin module registration (math, string, io, etc.)
- [ ] Alias support (use ... as ...)
- [ ] Module reloading (REPL support)
- [ ] Error messages for missing modules/symbols
- [ ] Documentation of module structure

---

## Best Practices for Module Design

### Module Organization

```
math/
  __init__.vxm          # Exports: PI, E, sin, cos, tan...
  vectors.vxm           # Exports: Vector struct, operations
  matrices.vxm          # Exports: Matrix struct, operations

# In __init__.vxm:
use math.vectors
use math.matrices

# So user can: use math  (imports everything)
```

### Symbol Naming

- Public: `CamelCase` for structs, `snake_case` for functions/variables
- Private: `_leading_underscore` for internal helpers
- Avoid: Single-letter names except loop counters

### Documentation

```vex
# math/vectors.vxm

# Vector3 mathematical operations
# Public types and functions:
#   - struct Vector(x: float, y: float, z: float)
#   - fn magnitude(v: Vector) -> float
#   - fn dot_product(a, b: Vector) -> float
#   - fn cross_product(a, b: Vector) -> Vector
```

---

