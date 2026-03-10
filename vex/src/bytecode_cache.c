/* ═══════════════════════════════════════════════════════════════════════════════
 *  VEXIUM BYTECODE CACHE MODULE - Implementation
 *  Save/Load compiled bytecode to/from .vxmc files
 * ═══════════════════════════════════════════════════════════════════════════════ */

#include "bytecode_cache.h"
#include "vm.h"  /* For object allocation */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* External reference to running_vm from vm.c for object allocation */
extern VM* running_vm;

/* Forward declaration for string creation from vm.c */
extern ObjString* obj_string_new(const char* chars, int length);

/* ═══════════════════════════════════════════════════════════════════════════════
 *  HASH FUNCTION (FNV-1a)
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint32_t fnv1a_hash(const uint8_t* data, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  FILE OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════════ */

uint32_t cache_compute_source_hash(const char* source) {
    return fnv1a_hash((const uint8_t*)source, strlen(source));
}

uint64_t cache_get_file_mtime(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    #ifdef _WIN32
    return (uint64_t)st.st_mtime;
    #else
    return (uint64_t)st.st_mtime;
    #endif
}

char* cache_generate_path(const char* source_path) {
    size_t len = strlen(source_path);
    char* cache_path = (char*)malloc(len + 2); /* +2 for 'c' and '\0' */
    if (!cache_path) return NULL;
    
    strcpy(cache_path, source_path);
    
    /* Replace .vxm with .vxmc or append .vxmc */
    if (len > 4 && strcmp(source_path + len - 4, ".vxm") == 0) {
        cache_path[len] = 'c';
        cache_path[len + 1] = '\0';
    } else {
        strcat(cache_path, "c");
    }
    
    return cache_path;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  CACHE VALIDATION
 * ═══════════════════════════════════════════════════════════════════════════════ */

CacheStatus cache_check_validity(const char* source_path, const char* cache_path) {
    FILE* file = fopen(cache_path, "rb");
    if (!file) {
        return CACHE_NOT_FOUND;
    }
    
    /* Read header */
    VXMCHeader header;
    if (fread(&header, sizeof(VXMCHeader), 1, file) != 1) {
        fclose(file);
        return CACHE_CORRUPTED;
    }
    fclose(file);
    
    /* Check magic */
    if (memcmp(header.magic, VXMC_MAGIC, 4) != 0) {
        return CACHE_CORRUPTED;
    }
    
    /* Check version */
    if (header.version != VXMC_VERSION) {
        return CACHE_VERSION_MISMATCH;
    }
    
    /* Check if source file is newer than cache */
    uint64_t source_mtime = cache_get_file_mtime(source_path);
    if (source_mtime > header.timestamp) {
        return CACHE_OUTDATED;
    }
    
    /* Compute and verify source hash */
    FILE* source_file = fopen(source_path, "rb");
    if (!source_file) {
        return CACHE_NOT_FOUND;
    }
    
    fseek(source_file, 0, SEEK_END);
    long source_size = ftell(source_file);
    fseek(source_file, 0, SEEK_SET);
    
    char* source_content = (char*)malloc(source_size + 1);
    if (!source_content) {
        fclose(source_file);
        return CACHE_CORRUPTED;
    }
    
    if (fread(source_content, 1, source_size, source_file) != (size_t)source_size) {
        free(source_content);
        fclose(source_file);
        return CACHE_CORRUPTED;
    }
    source_content[source_size] = '\0';
    fclose(source_file);
    
    uint32_t computed_hash = cache_compute_source_hash(source_content);
    free(source_content);
    
    if (computed_hash != header.source_hash) {
        return CACHE_OUTDATED;
    }
    
    return CACHE_VALID;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  CONSTANT SERIALIZATION (NaN-boxing aware)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    CACHE_NOTHING = 0,
    CACHE_BOOL = 1,
    CACHE_INT = 2,
    CACHE_FLOAT = 3,
    CACHE_STRING = 4
} CacheValueType;

static bool write_value(FILE* file, Value value) {
    /* Determine value type and write tag */
    if (is_nothing(value)) {
        uint8_t tag = CACHE_NOTHING;
        if (fwrite(&tag, 1, 1, file) != 1) return false;
        return true;
    }
    else if (is_bool(value)) {
        uint8_t tag = CACHE_BOOL;
        uint8_t bool_val = as_bool(value) ? 1 : 0;
        if (fwrite(&tag, 1, 1, file) != 1) return false;
        return fwrite(&bool_val, 1, 1, file) == 1;
    }
    else if (is_int(value)) {
        uint8_t tag = CACHE_INT;
        int32_t int_val = as_int(value);
        if (fwrite(&tag, 1, 1, file) != 1) return false;
        return fwrite(&int_val, sizeof(int32_t), 1, file) == 1;
    }
    else if (is_number(value)) {
        uint8_t tag = CACHE_FLOAT;
        double float_val = as_number(value);
        if (fwrite(&tag, 1, 1, file) != 1) return false;
        return fwrite(&float_val, sizeof(double), 1, file) == 1;
    }
    else if (is_obj(value)) {
        /* Only strings are cacheable */
        Obj* obj = (Obj*)as_obj(value);
        if (obj->type == OBJ_STRING) {
            ObjString* str = (ObjString*)obj;
            uint8_t tag = CACHE_STRING;
            uint32_t len = (uint32_t)str->length;
            /* Store hash for faster reconstruction */
            uint32_t hash = str->hash;
            if (fwrite(&tag, 1, 1, file) != 1) return false;
            if (fwrite(&len, sizeof(uint32_t), 1, file) != 1) return false;
            if (fwrite(&hash, sizeof(uint32_t), 1, file) != 1) return false;
            return fwrite(str->chars, 1, len, file) == len;
        }
    }
    
    /* Unknown type - write as nothing */
    uint8_t tag = CACHE_NOTHING;
    fwrite(&tag, 1, 1, file);
    return true;
}

/* Reconstruct a string object with proper VM context for GC tracking
 * This function temporarily sets running_vm to ensure proper object allocation */
static ObjString* reconstruct_string_with_vm(VM* vm, const char* chars, int length, uint32_t hash) {
    /* Save current running_vm */
    VM* saved_vm = running_vm;
    
    /* Set running_vm to allow allocate_obj to track this object */
    running_vm = vm;
    
    /* Create string using existing obj_string_new function */
    ObjString* str = obj_string_new(chars, length);
    
    /* Use the pre-computed hash from the cache */
    str->hash = hash;
    
    /* Restore running_vm */
    running_vm = saved_vm;
    
    return str;
}

static bool read_value(FILE* file, Value* value, VM* vm) {
    uint8_t tag;
    if (fread(&tag, 1, 1, file) != 1) return false;
    
    switch (tag) {
        case CACHE_NOTHING:
            *value = val_nothing_v();
            return true;
            
        case CACHE_BOOL: {
            uint8_t bool_val;
            if (fread(&bool_val, 1, 1, file) != 1) return false;
            *value = val_bool(bool_val != 0);
            return true;
        }
        
        case CACHE_INT: {
            int32_t int_val;
            if (fread(&int_val, sizeof(int32_t), 1, file) != 1) return false;
            *value = val_int(int_val);
            return true;
        }
        
        case CACHE_FLOAT: {
            double float_val;
            if (fread(&float_val, sizeof(double), 1, file) != 1) return false;
            *value = val_number(float_val);
            return true;
        }
        
        case CACHE_STRING: {
            uint32_t len;
            uint32_t hash;
            if (fread(&len, sizeof(uint32_t), 1, file) != 1) return false;
            if (fread(&hash, sizeof(uint32_t), 1, file) != 1) return false;
            
            char* str_buf = (char*)malloc(len + 1);
            if (!str_buf) return false;
            if (fread(str_buf, 1, len, file) != len) {
                free(str_buf);
                return false;
            }
            str_buf[len] = '\0';
            
            /* Create string object using provided VM context */
            if (vm) {
                ObjString* obj_str = reconstruct_string_with_vm(vm, str_buf, (int)len, hash);
                if (obj_str) {
                    *value = val_obj(obj_str);
                } else {
                    /* String reconstruction failed - use nothing but report error */
                    *value = val_nothing_v();
                    fprintf(stderr, "Warning: Failed to reconstruct cached string\n");
                }
            } else {
                /* No VM context - this is an error condition */
                fprintf(stderr, "Error: Cannot load cached strings without VM context\n");
                *value = val_nothing_v();
            }
            free(str_buf);
            return true;
        }
        
        default:
            *value = val_nothing_v();
            return true;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  SAVE/LOAD CHUNK
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool cache_save_chunk(const char* cache_path, Chunk* chunk, 
                      const char* source_path, uint32_t source_hash) {
    FILE* file = fopen(cache_path, "wb");
    if (!file) return false;
    
    /* Prepare header */
    VXMCHeader header;
    memset(&header, 0, sizeof(VXMCHeader));
    memcpy(header.magic, VXMC_MAGIC, 4);
    header.version = VXMC_VERSION;
    header.source_hash = source_hash;
    header.timestamp = cache_get_file_mtime(source_path);
    header.code_size = chunk->count;
    header.num_constants = chunk->const_count;
    
    /* Write header */
    if (fwrite(&header, sizeof(VXMCHeader), 1, file) != 1) {
        fclose(file);
        return false;
    }
    
    /* Write bytecode */
    if (fwrite(chunk->code, 1, chunk->count, file) != (size_t)chunk->count) {
        fclose(file);
        return false;
    }
    
    /* Write constants */
    for (int i = 0; i < chunk->const_count; i++) {
        if (!write_value(file, chunk->constants[i])) {
            fclose(file);
            return false;
        }
    }
    
    /* Write line info */
    uint32_t line_count = chunk->count;
    if (fwrite(&line_count, sizeof(uint32_t), 1, file) != 1) {
        fclose(file);
        return false;
    }
    
    for (uint32_t i = 0; i < line_count; i++) {
        if (fwrite(&chunk->lines[i], sizeof(int), 1, file) != 1) {
            fclose(file);
            return false;
        }
    }
    
    fclose(file);
    return true;
}

bool cache_load_chunk(const char* cache_path, Chunk* chunk, VM* vm) {
    FILE* file = fopen(cache_path, "rb");
    if (!file) return false;
    
    /* Read header */
    VXMCHeader header;
    if (fread(&header, sizeof(VXMCHeader), 1, file) != 1) {
        fclose(file);
        return false;
    }
    
    /* Initialize chunk */
    chunk_init(chunk);
    
    /* Read bytecode */
    chunk->code = (uint8_t*)malloc(header.code_size);
    if (!chunk->code) {
        fclose(file);
        return false;
    }
    
    if (fread(chunk->code, 1, header.code_size, file) != header.code_size) {
        free(chunk->code);
        fclose(file);
        return false;
    }
    chunk->count = header.code_size;
    chunk->capacity = header.code_size;
    
    /* Read constants - pass VM for proper string reconstruction */
    for (uint32_t i = 0; i < header.num_constants; i++) {
        Value val;
        if (!read_value(file, &val, (VM*)vm)) {
            chunk_free(chunk);
            fclose(file);
            return false;
        }
        chunk_add_constant(chunk, val);
    }
    
    /* Read line info */
    uint32_t line_count;
    if (fread(&line_count, sizeof(uint32_t), 1, file) != 1) {
        chunk_free(chunk);
        fclose(file);
        return false;
    }
    
    chunk->lines = (int*)malloc(sizeof(int) * line_count);
    if (!chunk->lines) {
        chunk_free(chunk);
        fclose(file);
        return false;
    }
    
    for (uint32_t i = 0; i < line_count; i++) {
        if (fread(&chunk->lines[i], sizeof(int), 1, file) != 1) {
            chunk_free(chunk);
            fclose(file);
            return false;
        }
    }
    
    fclose(file);
    return true;
}
