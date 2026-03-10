#define _POSIX_C_SOURCE 200809L

#include "module_loader.h"
#include "parser.h"
#include "lexer.h"
#include "interpreter.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ══════════════════════════════════════════════════════════════
 *  UTILITY FUNCTIONS
 * ══════════════════════════════════════════════════════════════ */

/* String duplication (strdup replacement) */
static char* string_dup(const char* src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char* dup = malloc(len + 1);
    if (dup) {
        strcpy(dup, src);
    }
    return dup;
}

/* Read entire file contents into string */
static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) return NULL;

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* buffer = malloc(file_size + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, file_size, file);
    buffer[bytes_read] = '\0';
    fclose(file);

    return buffer;
}

/* Check if file exists */
static bool file_exists(const char* path) {
    FILE* file = fopen(path, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

/* ══════════════════════════════════════════════════════════════
 *  LOADING STACK (for circular dependency detection)
 * ══════════════════════════════════════════════════════════════ */

LoadingStack* loading_stack_create(void) {
    LoadingStack* stack = malloc(sizeof(LoadingStack));
    stack->capacity = 16;
    stack->modules = malloc(sizeof(char*) * stack->capacity);
    stack->count = 0;
    return stack;
}

void loading_stack_free(LoadingStack* stack) {
    if (!stack) return;
    for (int i = 0; i < stack->count; i++) {
        free(stack->modules[i]);
    }
    free(stack->modules);
    free(stack);
}

void loading_stack_push(LoadingStack* stack, const char* module_name) {
    if (stack->count >= stack->capacity) {
        stack->capacity *= 2;
        stack->modules = realloc(stack->modules, sizeof(char*) * stack->capacity);
    }
    stack->modules[stack->count++] = string_dup(module_name);
}

void loading_stack_pop(LoadingStack* stack) {
    if (stack->count > 0) {
        free(stack->modules[--stack->count]);
    }
}

bool loading_stack_contains(LoadingStack* stack, const char* module_name) {
    for (int i = 0; i < stack->count; i++) {
        if (strcmp(stack->modules[i], module_name) == 0) {
            return true;
        }
    }
    return false;
}

/* Resolve "lib.math" -> "lib/math.vxm" */
char* module_resolve_path(const char* module_name) {
    char buffer[512];
    buffer[0] = '\0';

    /* Replace dots with slashes */
    int pos = 0;
    for (int i = 0; module_name[i]; i++) {
        if (module_name[i] == '.') {
            buffer[pos++] = '/';
        } else {
            buffer[pos++] = module_name[i];
        }
    }
    buffer[pos] = '\0';

    /* Append .vxm extension */
    strcat(buffer, ".vxm");

    /* Check if exists relative to current directory */
    if (file_exists(buffer)) {
        return string_dup(buffer);
    }

    /* Try with "lib/" prefix if not found */
    char with_lib[512] = "lib/";
    strcat(with_lib, buffer);
    if (file_exists(with_lib)) {
        return string_dup(with_lib);
    }

    return NULL;
}

/* ══════════════════════════════════════════════════════════════
 *  MODULE CACHE
 * ══════════════════════════════════════════════════════════════ */

ModuleCache* module_cache_create(void) {
    ModuleCache* cache = malloc(sizeof(ModuleCache));
    cache->root = NULL;
    cache->count = 0;
    return cache;
}

void module_cache_free(ModuleCache* cache) {
    if (!cache) return;

    Module* current = cache->root;
    while (current) {
        Module* next = current->next;

        free(current->name);
        free(current->filepath);
        if (current->exports) {
            env_free((Environment*)current->exports);
        }
        if (current->ast) {
            ast_free((ASTNode*)current->ast);
        }

        free(current);
        current = next;
    }

    free(cache);
}

/* Find module in cache by name */
static Module* module_cache_get(ModuleCache* cache, const char* name) {
    Module* current = cache->root;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/* Add module to cache */
static void module_cache_add(ModuleCache* cache, Module* module) {
    module->next = cache->root;
    cache->root = module;
    cache->count++;
}

/* ══════════════════════════════════════════════════════════════
 *  MODULE LOADING
 * ══════════════════════════════════════════════════════════════ */

/* Forward declare for recursion */
static Module* module_load_impl(ModuleCache* cache, const char* module_name,
                               Interpreter* interp, LoadingStack* loading);

/* Main module loader with circular dependency detection */
Module* module_load(ModuleCache* cache, const char* module_name, Interpreter* interp) {
    /* Check cache first */
    Module* cached = module_cache_get(cache, module_name);
    if (cached) {
        return cached;
    }

    /* Create loading stack for this load session */
    LoadingStack* loading = loading_stack_create();
    Module* result = module_load_impl(cache, module_name, interp, loading);
    loading_stack_free(loading);

    return result;
}

/* NEW: Print the full circular dependency chain */
static void loading_stack_print_cycle(LoadingStack* stack, const char* module_name) {
    fprintf(stderr, "Error: Circular module dependency detected:\n");
    fprintf(stderr, "  ");
    for (int i = 0; i < stack->count; i++) {
        fprintf(stderr, "%s -> ", stack->modules[i]);
    }
    fprintf(stderr, "%s (cycle starts here)\n", module_name);
}

static Module* module_load_impl(ModuleCache* cache, const char* module_name,
                               Interpreter* interp, LoadingStack* loading) {
    /* Detect circular dependency */
    if (loading_stack_contains(loading, module_name)) {
        loading_stack_print_cycle(loading, module_name);
        return NULL;
    }

    /* Check cache */
    Module* cached = module_cache_get(cache, module_name);
    if (cached && cached->is_loaded) {
        return cached;
    }

    /* Resolve module file path */
    char* filepath = module_resolve_path(module_name);
    if (!filepath) {
        fprintf(stderr, "Error: Module not found: %s.vxm\n", module_name);
        return NULL;
    }

    /* Read and parse module source */
    char* source = read_file(filepath);
    if (!source) {
        fprintf(stderr, "Error: Cannot read module file: %s\n", filepath);
        free(filepath);
        return NULL;
    }

    /* Push to loading stack */
    loading_stack_push(loading, module_name);

    /* Parse module */
    Parser parser;
    parser_init(&parser, source);
    ASTNode* program = parser_parse(&parser);

    if (parser.had_error) {
        fprintf(stderr, "Error: Parse error in module %s\n", module_name);
        free(source);
        free(filepath);
        loading_stack_pop(loading);
        return NULL;
    }

    /* Create module */
    Module* module = malloc(sizeof(Module));
    module->name = string_dup(module_name);
    module->filepath = filepath;
    module->exports = (void*)env_new(NULL);  /* Isolated module scope - cast to void* for Module definition */
    module->ast = (void*)program;  /* Keep AST alive for function references */
    module->is_loaded = false;

    /* Add to cache before execution (prevents infinite recursion if module requires itself) */
    module_cache_add(cache, module);

    /* Execute module code in isolated environment */
    if (program && program->type == NODE_PROGRAM) {
        for (int i = 0; i < program->as.program.statements.count; i++) {
            ASTNode* stmt = program->as.program.statements.items[i];
            
            /* Skip module imports (handle separately) */
            if (stmt->type == NODE_USE || stmt->type == NODE_FROM_USE) {
                continue;
            }
            
            /* Execute statement to populate module's exports */
            interpret(interp, stmt, (Environment*)module->exports);
            
            /* Stop on error */
            if (interp->had_error) {
                fprintf(stderr, "Error: Module execution failed in %s\n", module_name);
                free(source);
                if (program) ast_free(program);
                loading_stack_pop(loading);
                return NULL;
            }
        }
    }

    module->is_loaded = true;
    loading_stack_pop(loading);

    free(source);
    /* Don't free program - keep it alive in module->ast for function references */

    return module;
}

/* ══════════════════════════════════════════════════════════════
 *  SYMBOL EXTRACTION
 * ══════════════════════════════════════════════════════════════ */

VexValue module_get_symbol(Module* module, const char* symbol_name) {
    if (!module || !module->is_loaded) {
        return vex_nothing();
    }

    /* Look up symbol in module's exports */
    VexValue* value_ptr = env_get((Environment*)module->exports, symbol_name);
    if (value_ptr) {
        return *value_ptr;
    }

    /* Symbol not found */
    fprintf(stderr, "Error: Symbol '%s' not exported by module '%s'\n",
            symbol_name, module->name);
    return vex_nothing();
}

bool module_is_loaded(ModuleCache* cache, const char* module_name) {
    Module* module = module_cache_get(cache, module_name);
    return module && module->is_loaded;
}

