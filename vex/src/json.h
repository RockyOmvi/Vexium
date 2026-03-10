#ifndef VEXIUM_JSON_H
#define VEXIUM_JSON_H

#include "vm.h"

/* JSON parsing and stringification for Vexium */

/* Parse a JSON string into a Vexium Value */
Value json_parse_string_value(const char* json_str);

/* Convert a Vexium Value to a JSON string (caller must free) */
char* json_stringify(Value value);

/* Register JSON module natives with the VM */
void vm_register_json_module(VM* vm);

#endif /* VEXIUM_JSON_H */
