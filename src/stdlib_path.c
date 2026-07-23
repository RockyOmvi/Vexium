#include "interpreter.h"
#include "stdlib.h"

static VexValue path_join(VexValue* args, int argc) {
    if (argc < 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        fprintf(stderr, "Error: path.join expects 2 string arguments\n");
        return vex_nothing();
    }
    const char* p1 = args[0].as.string_val.data;
    const char* p2 = args[1].as.string_val.data;
    char buf[512];
    snprintf(buf, sizeof(buf), "%s/%s", p1, p2);
    return vex_string(buf, (int)strlen(buf));
}

static VexValue path_basename(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) {
        fprintf(stderr, "Error: path.basename expects 1 string argument\n");
        return vex_nothing();
    }
    const char* path = args[0].as.string_val.data;
    const char* base = strrchr(path, '/');
    if (!base) base = strrchr(path, '\\');
    base = base ? base + 1 : path;
    return vex_string(base, (int)strlen(base));
}

typedef struct {
    const char* name;
    BuiltinFn func;
} PathEntry;

static PathEntry path_entries[] = {
    {"join",     path_join},
    {"basename", path_basename},
    {NULL, NULL}
};

bool stdlib_load_path_module(Environment* env) {
    for (int i = 0; path_entries[i].name != NULL; i++) {
        VexValue val;
        val.type = VAL_BUILTIN_FN;
        val.as.builtin_fn.func = path_entries[i].func;
        val.as.builtin_fn.name = path_entries[i].name;
        env_define(env, path_entries[i].name, val, true);
    }
    return true;
}
