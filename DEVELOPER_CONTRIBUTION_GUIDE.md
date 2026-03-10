# Developer Contribution Guide

> **Guide for contributing to Vex v2 language development**

---

## Quick Start for Contributors

### Prerequisites

- **C compiler**: GCC 9+ or Clang 10+ (C99 standard)
- **Build tool**: Manual gcc compilation (no make on Windows, optional on Linux/Mac)
- **Git**: For version control
- **Editor**: VS Code recommended with C extension

### Setup Development Environment

```bash
# Clone repository
git clone https://github.com/yourusername/vexium.git
cd vexium/vex

# Compile the interpreter
gcc -std=c99 -O2 -o vexium.exe src/*.c -lm

# Run tests
./vexium.exe run examples/test_interpreter.vxm
./vexium.exe run examples/test_vm.vxm
./vexium.exe run examples/test_stdlib.vxm
```

### File Structure

```
vex/
  src/
    main.c            # Entry point and CLI
    lexer.c/h         # Tokenization
    token.c/h         # Token definitions
    parser.c/h        # AST construction
    ast.c/h           # AST node types
    interpreter.c/h   # Tree-walk execution
    compiler.c/h      # Bytecode compilation
    vm.c/h            # Stack-based VM
    stdlib.c/h        # Built-in functions
    module_loader.c/h # Module system
    common.h          # Common utilities
  
  examples/
    test_interpreter.vxm  # Test suite
    test_vm.vxm           # VM tests
    test_stdlib.vxm       # Stdlib tests
    test_phase6.vxm       # Advanced features
    hello.vxm             # Simple example
  
  Makefile           # Build script (Linux/Mac)
  vexium.exe         # Compiled binary (Windows)
```

---

## Implementation Tasks by Priority

### Phase 1: Core Completion (Currently 70% done)

**Status**: Most core features working. Focus on edge cases and polishing.

#### Task 1.1: Complete Bytecode Compiler
**Where**: `src/compiler.c`
**Status**: ~90% complete
**What's left**:
- [ ] Verify all AST node types compile correctly
- [ ] Test edge cases (deeply nested structures, etc.)
- [ ] Optimize constant folding
- [ ] Add jump offset validation

**Implementation guide**: See [BYTECODE_COMPILER_GUIDE.md](BYTECODE_COMPILER_GUIDE.md)

#### Task 1.2: User-Defined Module Loading
**Where**: `src/module_loader.c`
**Status**: ~30% (infrastructure in place, needs execution integration)
**What's left**:
- [ ] Execute module code and populate exports
- [ ] Implement public/private symbol filtering (\_prefix convention)
- [ ] Test circular dependency detection
- [ ] Add module caching validation

**Implementation guide**: See [MODULE_SYSTEM_IMPLEMENTATION.md](MODULE_SYSTEM_IMPLEMENTATION.md)

#### Task 1.3: Standard Library Completion
**Where**: `src/stdlib.c`
**Status**: ~40% complete (math, string, fs working, need json, http, concurrent)
**What's left**:
- [ ] JSON parsing/stringification (json module)
- [ ] HTTP client (http module)
- [ ] Collections: Queue, Stack, Set data structures
- [ ] More string utilities: trim, format, split variants
- [ ] More math: complex numbers, matrix operations

**Implementation guide**: See [STDLIB_IMPLEMENTATION_GUIDE.md](STDLIB_IMPLEMENTATION_GUIDE.md)

---

### Phase 2: Type System (Currently 0% - not started)

**Status**: Parser support exists, needs type checking implementation

#### Task 2.1: Type Annotation Parser
**Where**: `src/parser.c`
**What to do**:
```c
// Add support for type annotations in function parameters and variables
fn add(a: int, b: int) -> int { return a + b }
let x: str = "hello"
let y: [int] = [1, 2, 3]
```

**Implementation guide**: See [TYPE_SYSTEM_IMPLEMENTATION.md](TYPE_SYSTEM_IMPLEMENTATION.md)

#### Task 2.2: Type Inference Engine
**Where**: New file `src/type_checker.c`
**What to do**:
- Implement Hindley-Milner type inference
- Create unification algorithm
- Track variable types through assignments

#### Task 2.3: Strict Mode Type Checker
**Where**: `src/main.c` (add `vex check --strict` command)
**What to do**:
- Validate all types against annotations
- Report mismatches with helpful messages
- Support gradual typing (dynamic + static blend)

