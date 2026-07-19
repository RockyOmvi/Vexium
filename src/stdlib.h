#ifndef VEXIUM_STDLIB_H
#define VEXIUM_STDLIB_H

#include "interpreter.h"

bool stdlib_load_module(const char* module_name, Environment* env);

bool stdlib_load_symbol(const char* module_name, const char* symbol_name, Environment* env);

#endif
