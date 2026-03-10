#ifndef VEXIUM_MODULE_LOADER_H
#define VEXIUM_MODULE_LOADER_H

#include "common.h"
#include "interpreter.h"  /* Include for full type definitions */

/* ══════════════════════════════════════════════════════════════
 *  MODULE LOADER - Load and cache user-defined .vxm modules
 * ══════════════════════════════════════════════════════════════ */

typedef struct Module {
    char* name;
    char* filepath;
    void* exports;
    void* ast;
    bool is_loaded;
    struct Module* next;
} Module;

typedef struct ModuleCache {
    Module* root;
    int count;
} ModuleCache;

/* Initialize module cache */
ModuleCache* module_cache_create(void);

/* Free all cached modules */
void module_cache_free(ModuleCache* cache);

/* Load a user-defined module or return cached version */
Module* module_load(ModuleCache* cache, const char* module_name, Interpreter* interp);

/* Get a public symbol from a loaded module */
VexValue module_get_symbol(Module* module, const char* symbol_name);

/* Check if module has been loaded */
bool module_is_loaded(ModuleCache* cache, const char* module_name);

/* Resolve module name to file path (e.g., "lib.math" -> "lib/math.vxm") */
char* module_resolve_path(const char* module_name);

/* Check for circular dependencies */
typedef struct {
    char** modules;
    int count;
    int capacity;
} LoadingStack;

LoadingStack* loading_stack_create(void);
void loading_stack_free(LoadingStack* stack);
void loading_stack_push(LoadingStack* stack, const char* module_name);
void loading_stack_pop(LoadingStack* stack);
bool loading_stack_contains(LoadingStack* stack, const char* module_name);

#endif