---

### Phase 3: Concurrency (Currently 0% - not started)

**Status**: Planned for v2.1, infrastructure not yet implemented

#### Task 3.1: Task Runtime
**Where**: New file `src/scheduler.c`
**What to do**:
- Create thread pool for task execution
- Implement task spawning and joining
- Handle task cancellation

**Implementation guide**: See [CONCURRENCY_IMPLEMENTATION.md](CONCURRENCY_IMPLEMENTATION.md)

#### Task 3.2: Channel Communication
**Where**: New file `src/channel.c`
**What to do**:
- Implement send/recv channels
- Support buffered and unbuffered modes
- Implement select for multiplexing

#### Task 3.3: Synchronization Primitives
**Where**: New file `src/sync.c`
**What to do**:
- Mutex implementation
- Read-write locks
- Condition variables
- Semaphores and barriers

---

## How to Implement a Feature

### Step 1: Define AST Node (if needed)

Add to [src/ast.h](src/ast.h):
```c
typedef enum {
    // ... existing nodes
    NODE_NEW_FEATURE,
} ASTNodeType;

typedef struct {
    // Node-specific fields
} NewFeatureData;
```

Update the union in `ASTNode`:
```c
union {
    // ... existing data
    NewFeatureData new_feature;
};
```

### Step 2: Add Parser Support

In [src/parser.c](src/parser.c):
```c
// Add keyword recognition
if (match_token(parser, TOKEN_NEW_KEYWORD)) {
    return parse_new_feature(parser);
}

// Implement parser
static ASTNode* parse_new_feature(Parser* parser) {
    // Parse the feature's syntax
    ASTNode* node = ast_new_node(NODE_NEW_FEATURE, ...);
    // ... populate fields
    return node;
}
```

### Step 3: Add Interpreter Support (if tree-walk)

In [src/interpreter.c](src/interpreter.c):
```c
case NODE_NEW_FEATURE: {
    // Execute the feature
    // Access feature data: node->as.new_feature.field
    return result_value;
}
```

### Step 4: Add Compiler Support (for bytecode)

In [src/compiler.c](src/compiler.c):
```c
case NODE_NEW_FEATURE: {
    // Compile to bytecode instructions
    compile_new_feature(compiler, node);
    break;
}
```

### Step 5: Add Tests

Create `examples/test_new_feature.vxm`:
```vex
display "=== Testing New Feature ==="

# Test case 1
# ... verify expected output

# Test case 2
# ... more tests

display "=== NEW FEATURE TESTS PASSED ==="
```

Run test:
```bash
./vexium.exe run examples/test_new_feature.vxm
```

### Step 6: Update Documentation

- Update relevant guide file (stdlib, type system, etc.)
- Add examples to [QUICKSTART_V2.md](doc/QUICKSTART_V2.md)
- Document breaking changes if any

---

## Code Style Guidelines

### Naming Conventions

```c
/* Functions: snake_case */
void interpreter_init(Interpreter* interp);
VexValue eval(Interpreter* interp, ASTNode* node);

/* Types: PascalCase */
typedef struct {
    int value;
} MyType;

/* Constants: UPPER_SNAKE_CASE */
#define MAX_STACK_SIZE 1024
#define DEFAULT_CAPACITY 16

/* Private: prefixed with underscore */
static char* _internal_function(void);
```

### Comments

```c
/* ══════════════════════════════════════════════════════════════
 *  MAJOR SECTION HEADER
 * ══════════════════════════════════════════════════════════════ */

/* Small section */

/* Single-line comment explaining what */
int process_data(Data* data);

/* Multi-line comments for complex logic
   - First point
   - Second point
   - Result: explained */
```

### Formatting

- Use 4-space indentation
- Max line length: 100 characters
- One statement per line
- Opening brace on same line as declaration

```c
void function(int param) {
    if (condition) {
        do_something();
    } else {
        do_something_else();
    }
}
```

---

## Testing Strategy

### Unit Tests (In Source)

For simple functions, add inline tests:
```c
/* Test: string concatenation */
char* s1 = "hello";
char* s2 = "world";
char* result = concat_strings(s1, s2);
assert(strcmp(result, "helloworld") == 0);
free(result);
```

### Integration Tests  

