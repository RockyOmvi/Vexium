/* ═══════════════════════════════════════════════════════════════════════════════
 *  VEXIUM MODULE EXECUTOR - Execute use statements and module imports
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define _POSIX_C_SOURCE 200809L

#include "module_executor.h"
#include "module_loader.h"
#include "parser.h"
#include "lexer.h"
#include "interpreter.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Forward declarations for functions used before definition */
static char* read_file(const char* path);
static VexValue interpret_program_in_env(Interpreter* interp, ASTNode* ast, Environment* env);

/* Global module registry for loaded modules */
static ModuleRegistry* global_registry = NULL;

/* ═══════════════════════════════════════════════════════════════════════════════
 *  MODULE REGISTRY
 * ═══════════════════════════════════════════════════════════════════════════════ */

ModuleRegistry* module_registry_create(void) {
    ModuleRegistry* reg = malloc(sizeof(ModuleRegistry));
    if (!reg) return NULL;
    
    reg->modules = malloc(sizeof(ModuleEntry) * 16);
    reg->capacity = 16;
    reg->count = 0;
    
    return reg;
}

void module_registry_free(ModuleRegistry* reg) {
    if (!reg) return;
    
    for (int i = 0; i < reg->count; i++) {
        free(reg->modules[i].name);
        if (reg->modules[i].exports) {
            for (int j = 0; j < reg->modules[i].export_count; j++) {
                free(reg->modules[i].exports[j].name);
            }
            free(reg->modules[i].exports);
        }
        /* Note: We don't free the environment as it may be in use */
    }
    
    free(reg->modules);
    free(reg);
}

static int find_module(ModuleRegistry* reg, const char* name) {
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->modules[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void add_module(ModuleRegistry* reg, const char* name, Environment* env) {
    if (reg->count >= reg->capacity) {
        reg->capacity *= 2;
        reg->modules = realloc(reg->modules, sizeof(ModuleEntry) * reg->capacity);
    }
    
    ModuleEntry* entry = &reg->modules[reg->count++];
    entry->name = strdup(name);
    entry->env = env;
    entry->exports = NULL;
    entry->export_count = 0;
    entry->export_capacity = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  STANDARD LIBRARY MODULES
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Initialize the math module with all functions */
static void init_math_module(Environment* env) {
    /* Constants */
    env_define(env, "PI", vex_float(3.14159265358979323846), true);
    env_define(env, "E", vex_float(2.71828182845904523536), true);
    
    /* Note: Functions would be defined as native functions here */
    /* For now, the module system loads from .vxm files */
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  MODULE LOADING
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Get the base path for finding modules */
static char* get_module_base_path(void) {
    /* Check environment variable first */
    char* env_path = getenv("VEXIUM_PATH");
    if (env_path) {
        return strdup(env_path);
    }
    
    /* Default to lib/ subdirectory relative to executable */
    return strdup("lib");
}

/* Resolve module name to file path */
static char* resolve_module_path(const char* module_name) {
    char* base = get_module_base_path();
    
    /* Replace dots with path separators */
    char* path = malloc(strlen(base) + strlen(module_name) + 10);
    sprintf(path, "%s/", base);
    
    int path_len = strlen(path);
    for (int i = 0; module_name[i]; i++) {
        if (module_name[i] == '.') {
            path[path_len++] = '/';
        } else {
            path[path_len++] = module_name[i];
        }
    }
    path[path_len] = '\0';
    
    /* Try .vxm extension */
    strcat(path, ".vxm");
    
    free(base);
    return path;
}

/* Check if a standard library module */
static bool is_stdlib_module(const char* name) {
    return strcmp(name, "math") == 0 ||
           strcmp(name, "string") == 0 ||
           strcmp(name, "fs") == 0 ||
           strcmp(name, "json") == 0 ||
           strcmp(name, "time") == 0 ||
           strcmp(name, "collections") == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  MODULE EXECUTION API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Initialize the module system */
void module_system_init(void) {
    if (!global_registry) {
        global_registry = module_registry_create();
    }
}

/* Shutdown the module system */
void module_system_shutdown(void) {
    if (global_registry) {
        module_registry_free(global_registry);
        global_registry = NULL;
    }
}

/* Execute a use statement: use module_name */
bool module_execute_use(const char* module_name, Interpreter* interp, Environment* target_env) {
    if (!global_registry) {
        module_system_init();
    }
    
    /* Check if already loaded */
    int idx = find_module(global_registry, module_name);
    if (idx >= 0) {
        /* Module already loaded, nothing to do */
        return true;
    }
    
    /* Resolve path */
    char* path = resolve_module_path(module_name);
    
    /* Check if file exists */
    FILE* file = fopen(path, "r");
    if (!file && !is_stdlib_module(module_name)) {
        fprintf(stderr, "Module not found: %s (tried %s)\n", module_name, path);
        free(path);
        return false;
    }
    if (file) fclose(file);
    
    /* Create new environment for the module */
    Environment* module_env = env_new(interp->global_env);
    
    /* Parse and execute the module */
    if (file) {
        char* source = read_file(path);
        if (!source) {
            env_release(module_env);
            free(path);
            return false;
        }
        
        Parser parser;
        parser_init(&parser, source);
        ASTNode* ast = parser_parse(&parser);
        
        if (parser.had_error) {
            fprintf(stderr, "Parse error in module %s\n", module_name);
            ast_free(ast);
            free(source);
            env_release(module_env);
            free(path);
            return false;
        }
        
        /* Execute the module */
        interpret(interp, ast, module_env);
        
        ast_free(ast);
        free(source);
    }
    
    /* Register the module */
    add_module(global_registry, module_name, module_env);
    
    /* Import all exports to target environment */
    /* For now, import all public variables */
    for (int i = 0; i < module_env->count; i++) {
        if (!module_env->entries[i].name) continue;
        
        /* Skip private variables (starting with _) */
        if (module_env->entries[i].name[0] == '_') continue;
        
        env_define(target_env, 
                   module_env->entries[i].name,
                   module_env->entries[i].value,
                   module_env->entries[i].is_const);
    }
    
    free(path);
    return true;
}

/* Execute from-use: from module_name use symbol */
bool module_execute_from_use(const char* module_name, const char* symbol, 
                              Interpreter* interp, Environment* target_env) {
    if (!global_registry) {
        module_system_init();
    }
    
    /* Ensure module is loaded */
    int idx = find_module(global_registry, module_name);
    if (idx < 0) {
        /* Load the module */
        if (!module_execute_use(module_name, interp, target_env)) {
            return false;
        }
        idx = find_module(global_registry, module_name);
    }
    
    if (idx < 0) {
        return false;
    }
    
    Environment* module_env = global_registry->modules[idx].env;
    
    /* Find the specific symbol */
    for (int i = 0; i < module_env->count; i++) {
        if (module_env->entries[i].name && 
            strcmp(module_env->entries[i].name, symbol) == 0) {
            env_define(target_env, symbol, 
                      module_env->entries[i].value,
                      module_env->entries[i].is_const);
            return true;
        }
    }
    
    fprintf(stderr, "Symbol '%s' not found in module '%s'\n", symbol, module_name);
    return false;
}

/* Read file helper */
static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    
    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    size_t read = fread(buffer, 1, size, file);
    buffer[read] = '\0';
    fclose(file);
    
    return buffer;
}
