# Vexium Codebase - Known Failures and Issues

## Critical Issues

### 1. Bytecode Cache String Reconstruction Fails
- **Location:** `src/bytecode_cache.c:232`
- **Issue:** String constants in cached bytecode become `nothing` values because the cache is loaded BEFORE the VM is created
- **Impact:** Programs using cached bytecode lose all string constants
- **Workaround:** Delete `.vxmc` files to force recompilation from source

### 2. Task System VM Type Mismatch
- **Location:** `src/task.c:186`
- **Issue:** `task->vm` is stored as `void*` but cast to `VM*` without validation
- **Impact:** Potential crashes if VM structure changes

### 3. Dead Code Elimination Incomplete
- **Location:** `src/compiler.c:1212`
- **Issue:** Only `give back` marks code unreachable; `break`/`skip` don't
- **Impact:** Code after break/skip still emitted unnecessarily

## Memory Safety Issues

### 4. strdup Fallback Doesn't Handle ENOMEM
- **Location:** `src/task.c:14`, `src/type_system.c:14`
- **Issue:** Custom `strdup` returns NULL on failure but callers don't always check
- **Impact:** Potential NULL pointer dereference

### 5. GC Root Tracking May Miss Objects
- **Location:** `src/vm.c:25`
- **Issue:** Objects created when `running_vm` is NULL aren't tracked
- **Impact:** Memory leaks for objects created outside vm_run()

## Concurrency Issues

### 6. Task System Not Fully Integrated
- **Location:** `src/task.c`
- **Issue:** Tasks are created but the spawn/thread pool isn't wired to the language
- **Impact:** `spawn` keyword doesn't actually create concurrent tasks

### 7. Channel Implementation Incomplete
- **Location:** Based on task.h structure
- **Issue:** Channels declared but no send/receive operations implemented

## Type System Issues

### 8. Type System Not Connected to Compiler
- **Location:** `src/type_system.c`
- **Issue:** Type inference exists but isn't called during compilation
- **Impact:** No actual type checking occurs

### 9. Type Variables Never Resolved
- **Location:** `src/type_system.c:295`
- **Issue:** `type_apply()` stubbed but not implemented
- **Impact:** Generic types can't be instantiated

## Build/Portability Issues

### 10. Thread-Local Storage Warning
- **Location:** `src/task.c:33`
- **Issue:** `__declspec(thread)` not supported by MinGW
- **Impact:** Tasks may not be thread-safe on Windows with MinGW

### 11. Windows-Only Code Paths
- **Location:** Multiple files
- **Issue:** Platform abstractions exist but Unix implementations are untested
- **Impact:** Likely fails to build on Linux/Mac

## Specification Gaps

### 12. Missing V2 Features
From `language_v2_spec.md`:
- No `await` keyword implementation
- No `defer` statements
- No `unsafe` blocks
- No operator overloading
- No traits/interfaces

## Test Failures

### 13. HTTP Module Not Tested
- **Location:** `src/http_module.c`
- **Issue:** Requires network; no mock/testing infrastructure
- **Impact:** Untested code may have bugs

### 14. Import Cycle Detection Missing
- **Location:** `src/module_loader.c`
- **Issue:** Circular imports will cause infinite recursion

## Recommendations

1. **Highest Priority:** Fix bytecode cache string reconstruction (#1)
2. **High Priority:** Connect type system to compiler (#8)
3. **Medium Priority:** Complete channel implementation (#7)
4. **Low Priority:** Add import cycle detection (#14)
