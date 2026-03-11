# Vexium: Missing Features, Errors, and Failures
**Date:** March 11, 2026  
**Methodology:** Code inspection + runtime validation + cross-backend testing

---

## Part 1: Critical Errors (Runtime/Compilation)

### 1.1 Concurrency System Broken ❌ CRITICAL

**Status**: Tokens exist but language-level support missing  
**Evidence**: `vex/test_spawn.vxm` fails at parse time in both interpreter and VM

```
Error [line 3]: Expected '(' after 'channel' (got IDENTIFIER 'ch')
Error [line 4]: Expected ')' after channel arguments (got NEWLINE)
Parsing failed with errors.
```

**Root Cause**: Parser grammar incomplete
- tokens.h: `TOKEN_SPAWN`, `TOKEN_AWAIT` defined ✅
- lexer.c: keywords registered ✅
- parser.c: `match_token(p, TOKEN_AWAIT)` at line 83, `match_token(p, TOKEN_SPAWN)` at line 404 exist but no AST node creation
- compiler.c: No `OP_SPAWN`, `OP_AWAIT` opcodes

**What's Missing**:
1. AST node types: `NODE_SPAWN`, `NODE_AWAIT`, `NODE_SELECT`, `NODE_CHANNEL`
2. Parser rules in `parser.c` to create AST nodes when tokens matched
3. Compiler codegen for concurrent operations
4. VM instruction decode for spawn/await

**Impact**: Any code using `spawn`, `await`, or `channel` fails before execution  
**Blocks**: All async/concurrent patterns

---

### 1.2 Type System Not Connected ❌ CRITICAL

**Status**: Type checker runs but results **unused** by compiler  
**Evidence**: Deliberate type mismatch passes `vexium check`, fails at runtime

```vex
let x be 1
x be "hello"
display x + 1
```

**Output**:
```
vexium check: "Type checking passed! No errors found."
vexium run:   "Error [line 3]: Unsupported binary operation"
```

**Root Cause**: `src/main.c` runs type checker separately, compiler ignores results
- main.c line 46-47: Type check runs on AST
- main.c line 225: Lint calls type checker again
- src/compiler.c: ZERO references to type_system functions
- Compiler still generates code even when type checker would reject it

**Code Evidence**:
```c
// src/main.c:201 - check_file()
int tc = vex_type_check(prog);
if (tc != 0) { ... ast_free(prog); ... return 1; }
printf("Type checking passed! No errors found.\n");

// But compile path ignores this:
// src/compiler.c has NO type_* function calls
```

**What's Missing**:
1. Type-aware code generation in `compiler.c`
2. Type error reporting integrated into `compile_expr()`, `compile_stmt()`
3. Generic type instantiation (`type_apply()` at line 711 in type_system.c but never called from compiler)
4. Occurs check for infinite types (exists but unused)

**Impact**: Static analysis provides false confidence; runtime errors catch bugs too late  
**Equivalent Python Gap**: Like disabling `mypy` then running code

---

### 1.3 Interpreter/VM Numeric Inconsistency ❌ HIGH

**Status**: Same code produces different numeric types between backends  
**Evidence**: `vex/test_tail_call_opt.vxm` TCO test results diverge

```
=== INTERPRETER ===
TCO factorial_tco(5) = int     ✅
sum_to_n(1000) = int           ✅

=== VM ===
TCO factorial_tco(5) = float   ❌
sum_to_n(1000) = float         ❌
```

**Root Cause**: TCO implementation differs (or numeric widening rules differ)
- vm.c line 2500+: `OP_TAIL_CALL` reuses frame
- interpreter.c: Different tail-call handling path
- One backend widening to float on accumulation, other preserving int

**What's Missing**: Unified numeric semantics specification  
**Impact**: Code behaves differently under VM vs interpreter (data structure differences, performance surprises, logic bugs)

---

### 1.4 GC Root Tracking Incomplete ❌ MEDIUM

**Status**: Objects created when `running_vm` is NULL may leak  
**Code**: src/vm.c line 33-34
```c
static void allocate_obj(Obj* obj) {
    obj->next = running_vm ? running_vm->objects : NULL;  // If NULL, obj not tracked!
    if (running_vm) running_vm->objects = obj;
}
```

**Scenario**: Task spawns and allocates objects before VM fully initialized  
**Impact**: Memory leaks in concurrent code (once spawn/await fixed)

---

## Part 2: Missing Language Features

### 2.1 Concurrency Keywords (0% Complete) ❌ CRITICAL

| Feature | Status | Blocks |
|---------|--------|--------|
| `spawn foo()` | Tokens only; no parse/codegen | Concurrency patterns |
| `let result be await task` | Tokens only | Blocking on tasks |
| `channel<T>` type | Not implemented | Message passing |
| `ch <- value` (send) | Not implemented | Channel operations |
| `value <- ch` (receive) | Not implemented | Channel operations |
| `select { ch1 <- x; ch2 <- y; }` | Not implemented | Multiplexing |

### 2.2 Type System Features (50% Code, 0% Integration) 🟡 HIGH

| Feature | Status |
|---------|--------|
| Type inference | ✅ Implemented but unused |
| Generic types `<T>` | ✅ Parsing works, instantiation fails |
| Type unification | ✅ Partial, occurs check missing |
| Type application | ✅ Exists but not called by compiler |
| Error reporting with types | ❌ Not wired |

