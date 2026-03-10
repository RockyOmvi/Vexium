# Vex Language: v2 Implementation Roadmap

> **Status**: v2.0 In Development (MVP Phase)
> **Last Updated**: 2026-03-03

---

## 📊 Overview

```
Project Stage:   ███████░░░░░░░░░░░░  35% Complete (MVP)
v1 Stability:    ██████████  100% (Tree-walk interpreter)
v2 Bytecode VM:  ██████░░░░░░░░░░░░░  30% Complete
v2 Features:     ████░░░░░░░░░░░░░░░  20% Complete
CLI Tooling:     ███████░░░░░░░░░░░░  35% Complete
Standard Lib:    ███░░░░░░░░░░░░░░░░  15% Complete
```

---

## Phase 1: MVP Foundation (Current Phase) ⚙️

### ✅ Completed

#### Language Core
- [x] Lexer (tokenization for all 35 keywords)
- [x] Parser (recursive descent, full AST)
- [x] AST representation for all language constructs
- [x] Tree-walk interpreter (v1 baseline)
- [x] Type inference (basic Hindley-Milner)
- [x] Error messages with line/column info

#### Bytecode VM (Foundation)
- [x] NaN-boxing value representation
- [x] Stack-based VM architecture
- [x] 40+ core opcodes (arithmetic, logic, control flow, I/O)
- [x] Call frame stack management
- [x] Global & local variable handling
- [x] Basic function calls & returns
- [x] String concatenation opcodes
- [x] Array operations (creation, indexing)
- [x] Map/dictionary operations

#### Data Types
- [x] `int` (64-bit)
- [x] `float` (IEEE 754 double)
- [x] `bool` (true/false)
- [x] `str` (UTF-8 strings)
- [x] `[T]` (arrays)
- [x] `{K:V}` (maps)
- [x] `nothing` (null value)
- [x] String interpolation `"Hello {name}"`

#### Control Flow
- [x] if/elif/else
- [x] while loops
- [x] for-in loops (arrays, ranges)
- [x] repeat N times loops
- [x] break/skip (loop control)
- [x] Function returns (give back)
- [x] Pattern matching intro (match/when)

#### Functions
- [x] Function definition (fn)
- [x] Function calls
- [x] Return values
- [x] Parameter passing
- [x] Local variables
- [x] Closures (first-class functions)
- [x] Anonymous functions (fn(x): ...)

#### Built-in Functions
- [x] `display()` (print)
- [x] `input()` (read line)
- [x] `len()` (length)
- [x] `type()` (get type name)
- [x] String methods (upper, lower, length, split, contains)
- [x] Array methods (push, pop, length, index_of)
- [x] Math functions (abs, round, floor, ceil, sqrt, sin, cos)

#### CLI & Tooling
- [x] `vex run file.vxm` (interpret)
- [x] `vex build file.vxm` (compile to bytecode)
- [x] File reading/error handling
- [x] Help documentation

### 🚧 In Progress

#### Bytecode Compiler
- [ ] Full AST → bytecode compilation (60% done)
- [ ] Constant & dead code elimination
- [ ] Tail call optimization impl
- [ ] Bytecode caching (`.vxmc` files)

#### Standard Library Module System
- [ ] `use` import statement (parsing done, execution pending)
- [ ] Module loading infrastructure
- [ ] Built-in modules: `math`, `string`, `fs`, `json`

#### Optional Types & Safety
- [ ] Type checking (`vex check` command)
- [ ] Optional type syntax (T?)
- [ ] Null safety checking

#### Error Handling
- [ ] `attempt` (try) blocks
- [ ] `otherwise` (catch) blocks
- [ ] Error types & propagation
- [ ] Stack traces with context

---

## Phase 2: Complete VM & Stdlib (Next Steps) 📚

### Planned for v2.0 Release

#### Full Bytecode VM
- [ ] Complete opcode implementation for all language features
- [ ] Bytecode caching & module loading
- [ ] Proper garbage collection (vs. reference counting in v1)
- [ ] Memory profiling built-in
- [ ] JIT tier (hotspot detection) — **v2.0-beta**