Create `.vxm` test files in `examples/`:
```vex
# Test feature X
# Verify output matches expected
```

Run: `./vexium.exe run examples/test_feature_x.vxm`

### Regression Tests

After fixing a bug, add test that would have caught it:
```bash
# Ensure all existing tests still pass after changes
./vexium.exe run examples/test_interpreter.vxm
./vexium.exe run examples/test_vm.vxm
```

---

## Debugging Techniques

### Print Debugging

```c
/* Enable with DEBUG macro */
#ifdef DEBUG
    printf("[DEBUG] Variable value: %d\n", value);
#endif

/* Compile with debug: gcc -DDEBUG ... */
```

### AST Printing

```bash
./vexium.exe ast examples/hello.vxm
```

### Token Debugging

```bash
./vexium.exe lex examples/hello.vxm
```

### Bytecode Disassembly

```bash
./vexium.exe disasm examples/hello.vxm
```

### GDB Debugging

```bash
# Compile with debug symbols
gcc -std=c99 -g -o vexium examples/*.vxm src/*.c -lm

# Run in debugger
gdb ./vexium.exe
(gdb) run examples/hello.vxm
(gdb) break interpreter.c:123
(gdb) continue
(gdb) print variable_name
```

---

## Performance Profiling

### Benchmark Tool

```bash
./vexium.exe bench examples/test_simple_vm.vxm
```

Compares tree-walk vs bytecode VM execution times.

### Hotspot Analysis

For expensive operations:
```c
#include <time.h>

clock_t start = clock();
{ expensive_operation(); }
clock_t end = clock();

double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
printf("Time: %.6f seconds\n", elapsed);
```

### Memory Usage

Track allocations:
```c
#ifdef TRACK_MEMORY
    printf("Allocated: %zu bytes\n", total_allocated);
    printf("Live objects: %d\n", object_count);
#endif
```

---

## Common Pitfalls and Solutions

### Pitfall 1: Memory Leaks

**Problem**: Forgotten `free()` calls
**Solution**: Use VALGRIND (Linux):
```bash
valgrind --leak-check=full ./vexium.exe examples/hello.vxm
```

### Pitfall 2: Uninitialized Variables

**Problem**: Using variables without initialization
**Solution**: Compile with warnings:
```bash
gcc -Wall -Wextra -std=c99 ...
```

### Pitfall 3: Buffer Overflows

**Problem**: Writing past array bounds
**Solution**: Always check sizes:
```c
if (array_index >= array->capacity) {
    resize_array(array);
}
```

### Pitfall 4: Dangling Pointers

**Problem**: Using pointer after freeing
**Solution**: Set to NULL after free:
```c
free(ptr);
ptr = NULL;
```

---

## Build and Test Checklist

Before submitting changes:

- [ ] Code compiles without warnings: `gcc -Wall -Wextra -std=c99 ...`
- [ ] All existing tests pass: `./vexium.exe run examples/test_*.vxm`
- [ ] New feature has tests
- [ ] Code follows style guidelines
- [ ] Comments added for complex logic
- [ ] Memory cleaned up (no leaks)
- [ ] Documentation updated
- [ ] Example added to QUICKSTART

---

## Getting Help

### Resources

1. **[language_v2_spec.md](language_v2_spec.md)** - Language specification
2. **[IMPLEMENTATION_ROADMAP.md](IMPLEMENTATION_ROADMAP.md)** - Development plan
3. **Implementation guides** (see list in README)
4. **Existing code** - Best reference (especially working features)

### Discussion

For questions:
1. Check existing tests for examples
2. Read related implementation guide
3. Examine similar feature implementation
4. Ask in discussions/issues

---

## Submitting Contributions

### Process

1. Fork repository
2. Create feature branch: `git checkout -b feature/my-feature`
3. Make changes and test
4. Commit with clear messages: `git commit -m "Add type system foundation"`
5. Push to fork: `git push origin feature/my-feature`
6. Create pull request with description

### PR Description Template

```markdown
## Description
Brief explanation of the feature/fix

## Changes
- Change 1
- Change 2

## Testing
- [x] New feature has tests
- [x] All existing tests pass
- [ ] Benchmarks show no regression

## Related Issues
Fixes #123
```

---

## Recognition

Contributors are recognized in:
- CONTRIBUTORS.md file
- Release notes
- GitHub contributors list

Thank you for helping build Vex! 🎉

---

