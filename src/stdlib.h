#ifndef VEXIUM_STDLIB_H
#define VEXIUM_STDLIB_H

#include "interpreter.h"

bool stdlib_load_module(const char* module_name, Environment* env);
bool stdlib_load_symbol(const char* module_name, const char* symbol_name, Environment* env);

bool stdlib_load_ai_module(Environment* env);
bool stdlib_load_network_module(Environment* env);
bool stdlib_load_system_module(Environment* env);
bool stdlib_load_json_module(Environment* env);
bool stdlib_load_path_module(Environment* env);
bool stdlib_load_db_module(Environment* env);
bool stdlib_load_web_module(Environment* env);
bool stdlib_load_crypto_module(Environment* env);
bool stdlib_load_ffi_module(Environment* env);
bool stdlib_load_gui_module(Environment* env);

#endif