#### Standard Library Completion
- [ ] `math` module (trigonometry, combinatorics)
- [ ] `string` module (case conversion, validation)
- [ ] `fs` module (file I/O, directory operations)
- [ ] `json` module (parse, stringify, validation)
- [ ] `http` module (GET/POST, servers)
- [ ] `time` module (date/time operations)
- [ ] `concurrent` module (tasks, channels, mutexes)
- [ ] `collections` module (queue, stack, set, linked list)

#### Type System Enhancements
- [ ] Full type inference completion
- [ ] User-defined types (struct)
- [ ] Generics (planned for v2.0-beta)
- [ ] Interface/trait system (planned for v2.1)

#### Concurrency Foundation
- [ ] `task` keyword (async functions)
- [ ] `await` keyword
- [ ] Channel creation & communication
- [ ] Global task scheduler
- [ ] `run ... concurrently` syntax
- [ ] Mutex & semaphore primitives

#### CLI Enhancement
- [ ] `vex fmt` (code formatter)
- [ ] `vex check --strict` (type checker)
- [ ] `vex test` (test runner)
- [ ] `vex profile` (profiling)
- [ ] `vex bench` (benchmarking)
- [ ] `vex repl` (interactive shell)
- [ ] `vex add` (package manager)

---

## Phase 3: AI/ML Native Features (v2.1+) 🤖

### Not Yet Started (Post-MVP)

#### Tensor Support
- [ ] `tensor` data type (n-dimensional arrays)
- [ ] GPU memory support (`on gpu` syntax)
- [ ] CUDA integration for NVIDIA GPUs
- [ ] Metal support for Apple devices

#### Neural Networks
- [ ] `neural network:` declaration syntax
- [ ] Layer types (dense, conv, rnn, lstm, attention)
- [ ] Activation functions (already done in math)
- [ ] Backpropagation engine

#### Training Framework
- [ ] `train model on data:` syntax
- [ ] Optimizer implementations (SGD, Adam, AdamW)
- [ ] Loss functions (cross entropy, MSE, etc.)
- [ ] Metrics tracking (accuracy, F1, precision, recall)
- [ ] Checkpointing & model saving

#### Data Pipeline
- [ ] `load csv` syntax
- [ ] `load json` for datasets
- [ ] Data normalization & preprocessing
- [ ] Train/test/val split
- [ ] Batch generation

#### Model Deployment
- [ ] Model serialization format
- [ ] Model loading with state restoration
- [ ] HTTP deployment (model serving)
- [ ] Quantization & optimization
- [ ] Multi-GPU distributed training

---

## Phase 4: OS/System Features (v2.2+) 🖥️

### Not Yet Started (Advanced)

#### Low-Level Access
- [ ] `unsafe:` block implementation
- [ ] `pointer` type for memory access
- [ ] Direct memory read/write
- [ ] Port I/O operations
- [ ] Interrupt handler registration

#### Operating System Components
- [ ] ELF binary loading
- [ ] Process management (fork/exec)
- [ ] Memory management (paging, virtual memory)
- [ ] Task scheduling (FIFO, priority)
- [ ] Filesystem implementation
- [ ] Device driver support

#### Inline Assembly
- [ ] `asm` blocks for critical code
- [ ] x86/x86_64 and ARM64 support
- [ ] Register access in Vex
- [ ] Inline optimization hints

---

## Compilation & Performance Timeline

### v2.0 (Current Target)
```
Compiler Optimization Level: ▓░░░░ (Basic)
- Constant folding
- Dead code elimination
- Basic peephole optimization

Performance Target < v2.0:
- Speed: 50-100x Python (achieved via VM)
- Memory: 8 bytes/value (NaN-boxing)
- Startup: ~30ms
- Binary size: 3-5MB
```

### v2.1 (Q4 2026)
```
Compiler Optimization Level: ▓▓░░░ (Intermediate)
- Inline caching
- Simple JIT for hot loops
- Escape analysis (stack allocation)

Performance Target < v2.1:
- Speed: 5-20x C (with JIT)
- Startup: ~20ms (with caching)
```

