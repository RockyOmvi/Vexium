/* VEXIUM MODULE EXECUTOR - Header */

#ifndef VEXIUM_MODULE_EXECUTOR_H
#define VEXIUM_MODULE_EXECUTOR_H

#include "interpreter.h"

/* Module export entry */
typedef struct {
    char* name;
    VexValue value;
    bool is_public;
} ExportEntry;

/* Loaded module entry */
typedef struct {
    char* name;
    Environment* env;
    ExportEntry* exports;
    int export_count;
    int export_capacity;
} ModuleEntry;

/* Module registry */
typedef struct {
    ModuleEntry* modules;
    int count;
    int capacity;
} ModuleRegistry;

/* Initialize module system */
void module_system_init(void);
void module_system_shutdown(void);

/* Execute use statement */
bool module_execute_use(const char* module_name, Interpreter* interp, Environment* target_env);

/* Execute from-use statement */
bool module_execute_from_use(const char* module_name, const char* symbol, 
                              Interpreter* interp, Environment* target_env);

#endif