**Example: Generics are broken**
```vex
fn identity<T>(x: T) -> T:
    give back x

let n be identity<int>(42)  // ❌ Generic resolution fails
```

### 2.3 Package Manager (Framework Only) 🟡 MEDIUM

| Command | Status | What Works |
|---------|--------|-----------|
| `vex init` | 🟡 Partial | Creates vex.toml stub |
| `vex add <pkg>` | 🟡 Stubbed | Requires vex.toml; no dependency solver |
| `vex lock` | ❌ Not implemented | No SAT solver |
| `vex publish` | ❌ Not implemented | No registry |
| `vex search` | ❌ Not implemented | No remote index |

**What's Missing**:
- Dependency resolution algorithm (SAT solver)
- Package registry client
- Lockfile generation/parsing
- Conflict detection
- Version constraints parsing

### 2.4 Native Compilation (0% Complete) ❌ LOW (Out of Scope)

| Feature | Status |
|---------|--------|
| `vex build --release` | ❌ Not implemented |
| `vex build --target linux-x64` | ❌ Not implemented |
| Cross-compilation | ❌ Not implemented |
| Binary size < 10MB | ❌ Not achieved |

---

## Part 3: Feature Gaps vs Python

### Python Advantages (Vexium Missing)

| Category | Python | Vexium | Gap |
|----------|--------|--------|-----|
| **Async/Await** | Production-ready (`asyncio`) | Broken at parse | Can't write concurrent code |
| **Static Analysis** | `mypy`, `pyright` integrated | Checker runs but ignored | False security |
| **Package Ecosystem** | 400k+ packages on PyPI | 0 packages | No code reuse |
| **Type Hints** | Fully integrated, enforced | Exists but not enforced | Type hints ignored |
| **Concurrency** | asyncio, threading, multiprocessing | Task framework incomplete | Task system broken |
| **Numeric Stability** | Consistent across interpreters | Divergent (int vs float) | Reproducibility issues |
| **Error Messages** | Excellent context | Basic | Hard to debug |
| **Stdlib Completeness** | 200+ modules | ~12 modules | Limited batteries |

### Where Vexium Leads

| Category | Vexium | Python | Advantage |
|----------|--------|--------|-----------|
| **Dual Execution** | Interpreter + VM bytecode | Interpreter only | Fast path option |
| **AI/ML Built-in** | Tensors, NN, ONNX, transformers | Library ecosystem | Native semantics |
| **GPU Support** | Kernel compilation, memory mgmt | CUDA via libraries | First-class GPU |
| **Data Science DSL** | CSV, splits, shuffle modules | Pandas ecosystem | Lighter weight |
| **Binary Size** | ~4MB | 30MB+ | Mobile-friendly |

---

## Part 4: Validation Results

### Passing Tests (March 11, 2026)
✅ AI module tests (interpreter + VM)  
✅ Data module tests  
✅ Transformer tests  
✅ ONNX export/import  
✅ GPU module tests  
✅ Tail-call optimization recursion tests  
✅ Type check command (though too permissive)  
✅ Build command (bytecode compilation)  
✅ REPL with multiline support  

### Failing Tests
❌ Concurrency (spawn/await/channel parse failures)  
❌ Type error detection (false negatives)  
❌ Numeric consistency (TCO interpreter vs VM)  
❌ Package manager (needs init)  

---

## Part 5: Error Categories

### Parse Errors (Concurrency + Type)
- Tokens recognized but grammar incomplete
- No error recovery; full parse failure
- Impact: Any forbidden pattern blocks entire file

### Runtime Errors (Type Mismatch)
- Operations on incompatible types (e.g., string + int)
- Caught only during execution
- Late error detection defeats static analysis

### Semantic Errors (Numeric Divergence)
- Same code, different results (int vs float)
- Hard to debug without cross-backend testing
- Affects reproducibility and portability

### Missing Implementation (Concurrency, Generics)
- Framework exists (C-level tasks, type structs)
- Language-level integration incomplete
- Parser/compiler gaps prevent usage

---

## Part 6: Code Statistics

| Metric | Value | Note |
|--------|-------|------|
| Lines of type_system.c | 800+ | Mostly unused |
| Lines of compiler.c | 1200+ | Zero type-system integration |
| Task.c functions | 10+ | Not reachable from language |
| Test files | 40+ | Good coverage for what works |
| Unsupported features | 15+ | parse errors block code |

---

## Recommendations for Stabilization

### Must Fix (Blocking Usability)
1. **Wire type system into compiler** (~1k LOC refactor in compiler.c)
2. **Complete concurrency parse/codegen** (~800 LOC in parser.c + compiler.c)
3. **Unify interpreter/VM numeric semantics** (benchmark + align widening rules)

### Should Fix (Ecosystem)
4. Implement SAT solver for dependencies (~500 LOC)
5. Add native compilation targets (~2k LOC)

### Nice to Have (Polish)
6. Better error messages with source context
7. Debugger integration
8. Performance profiler

---

## Files to Review

**Highest Priority**:
- `vex/src/compiler.c` — Missing type integration
- `vex/src/parser.c` — Incomplete concurrency grammar
- `vex/src/type_system.c` — Unused but correct

**Supporting**:
- `vex/src/vm.c` — TCO implementation (numeric divergence)
- `vex/src/task.c` — Framework incomplete
- `vex/src/main.c` — Type check logic exists but unused