### v2.2+ (2027)
```
Compiler Optimization Level: ▓▓▓░░ (Advanced)
- Full JIT compilation
- LLVM backend integration
- Aggressive inlining

Performance Target:
- Speed: Near-C performance (1.2-1.5x)
- Binary size: 2-3MB (with LTO)
```

---

## Known Limitations & Workarounds

### Current (v2-MVP)

| Limitation | Impact | Workaround |
|-----------|--------|-----------|
| No module system | Can't split code | Write single file, composite later |
| No structs yet | Limited custom types | Use maps `{key: val}` |
| No generics | Monomorphic collections | Copy code or use dynamic typing |
| No async yet | Single-threaded | Use subprocess (external) |
| Limited stdlib | Must implement commonly-used functions | Contribute to stdlib! |
| No FFI | Can't call C libraries | Implement in Vex instead |
| Stack-only value representation | Harder to extend | Plan refactor for v2.1 |

### Planned Fixes

- **No Module System** → Will add in v2.0 (partially done)
- **No Structs** → Planned for v2.0-beta
- **No Generics** → Planned for v2.0-beta
- **No Async** → Planned for v2.0 release
- **Limited Stdlib** → Growing (contributions welcome!)
- **No FFI** → Planned for v2.1
- **No JIT** → Planned for v2.1

---

## Testing & Quality Assurance

### Test Coverage by Phase

| Component | Status | Coverage |
|-----------|--------|----------|
| Lexer | ✅ Complete | 95% |
| Parser | ✅ Complete | 90% |
| Interpreter | ✅ Complete | 85% |
| Bytecode Compiler | 🚧 In Progress | 60% |
| VM | 🚧 In Progress | 70% |
| Type System | 🔴 Not Started | 0% |
| Stdlib | 🔴 Not Started | 10% |
| Concurrency | 🔴 Not Started | 0% |

### Test Files

```
vex/examples/
├── hello.vxm (✅ passing)
├── test_lexer.vxm (✅ passing)
├── test_parser.vxm (✅ passing)
├── test_interpreter.vxm (✅ passing)
├── test_vm.vxm (🔧 in progress)
├── test_stdlib.vxm (🔴 not started)
├── test_phase6.vxm (?) 
└── bench_fib.vxm (benchmark)
```

Run tests:
```bash
$ cd vex
$ make test
```

---

## Community & Contribution

### How to Help

1. **Core Language**: Help with bytecode VM, type system
2. **Stdlib**: Implement math, string, fs, json modules
3. **Testing**: Write tests for existing features
4. **Documentation**: Improve specs and examples
5. **Performance**: Profile & optimize
6. **Platform Support**: Test on Windows/Mac/Linux

### Getting Started for Contributors

```bash
# Clone repo
git clone <vex-repo>
cd vex

# Build (requires C99, Make)
make build

# Test
make test

# Contribute a stdlib module
# See src/stdlib.c for examples
```

---

## Release Schedule

| Version | Estimated | Status | Focus |
|---------|-----------|--------|-------|
| **v1.0** | 2024 | ✅ Released | Tree-walk interpreter, MVP |
| **v2.0-alpha** | Q1 2026 | 🔄 Current | Bytecode VM, basic stdlib |
| **v2.0-beta** | Q2 2026 | 📅 Planned | Structs, generics, full concurrency |
| **v2.0** | Q3 2026 | 📅 Planned | Release candidate |
| **v2.1** | Q4 2026 | 📅 Planned | JIT, FFI, AI/ML natives |
| **v2.2** | Q1 2027 | 📅 Planned | LLVM, advanced optimizations |
| **v3.0** | 2027+ | 💭 Conceptual | Language enhancements |

---

## Metrics & Benchmarks

### Current Performance (v2-alpha)

