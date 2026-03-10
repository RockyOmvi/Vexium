# Vexium Compiler Optimizations

## Implemented Optimizations

### 1. Constant Folding (✅ Implemented)

Evaluates constant expressions at compile time.

**Examples:**
```vexium
let a be 2 + 3           # Compiled as: let a be 5
let b be 10 * 5         # Compiled as: let b be 50
let c be not true       # Compiled as: let c be false
let d be "a" + "b"      # Compiled as: let d be "ab"
```

**Implementation:** [`vex/src/compiler.c`](vex/src/compiler.c)
- `is_constant()` - Detects constant nodes
- `fold_binary()` - Evaluates binary operations
- `fold_unary()` - Evaluates unary operations

### 2. Bytecode Caching (✅ Foundation Implemented)

Saves compiled bytecode to disk for faster loading.

**Cache File:** `.vxmc` format
- 64-byte header with validation info
- Source hash for change detection
- Automatic cache invalidation

**Implementation:** [`vex/src/bytecode_cache.c`](vex/src/bytecode_cache.c)

### 3. Garbage Collection (✅ Implemented)

Mark-sweep collector for automatic memory management.

**Features:**
- Root marking from stack, globals, frames
- Growth-based triggering
- Configurable thresholds

**Implementation:** [`vex/src/gc.c`](vex/src/gc.c)

## Planned Optimizations

### Dead Code Elimination (📋 Planned)

Remove unreachable code after return/break/continue statements.

**Example:**
```vexium
fn example():
    give back 42
    display "unreachable"  # Should be eliminated
```

**Approach:**
- Track reachability during compilation
- Skip generating code for unreachable statements
- Issue warnings to developers

### Future Optimizations

1. **Peephole Optimization**: Pattern-based instruction replacement
2. **Common Subexpression Elimination**: Avoid redundant calculations
3. **Loop Invariant Code Motion**: Move loop-invariant calculations outside
4. **Tail Call Optimization**: Optimize recursive tail calls
5. **Inline Caching**: Speed up repeated method calls

## Performance Tips

1. Use `run-vm` instead of `run` for compiled execution
2. Enable bytecode caching for faster startup
3. Use constant expressions where possible (enables folding)
4. Avoid unnecessary string concatenation in loops

## Benchmarking

Use the built-in benchmark command:
```bash
vexium bench examples/bench_fib.vxm
```

This compares interpreter vs VM performance.
