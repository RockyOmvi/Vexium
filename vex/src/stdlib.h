#ifndef VEXIUM_STDLIB_H
#define VEXIUM_STDLIB_H

#include "interpreter.h"

/* ══════════════════════════════════════════════════════════════
 *  STANDARD LIBRARY MODULE SYSTEM
 *
 *  Modules: math, string, file, sys
 *
 *  Usage in Vexium:
 *    use math           # loads all math functions
 *    from math use sqrt # loads only sqrt
 * ══════════════════════════════════════════════════════════════ */

/* Register a standard library module into the given environment.
 * Returns true if the module was found, false otherwise. */
bool stdlib_load_module(const char* module_name, Environment* env);

/* Register a single function from a module.
 * Returns true if found, false otherwise. */
bool stdlib_load_symbol(const char* module_name, const char* symbol_name, Environment* env);

#endif /* VEXIUM_STDLIB_H */