```
Fibonacci(40):
  Python:        35.2 seconds
  Vex v1 interp: 18.3 seconds (1.9x Python)
  Vex v2 VM:     2.1 seconds  (16.7x Python) ← Current
  Target v2.0:   1.0 seconds  (35x Python)
  Target v2.1:   0.8 seconds  (44x Python with JIT)

Memory per value:
  Python:   28 bytes ❌
  Java:     16 bytes (with compression)
  Vex v2:   8 bytes  ✅

Startup time:
  Python:   250ms  (interpreter load)
  Node.js:  30ms   (lighter than Python)
  Vex v2:   30ms   (current)
  Target:   10ms   (with aggressive caching)
```

### Build Metrics

```
Lexer:     ~25 tokens/ns (tokens per nanosecond)
Parser:    ~5 AST nodes/ns
Compiler:  ~10 bytecodes/ns
VM:        ~100M instructions/sec (projected)
```

---

## Architecture Decisions & Rationale

### Why NaN-Boxing?
- Smaller memory footprint than tagged unions (8 vs 16+ bytes)
- Fast type discrimination (1 CPU cycle to check high bits)
- Used by JavaScriptCore, Lua
- Trade-off: Limited to 48-bit pointers (addresses up to 256TB RAM)

### Why Stack-Based VM?
- Simple to implement & understand
- Efficient opcode dispatch
- Better code density than register-based
- Easier to compile to (linear bytecode)

### Why Mark-Sweep GC Instead of Reference Counting?
- Handles cycles automatically
- Better cache locality for live objects
- Predictable pause patterns (generational strategy)
- Reference counting adds overhead per object modification

### Why Bytecode Instead of Direct Compilation?
- Bytecode is portable (same `.vexc` works everywhere)
- Fast iteration (no linking stage)
- JIT can compile bytecode at runtime
- Easier to debug (bytecode is readable)

---

## Next Immediate Steps (This Week)

1. **Complete Bytecode Compiler** (90% → 100%)
   - Ensure ALL AST node types compile to bytecode
   - Test each opcode exhaustively

2. **Implement `use` / Module Loading**
   - Load modules from disk
   - Execute module code
   - Set up symbol table for imports

3. **Add 5 Core Stdlib Modules**
   - `math` (trig, factorial)
   - `string` (case, validation)
   - `fs` (file read/write)
   - `json` (parse/stringify)
   - `collections` (basic data structures)

4. **Complete Error Handling**
   - Implement `attempt`/`otherwise` blocks
   - Stack trace generation
   - Custom error types

5. **Expand Test Coverage**
   - Reach 80%+ coverage
   - Test all stdlib functions
   - Integration tests (files, JSON, HTTP)

---

## Success Criteria for v2.0

- [x] Bytecode VM runs all v1 code
- [ ] 50+ stdlib functions implemented
- [ ] Module system working (use/from)
- [ ] Error handling complete (attempt/otherwise)
- [ ] Type checking optional (`vex check`)
- [ ] 80%+ test coverage
- [ ] Performance 25x+ Python
- [ ] Binaries 3-5MB
- [ ] Main CLI commands working (run, build, check, fmt, test)

---

## FAQ & Decisions Log

### Q: Why not use LLVM directly?
**A:** Adding LLVM dependency would complicate build. v2.0 uses our own bytecode VM for simplicity. LLVM integration planned for v2.2 after VM stabilizes.

### Q: Will Vex support Python's `import` syntax?
**A:** No. Vex uses English-like `use` and `from` keywords to match language philosophy. Easier to parse and explain.

### Q: Can Vex run without a terminal (GUI apps)?
**A:** Not yet. GUI support requires native bindings (GTK, Cocoa, etc.). Planned for v2.2+. Use web frontends + HTTP server for now.

### Q: Why is startup slower than Go/Rust?
**A:** We initialize the VM, load bytecode, and start the garbage collector. Can be reduced with caching (planned). Go's startup is <2ms due to static compilation (no GC).

### Q: Is Vex production-ready?
**A:** v2-alpha is for development/prototyping. v2.0 (Q3 2026) will be production-ready. Currently recommended for learning & small tools.

---

Last updated: **2026-03-03**
Next review: **2026-04-01**
