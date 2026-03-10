# Vex v2 Documentation Index

Welcome to the Vex Programming Language v2 development documentation. This index provides links to all guides and their purposes.

---

## Welcome & Getting Started

- **[QUICKSTART_V2.md](QUICKSTART_V2.md)** - 15-minute guide to using Vex
  - Installation instructions
  - Basic syntax with examples
  - Building your first program
  - Common patterns and recipes
  - **→ Start here if new to Vex**

- **[language_v2_spec.md](language_v2_spec.md)** - Complete language specification
  - Formal grammar (EBNF)
  - All 35 keywords documented
  - Type system explanation
  - Built-in data types
  - Standard library reference
  - **→ Use to understand language details**

---

## Planning & Project Management

- **[IMPLEMENTATION_ROADMAP.md](IMPLEMENTATION_ROADMAP.md)** - Development timeline and status
  - Current status: v2.0-alpha MVP (35% complete)
  - Completed components checklist
  - In-progress work
  - Release schedule
  - Performance targets
  - **→ Track progress and plan work**

- **[V1_VS_V2_MIGRATION.md](V1_VS_V2_MIGRATION.md)** - Comparison and upgrade guide
  - What's new in v2
  - Performance improvements (8-16x faster)
  - Backwards compatibility (100%)
  - Migration steps from v1
  - Quick comparison table
  - **→ For existing v1 users**

---

## Implementation Guides (For Contributors)

These guides teach HOW to implement each major system. Use these when building features.

### Core Runtime

1. **[VM_RUNTIME_IMPLEMENTATION.md](VM_RUNTIME_IMPLEMENTATION.md)** - Bytecode VM and runtime
   - NaN-boxing value representation (8 bytes)
   - 40+ opcodes and instruction format
   - Stack-based execution model
   - Generational garbage collection
   - Function calls and frames
   - **→ When implementing VM features**

2. **[BYTECODE_COMPILER_GUIDE.md](BYTECODE_COMPILER_GUIDE.md)** - Compiling AST to bytecode
   - Chunk structure and opcode emissison
   - Compilation patterns for all node types:
     - Literals and variables
     - Arithmetic and comparisons
     - Control flow (if, while, for)
     - Functions and calls
     - Collections (arrays, maps)
   - **→ When adding/fixing bytecode compilation**

### Language Features

3. **[TYPE_SYSTEM_IMPLEMENTATION.md](TYPE_SYSTEM_IMPLEMENTATION.md)** - Type checking and inference
   - Hindley-Milner inference algorithm
   - Unification for type constraints
   - Gradual typing (dynamic ↔ static)
   - Strict mode type checking
   - Type annotations (`:` syntax)
   - **→ When building type system**

4. **[ERROR_HANDLING_GUIDE.md](ERROR_HANDLING_GUIDE.md)** - Exception and error handling
   - try-catch blocks (`attempt...otherwise`)
   - Error object structure
   - Stack trace capture
   - Error propagation patterns
   - Custom error types
   - **→ When extending error handling**

5. **[CONCURRENCY_IMPLEMENTATION.md](CONCURRENCY_IMPLEMENTATION.md)** - Parallel execution
   - Task spawning and async/await
   - Channel communication (send/recv)
   - Synchronization: mutex, rwlock, semaphore, barrier
   - Common patterns: worker pool, fan-out/fan-in, timeouts
   - Memory safety in concurrent code
   - **→ When building async features (v2.1+)**

### Real Systems

6. **[STDLIB_IMPLEMENTATION_GUIDE.md](STDLIB_IMPLEMENTATION_GUIDE.md)** - Standard library
   - 50+ built-in functions
   - Math module (trig, logarithms, constants)
   - String operations (case, splitting, formatting)
   - File I/O (read, write, exists, paths)
   - JSON parsing/serialization
   - Collections (queue, stack)
   - **→ When adding stdlib modules**

7. **[MODULE_SYSTEM_IMPLEMENTATION.md](MODULE_SYSTEM_IMPLEMENTATION.md)** - Module loading
   - File-based module organization
   - Module loading and caching
   - Circular dependency detection
   - Symbol table management
   - Public/private symbols
   - Package structure
   - **→ When building module system**

---

## Development Process

- **[DEVELOPER_CONTRIBUTION_GUIDE.md](DEVELOPER_CONTRIBUTION_GUIDE.md)** - How to contribute
  - Setup development environment
  - File structure overview
  - Implementation tasks by priority
  - Step-by-step feature implementation
  - Code style guide
  - Testing strategies
  - Debugging techniques
  - Performance profiling
  - Pull request process
  - **→ Before contributing code**

---

## Reference Material

### Language Reference
| Document | Purpose |
|----------|---------|
| language_v2_spec.md | Complete language specification |
| QUICKSTART_V2.md | Quick tutorial |
| IMPLEMENTATION_ROADMAP.md | Feature status |

### Implementation Guides
| Document | System | Status |
|----------|--------|--------|
| VM_RUNTIME_IMPLEMENTATION.md | Execution engine | 80% complete |
| BYTECODE_COMPILER_GUIDE.md | Bytecode generation | 90% complete |
| TYPE_SYSTEM_IMPLEMENTATION.md | Type checking | 0% (planned) |
| ERROR_HANDLING_GUIDE.md | Error system | 70% complete |
| CONCURRENCY_IMPLEMENTATION.md | Async/tasks | 0% (v2.1+) |
| STDLIB_IMPLEMENTATION_GUIDE.md | Built-ins | 40% complete |
| MODULE_SYSTEM_IMPLEMENTATION.md | Modules | 30% complete |

