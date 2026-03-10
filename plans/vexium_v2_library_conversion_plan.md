# Vexium v2 Library Conversion Plan

## Current State

The v2 libraries (nn.vxm, redis.vxm, sqlite.vxm, shell.vxm, git.vxm, http.vxm, unsafe.vxm) use Python-like syntax that isn't compatible with the current Vexium v1 parser.

## Issues Found

1. **`can` keyword** - Used for methods outside of structs (parser expects `fn`)
2. **`for each` syntax** - Parser expects `for x in arr:` not `for each x in arr:`
3. **`elif` keyword** - Parser expects nested `if/else` not `elif`
4. **`in` operator** - Parser doesn't support `if x in arr:` syntax
5. **Slice notation** - `arr[1:3]` not fully supported
6. **Method calls with self** - `self.method()` inside struct methods

## Solution

### Option 1: Fix Parser (Recommended)
Add support for v2 syntax features to the parser:

1. Make `can` an alias for `fn` (already done for top-level)
2. Add `for each` loop syntax
3. Add `elif` support
4. Add `in` operator for arrays and maps
5. Add slice notation support

### Option 2: Rewrite Libraries (Quick Fix)
Rewrite all v2 libraries to use v1-compatible syntax:

**Before (v2):**
```vexium
can method_name(param):
    for each x in arr:
        if x > 0:
            do_something()
        elif x < 0:
            do_other()
```

**After (v1):**
```vexium
fn method_name(param):
    for i in 0 to len(arr):
        let x be arr[i]
        if x > 0:
            do_something()
        else:
            if x < 0:
                do_other()
```

## Recommendation

Implement **Option 1** by extending the parser to support v2 syntax. This is better because:
1. The spec already defines v2 syntax
2. Users will expect `can`, `for each`, `elif` to work
3. The tokenizer already supports these tokens

## Implementation Steps

1. **Fix `for each` parsing** - Update parser to recognize `for each x in arr:`
2. **Fix `elif` parsing** - Add elif as alternative to else-if nesting
3. **Fix `in` operator** - Add binary operator for array/map membership
4. **Fix slice notation** - Add parsing for `arr[start:end]`
5. **Test all libraries** - Verify all 7 libraries work

## Files to Modify

- `vex/src/parser.c` - Main parser logic
- `vex/src/lexer.c` - Tokenizer (already has tokens)
- `vex/src/ast.h` - Add NODE_FOREACH, NODE_ELIF if needed
- `vex/src/interpreter.c` - Execute new node types
