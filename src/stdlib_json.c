#include "interpreter.h"
#include "stdlib.h"
#include <ctype.h>
#include <math.h>

static VexValue json_stringify(VexValue* args, int argc) {
    if (argc != 1) {
        fprintf(stderr, "Error: json.stringify expects 1 argument\n");
        return vex_nothing();
    }
    char* str = vex_value_to_string(args[0]);
    VexValue res = vex_string(str, (int)strlen(str));
    free(str);
    return res;
}

// Simple recursive JSON parser
static const char* skip_whitespace(const char* p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static VexValue parse_json_value(const char** json_ptr);

static VexValue parse_json_string(const char** json_ptr) {
    const char* p = *json_ptr;
    if (*p != '"') return vex_nothing();
    p++;
    const char* start = p;
    while (*p && *p != '"') {
        if (*p == '\\' && *(p+1)) p += 2;
        else p++;
    }
    int len = (int)(p - start);
    if (*p == '"') p++;
    *json_ptr = p;
    return vex_string(start, len);
}

static VexValue parse_json_number(const char** json_ptr) {
    const char* p = *json_ptr;
    char* end;
    double val = strtod(p, &end);
    *json_ptr = end;
    if (floor(val) == val) {
        return vex_int((int64_t)val);
    }
    return vex_float(val);
}

static VexValue parse_json_object(const char** json_ptr) {
    const char* p = *json_ptr;
    if (*p != '{') return vex_nothing();
    p++;

    VexValue map;
    map.type = VAL_MAP;
    map.as.map_val = (ValueMap*)calloc(1, sizeof(ValueMap));

    p = skip_whitespace(p);
    if (*p == '}') {
        p++;
        *json_ptr = p;
        return map;
    }

    while (*p) {
        p = skip_whitespace(p);
        if (*p != '"') break;
        VexValue key_val = parse_json_string(&p);
        p = skip_whitespace(p);
        if (*p == ':') p++;
        p = skip_whitespace(p);
        VexValue val_val = parse_json_value(&p);

        ValueMap* m = map.as.map_val;
        if (m->count >= m->capacity) {
            m->capacity = m->capacity == 0 ? 4 : m->capacity * 2;
            m->entries = (ValueMapEntry*)realloc(m->entries, sizeof(ValueMapEntry) * m->capacity);
        }
        ValueMapEntry* e = &m->entries[m->count++];
        e->key = vex_strdup(key_val.as.string_val.data, key_val.as.string_val.length);
        e->value = val_val;

        p = skip_whitespace(p);
        if (*p == ',') p++;
        else if (*p == '}') { p++; break; }
    }

    *json_ptr = p;
    return map;
}

static VexValue parse_json_value(const char** json_ptr) {
    const char* p = skip_whitespace(*json_ptr);
    if (*p == '{') return parse_json_object(json_ptr);
    if (*p == '"') return parse_json_string(json_ptr);
    if (isdigit((unsigned char)*p) || *p == '-') return parse_json_number(json_ptr);
    if (strncmp(p, "true", 4) == 0) { *json_ptr = p + 4; return vex_bool(true); }
    if (strncmp(p, "false", 5) == 0) { *json_ptr = p + 5; return vex_bool(false); }
    if (strncmp(p, "null", 4) == 0) { *json_ptr = p + 4; return vex_nothing(); }
    return vex_nothing();
}

static VexValue json_parse(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) {
        fprintf(stderr, "Error: json.parse expects 1 string argument\n");
        return vex_nothing();
    }
    const char* ptr = args[0].as.string_val.data;
    return parse_json_value(&ptr);
}

typedef struct {
    const char* name;
    BuiltinFn func;
} JsonEntry;

static JsonEntry json_entries[] = {
    {"stringify", json_stringify},
    {"parse",     json_parse},
    {NULL, NULL}
};

bool stdlib_load_json_module(Environment* env) {
    for (int i = 0; json_entries[i].name != NULL; i++) {
        VexValue val;
        val.type = VAL_BUILTIN_FN;
        val.as.builtin_fn.func = json_entries[i].func;
        val.as.builtin_fn.name = json_entries[i].name;
        env_define(env, json_entries[i].name, val, true);
    }
    return true;
}
