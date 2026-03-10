# Vexium Bytecode Caching (.vxmc files)

## Overview

Vexium supports bytecode caching to improve startup performance. When a Vexium source file (`.vxm`) is compiled for the VM, the bytecode can be saved to a cache file (`.vxmc`) for faster subsequent executions.

## How It Works

### Cache Generation

When you run a Vexium program using the VM mode:

```bash
vexium run-vm program.vxm
```

If no valid cache exists, the compiler:
1. Parses the source code
2. Compiles to bytecode
3. Saves bytecode to `program.vxmc`
4. Executes the program

### Cache Validation

On subsequent runs, Vexium validates the cache:
- **Source Hash**: FNV-1a hash of source content is compared
- **Timestamp**: File modification time is checked
- **Version**: Cache format version must match

If the cache is invalid or outdated, it is regenerated.

## Cache File Format

### Header (64 bytes)

| Field | Size | Description |
|-------|------|-------------|
| Magic | 4 bytes | "VXMC" identifier |
| Version | 4 bytes | Cache format version |
| Source Hash | 4 bytes | FNV-1a hash of source |
| Timestamp | 8 bytes | Source file mtime |
| Code Size | 4 bytes | Bytecode size in bytes |
| Constants | 4 bytes | Number of constants |
| Reserved | 36 bytes | Padding |

### Data Section

1. **Bytecode**: Raw bytecode instructions
2. **Constants**: Serialized constant pool values
3. **Line Info**: Source line numbers for debugging

## API

### `cache_check_validity(source_path, cache_path)`

Checks if a cache file exists and is valid for the given source.

**Returns:**
- `CACHE_VALID` - Cache is usable
- `CACHE_NOT_FOUND` - No cache exists
- `CACHE_OUTDATED` - Source has changed
- `CACHE_CORRUPTED` - Cache is invalid
- `CACHE_VERSION_MISMATCH` - Format version mismatch

### `cache_save_chunk(cache_path, chunk, source_path, source_hash)`

Saves a compiled chunk to a cache file.

### `cache_load_chunk(cache_path, chunk)`

Loads a compiled chunk from a cache file.

### `cache_generate_path(source_path)`

Generates cache file path from source path (e.g., `file.vxm` → `file.vxmc`).

## Implementation Status

- ✅ Cache file creation
- ✅ Source validation (hash + timestamp)
- ✅ Value serialization (int, float, bool, string)
- ✅ Automatic cache generation on VM run
- ⚠️ Cache loading (requires object reconstruction)

## Future Enhancements

1. **Full Cache Loading**: Complete object reconstruction for cached bytecode
2. **AOT Compilation**: Standalone compiler for build systems
3. **Cache Precompilation**: `vexium compile file.vxm` command
4. **Cross-Platform Caches**: Platform-independent cache format

## Cache Invalidation

Cache is automatically invalidated when:
- Source file is modified
- Source content changes (detected via hash)
- Vexium version changes (cache format version mismatch)
- Cache file is corrupted

## Performance Impact

The cache generation itself has minimal overhead. The main benefit will come from faster loading once full cache loading is implemented, eliminating the parse and compile phases on subsequent runs.