### Getting Started
| Document | Audience |
|----------|----------|
| QUICKSTART_V2.md | New users |
| DEVELOPER_CONTRIBUTION_GUIDE.md | Contributors |
| language_v2_spec.md | Reference |

---

## Reading Paths

### "I want to learn Vex"
1. [QUICKSTART_V2.md](QUICKSTART_V2.md) - Learn syntax and basics
2. [language_v2_spec.md](language_v2_spec.md) - Deep dive into features
3. Examples in `examples/` directory - See working code

### "I want to contribute"
1. [DEVELOPER_CONTRIBUTION_GUIDE.md](DEVELOPER_CONTRIBUTION_GUIDE.md) - Setup and process
2. [IMPLEMENTATION_ROADMAP.md](IMPLEMENTATION_ROADMAP.md) - Find task to work on
3. Relevant implementation guide - Learn what to build
4. Existing code - Study similar features

### "I want to add feature X"
Find X in [IMPLEMENTATION_ROADMAP.md](IMPLEMENTATION_ROADMAP.md), identify its section:
- **Bytecode compiler**: Read [BYTECODE_COMPILER_GUIDE.md](BYTECODE_COMPILER_GUIDE.md)
- **Type system**: Read [TYPE_SYSTEM_IMPLEMENTATION.md](TYPE_SYSTEM_IMPLEMENTATION.md)
- **Error handling**: Read [ERROR_HANDLING_GUIDE.md](ERROR_HANDLING_GUIDE.md)
- **Concurrency**: Read [CONCURRENCY_IMPLEMENTATION.md](CONCURRENCY_IMPLEMENTATION.md)
- **Standard library**: Read [STDLIB_IMPLEMENTATION_GUIDE.md](STDLIB_IMPLEMENTATION_GUIDE.md)
- **Module system**: Read [MODULE_SYSTEM_IMPLEMENTATION.md](MODULE_SYSTEM_IMPLEMENTATION.md)

Then follow [DEVELOPER_CONTRIBUTION_GUIDE.md](DEVELOPER_CONTRIBUTION_GUIDE.md) step-by-step.

### "I want to optimize performance"
1. [VM_RUNTIME_IMPLEMENTATION.md](VM_RUNTIME_IMPLEMENTATION.md) - Understand execution model
2. [IMPLEMENTATION_ROADMAP.md](IMPLEMENTATION_ROADMAP.md) - Check optimization tasks
3. [DEVELOPER_CONTRIBUTION_GUIDE.md](DEVELOPER_CONTRIBUTION_GUIDE.md) - See profiling techniques

---

## Project Statistics

### Documentation
- Total guides: 13 comprehensive documents
- Total lines: ~10,000+ lines of detailed technical content
- Code examples: 500+ runnable examples
- Coverage: Every major system documented with implementation guides

### Implementation Status
- **Completed (70%)**
  - Lexer & Parser (100%)
  - Tree-walk Interpreter (100%)
  - Bytecode VM (80%)
  - Most stdlib (40%)
  - Error handling (70%)

- **In Progress (20%)**
  - Module system (30%)
  - Bytecode compiler optimization

- **Not Started (10%)**
  - Type system (planned for v2.0)
  - Concurrency (planned for v2.1)
  - JIT compilation (planned for v2.1)

### Test Coverage
- test_interpreter.vxm - Phase 1-5 features ✅
- test_vm.vxm - All bytecode operations ✅
- test_stdlib.vxm - Standard library functions ✅
- test_phase6.vxm - Advanced features ✅

All tests passing as of March 3, 2026.

---

## Quick Links to Source Code

- `src/main.c` - CLI entry point
- `src/lexer.c` - Tokenization
- `src/parser.c` - Syntax analysis
- `src/interpreter.c` - Tree-walk execution
- `src/compiler.c` - Bytecode compilation
- `src/vm.c` - Stack-based VM
- `src/stdlib.c` - Built-in functions
- `src/module_loader.c` - Module system

---

## Getting Help

### Documentation
All questions should be answerable from the guides above. Search by system:
- **How do I compile code to bytecode?** → BYTECODE_COMPILER_GUIDE.md
- **How do I add to stdlib?** → STDLIB_IMPLEMENTATION_GUIDE.md
- **How do I implement error handling?** → ERROR_HANDLING_GUIDE.md

### Working Code Examples
Check the `examples/` directory:
- `test_interpreter.vxm` - Basic features
- `test_vm.vxm` - Bytecode execution
- `test_stdlib.vxm` - Library functions
- `test_phase6.vxm` - Advanced features
- `hello.vxm` - Simplest example

### Similar Implementations
Study existing features in source code:
- Want to add array indexing? Search for existing array operations
- Want to add new operator? Look at how `+` and `-` are handled
- Want to add new type? Look at how `string` and `int` are handled

---

## Contributing

To contribute:
1. Read [DEVELOPER_CONTRIBUTION_GUIDE.md](DEVELOPER_CONTRIBUTION_GUIDE.md)
2. Pick a task from [IMPLEMENTATION_ROADMAP.md](IMPLEMENTATION_ROADMAP.md)
3. Read relevant implementation guide
4. Code and test using the style guide
5. Submit PR with tests

---

## Version Information

- **Vex Version**: v2.0-alpha
- **Release Date**: March 3, 2026
- **Documentation Updated**: March 3, 2026
- **Implementation Status**: MVP (35% toward v2.0 full release)

---

**Welcome to Vex! Happy coding! 🚀**

