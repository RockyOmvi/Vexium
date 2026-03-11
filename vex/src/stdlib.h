#ifndef VEXIUM_STDLIB_H
#define VEXIUM_STDLIB_H

/* Include stdbool first to avoid conflicts with system stdlib.h */
#ifdef _WIN32
#ifndef __STDBOOL_H
#define __STDBOOL_H
#ifndef bool
typedef int bool;
#define true 1
#define false 0
#endif
#endif
#else
#include <stdbool.h>
#endif

#include "common.h"

/* Forward declarations to avoid circular dependency */
typedef struct Environment Environment;

/* ════════════════════════════════════════════════════════════════
 *  STANDARD LIBRARY MODULE SYSTEM
 *
 *  Modules: math, string, file, sys, time, json, http,
 *           collections, algo, data, gpu, ai
 *
 *  Usage in Vexium:
 *    use math           # loads all math functions
 *    from math use sqrt # loads only sqrt
 * ════════════════════════════════════════════════════════════════ */

/* Register a standard library module into the given environment.
 * Returns true if the module was found, false otherwise. */
bool stdlib_load_module(const char* module_name, Environment* env);

/* Register a single function from a module.
 * Returns true if found, false otherwise. */
bool stdlib_load_symbol(const char* module_name, const char* symbol_name, Environment* env);

#endif /* VEXIUM_STDLIB_H */
