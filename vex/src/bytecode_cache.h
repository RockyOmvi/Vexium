/* ═══════════════════════════════════════════════════════════════════════════════
 *  VEXIUM BYTECODE CACHE MODULE
 *  Save/Load compiled bytecode to/from .vxmc files
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef VEXIUM_BYTECODE_CACHE_H
#define VEXIUM_BYTECODE_CACHE_H

#include "opcodes.h"
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include "vm.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 *  CACHE FILE FORMAT (.vxmc)
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 *  Header (64 bytes):
 *    - Magic:        "VXMC" (4 bytes)
 *    - Version:      uint32_t (4 bytes) - cache format version
 *    - Source hash:  uint32_t (4 bytes) - hash of source for validation
 *    - Timestamp:    uint64_t (8 bytes) - source file modification time
 *    - Code size:    uint32_t (4 bytes) - bytecode size in bytes
 *    - Constants:    uint32_t (4 bytes) - number of constants
 *    - Reserved:     36 bytes (padding)
 *
 *  Data Section:
 *    - Bytecode:     code_size bytes
 *    - Constants:    serialized constant values
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define VXMC_MAGIC      "VXMC"
#define VXMC_VERSION    2  /* Bumped to v2 for new string format with pre-computed hash */

/* Cache file header */
typedef struct {
    char magic[4];           /* "VXMC" */
    uint32_t version;        /* Cache format version */
    uint32_t source_hash;    /* Hash of source file content */
    uint64_t timestamp;      /* Source file mtime for validation */
    uint32_t code_size;      /* Size of bytecode in bytes */
    uint32_t num_constants;  /* Number of constants in constant pool */
    uint8_t reserved[36];    /* Padding to 64 bytes */
} VXMCHeader;

/* Cache validation result */
typedef enum {
    CACHE_VALID,             /* Cache is valid and up-to-date */
    CACHE_NOT_FOUND,         /* No cache file exists */
    CACHE_OUTDATED,          /* Source file is newer than cache */
    CACHE_CORRUPTED,         /* Cache file is corrupted */
    CACHE_VERSION_MISMATCH   /* Cache format version mismatch */
} CacheStatus;

/* ═══════════════════════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Check if bytecode cache exists and is valid for given source file */
CacheStatus cache_check_validity(const char* source_path, const char* cache_path);

/* Save compiled chunk to cache file */
bool cache_save_chunk(const char* cache_path, Chunk* chunk, 
                      const char* source_path, uint32_t source_hash);

/* Load compiled chunk from cache file
 * vm - VM for string object reconstruction (required for proper string allocation) */
bool cache_load_chunk(const char* cache_path, Chunk* chunk, VM* vm);

/* Generate cache file path from source path */
/* Returns newly allocated string that caller must free */
char* cache_generate_path(const char* source_path);

/* Compute hash of source file content */
uint32_t cache_compute_source_hash(const char* source);

/* Get file modification time */
uint64_t cache_get_file_mtime(const char* path);

#endif /* VEXIUM_BYTECODE_CACHE_H */
