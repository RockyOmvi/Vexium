# Vexium - Surpass Python Implementation Plan

## Overview
This plan outlines all remaining features needed to make Vexium surpass Python in functionality while maintaining performance advantages.

## Current Status
- ✅ VM Core - Stable
- ✅ Bytecode - Working  
- ✅ Parser - Complete
- ✅ Basic Stdlib - Working
- 🟡 Type System - Foundation (40%)
- 🟡 Error Handling - Parsing Only (30%)
- 🟡 Module System - Partial (50%)
- 🔴 Decorators - Not Started
- 🔴 Generators/yield - Not Started
- 🔴 *args/**kwargs - Not Started
- 🔴 f-strings - Not Started
- 🔴 Context Managers - Not Started
- 🔴 Advanced Concurrency - Not Started
- 🔴 AI/ML Features - Not Started

---

## Phase 1: Type System Enhancement (Priority: HIGH)

### 1.1 Core Unification Engine
- [ ] `type_occurs_in()` - Occurs check for type variables
- [ ] `type_unify()` - Hindley-Milner unification algorithm
- [ ] `type_is_subtype()` - Subtyping for gradual typing
- [ ] `type_is_assignable()` - Assignment compatibility

### 1.2 Type Parser
- [ ] `type_parse_string()` - Convert annotation strings to Type structs
- [ ] Parse generics: `Array<T>`, `Map<K,V>`
- [ ] Parse optionals: `int?`, `str?`
- [ ] Parse function types: `fn(int, str) -> bool`
- [ ] Parse tuple types: `(int, str, bool)`
- [ ] Parse union types: `int | str | nothing`

### 1.3 Enhanced Inference
- [ ] Complete `infer_call()` - Function call type inference
- [ ] `infer_function()` - Function declaration inference
- [ ] `infer_if()` - Conditional expression typing
- [ ] `infer_match()` - Pattern matching types
- [ ] `infer_field_access()` - Struct field types

### 1.4 Compiler Integration & CLI
- [ ] Add type check pass before code generation
- [ ] Store inferred types in bytecode
- [ ] Type error reporting with line numbers
- [ ] `vex check <file>` - Type check without execution
- [ ] `vex check --strict` - Require all types concrete
- [ ] `vex check --show-types` - Display inferred types

---

## Phase 2: Error Handling (Priority: HIGH)

### 2.1 Error Types
- [ ] Define error type hierarchy
- [ ] Built-in error types: TypeError, ValueError, RuntimeError, IOError
- [ ] Custom error definitions with `error MyError(msg str):`

### 2.2 Bytecode Opcodes
- [ ] `OP_TRY_BEGIN` - Start protected region
- [ ] `OP_TRY_END` - End protected region
- [ ] `OP_THROW` - Throw error with value
- [ ] `OP_CATCH` - Begin catch handler

### 2.3 VM Implementation
- [ ] Try-catch frame management
- [ ] Stack unwinding on throw
- [ ] Error value propagation
- [ ] Finally block support (defer-style)

### 2.4 Compiler Support
- [ ] Compile `attempt` blocks to bytecode
- [ ] Compile `otherwise` handlers
- [ ] Error variable binding: `attempt: ... otherwise err:`

---

## Phase 3: Advanced Function Features (Priority: HIGH)

### 3.1 Decorators (@decorator)
- [ ] Add `TOKEN_AT` support in parser (already exists!)
- [ ] Parse decorator syntax: `@decorator` before functions/classes
- [ ] AST node for decorators: `NODE_DECORATOR`
- [ ] Interpreter support for decorators
- [ ] Bytecode compilation of decorators

Example:
```python
@log_calls
@timed
fn add(a int, b int) int:
    return a + b
```

### 3.2 Generators (yield)
- [ ] Add `TOKEN_YIELD` token
- [ ] Parse `yield` expression
- [ ] Generator function detection
- [ ] Generator state machine in VM
- [ ] `yield from` for delegation

Example:
```python
fn fibonacci():
    a = 0
    b = 1
    while true:
        yield b
        tmp = a + b
        a = b
        b = tmp
```

### 3.3 Variadic Arguments (*args, **kwargs)
- [ ] Parse `*args` parameter
- [ ] Parse `**kwargs` parameter
- [ ] Pack positional args into array
- [ ] Pack keyword args into map/dict
- [ ] Unpack in function calls: `fn(*args, **kwargs)`

Example:
```python
fn greet(greeting str, *names str):
    for name in names:
        print(greeting + " " + name)

greet("Hello", "Alice", "Bob", "Charlie")
```

---

## Phase 4: Enhanced String Features (Priority: MEDIUM)

### 4.1 f-strings (Formatted String Literals)
- [ ] Add `TOKEN_FSTRING` token
- [ ] Parse f"..." syntax with `{expr}` interpolation
- [ ] Evaluate expressions inside f-strings
- [ ] Format specifiers: `{value:.2f}`, `{value:>10}`

Example:
```python
name = "Alice"
age = 30
print(f"Name: {name}, Age: {age}")
print(f"Pi: {3.14159:.2f}")
```

### 4.2 Raw Strings
- [ ] Add `TOKEN_RSTRING` for r"..."
- [ ] Disable escape sequence processing

### 4.3 Multi-line Strings
- [ ] Triple quote support: `"""..."""`
- [ ] Preserve whitespace/newlines

---

## Phase 5: Context Managers (Priority: MEDIUM)

### 5.1 With Statement
- [ ] Add `TOKEN_WITH` token
- [ ] Parse `with expr as var:` syntax
- [ ] Call `__enter__()` and `__exit__()` protocols
- [ ] Multiple context managers: `with a as x, b as y:`

Example:
```python
with open("file.txt") as f:
    content = f.read()
# f automatically closed
```

### 5.2 Async Context Managers
- [ ] `async with` support
- [ ] `__aenter__` and `__aexit__`

---

## Phase 6: Advanced Concurrency (Priority: MEDIUM)

### 6.1 Async/Await
- [ ] `async fn` declaration
- [ ] `await` expression
- [ ] Async event loop in VM
- [ ] Async file I/O

### 6.2 Task System Enhancement
- [ ] Task priorities
- [ ] Task cancellation
- [ ] Task timeout
- [ ] Task communication channels

### 6.3 Threading (Optional)
- [ ] `thread` module
- `threading.Thread`
- `threading.Lock`
- `threading.Condition`

---

## Phase 7: Enhanced Collections (Priority: MEDIUM)

### 7.1 Advanced List Operations
- [ ] List multiplication: `[1,2] * 3` → `[1,2,1,2,1,2]`
- [ ] List addition: `[1] + [2]` → `[1,2]`
- [ ] Slicing with step: `arr[0:10:2]`

### 7.2 Sets and Frozen Sets
- [ ] Set literal: `{1, 2, 3}`
- [ ] Set operations: union, intersection, difference
- [ ] frozenset type

### 7.3 Dictionaries (Maps)
- [ ] Dict comprehension: `{k: v for k, v in items}`
- [ ] Dict merging: `{**dict1, **dict2}` (Python 3.9+)
- [ ] Dict operators: `|` and `|=`

### 7.4 Tuples
- [ ] Named tuples
- [ ] Tuple unpacking: `a, *b = (1, 2, 3, 4)`

---

## Phase 8: AI/ML Features (Priority: LOW - Differentiator)

### 8.1 Tensor Operations
- [ ] `tensor` type (already exists partially)
- [ ] GPU acceleration hints: `@gpu`
- [ ] Auto differentiation

### 8.2 Neural Network DSL
- [ ] `nn.linear()`, `nn.conv2d()`
- [ ] `nn.relu()`, `nn.sigmoid()`, `nn.softmax()`
- [ ] `nn.linear()`, `nn.dropout()`
- [ ] Training loop abstractions

### 8.3 Built-in ML Utilities
- [ ] `math.dist()` - Distance functions
- [ ] `stats.mean()`, `stats.stdev()`
- [ ] `random.choice()`, `random.sample()`

---

## Phase 9: CLI & Developer Experience (Priority: MEDIUM)

### 9.1 Package Manager
- [ ] `vex add <package>` - Add dependency
- [ ] `vex remove <package>` - Remove dependency
- [ ] `vex update` - Update packages
- [ ] `vex publish` - Publish to registry

### 9.2 REPL Enhancements
- [ ] Auto-complete
- [ ] History navigation
- [ ] Multi-line editing

### 9.3 Debugging
- [ ] Breakpoints
- [ ] Step-through debugging
- [ ] Variable inspection

---

## Phase 10: Performance Optimizations (Priority: LOW)

### 10.1 JIT Compilation
- [ ] Hot function detection
- [ ] Native code generation
- [ ] Inline caching

### 10.2 Memory Optimizations
- [ ] Object interning for strings
- [ ] Slot optimization for classes
- [ ] Buffer pooling

---

## Implementation Priority Order

1. **Type System** - Essential for IDE support and safety
2. **Error Handling** - Critical for robustness
3. **Decorators** - Highly requested feature
4. **f-strings** - Developer experience
5. **Generators** - Important for idiomatic code
6. ***args/**kwargs** - Function flexibility
7. **Context Managers** - Resource management
8. **Advanced Concurrency** - Performance
9. **AI/ML Features** - Differentiation

---

## Success Metrics

- ✅ All Python features implemented
- ✅ Type system superior to Python (static + dynamic)
- ✅ Performance 2-10x faster than Python
- ✅ AI/ML capabilities built-in
- ✅ Full IDE support with type inference
- ✅ Package ecosystem ready
