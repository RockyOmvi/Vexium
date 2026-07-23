#include "interpreter.h"
#include "stdlib.h"

static VexValue sys_allocate(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_INT) {
        fprintf(stderr, "Error: allocate expects 1 integer byte count argument\n");
        return vex_nothing();
    }
    size_t size = (size_t)args[0].as.int_val;
    void* ptr = malloc(size);
    return vex_int((intptr_t)ptr);
}

static VexValue sys_free_mem(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_INT) {
        fprintf(stderr, "Error: free_memory expects 1 pointer integer argument\n");
        return vex_nothing();
    }
    void* ptr = (void*)(intptr_t)args[0].as.int_val;
    if (ptr) free(ptr);
    return vex_nothing();
}

static VexValue sys_write_memory(VexValue* args, int argc) {
    if (argc != 3 || args[0].type != VAL_INT || args[1].type != VAL_INT || args[2].type != VAL_INT) {
        fprintf(stderr, "Error: write_memory expects (address, offset, byte_value)\n");
        return vex_nothing();
    }
    uint8_t* ptr = (uint8_t*)(intptr_t)args[0].as.int_val;
    int offset = (int)args[1].as.int_val;
    uint8_t val = (uint8_t)args[2].as.int_val;
    if (ptr) {
        ptr[offset] = val;
        return vex_bool(true);
    }
    return vex_bool(false);
}

static VexValue sys_read_memory(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_INT || args[1].type != VAL_INT) {
        fprintf(stderr, "Error: read_memory expects (address, offset)\n");
        return vex_nothing();
    }
    uint8_t* ptr = (uint8_t*)(intptr_t)args[0].as.int_val;
    int offset = (int)args[1].as.int_val;
    if (ptr) {
        return vex_int(ptr[offset]);
    }
    return vex_nothing();
}

typedef struct {
    const char* name;
    BuiltinFn func;
} SystemEntry;

static SystemEntry system_entries[] = {
    {"allocate",     sys_allocate},
    {"free_memory",  sys_free_mem},
    {"write_memory", sys_write_memory},
    {"read_memory",  sys_read_memory},
    {NULL, NULL}
};

bool stdlib_load_system_module(Environment* env) {
    for (int i = 0; system_entries[i].name != NULL; i++) {
        VexValue val;
        val.type = VAL_BUILTIN_FN;
        val.as.builtin_fn.func = system_entries[i].func;
        val.as.builtin_fn.name = system_entries[i].name;
        env_define(env, system_entries[i].name, val, true);
    }
    return true;
}
