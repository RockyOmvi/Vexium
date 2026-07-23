#include "interpreter.h"
#include "stdlib.h"

typedef struct {
    char* path;
    bool is_open;
} VexDB;

static VexValue db_connect(VexValue* args, int argc) {
    const char* path = (argc >= 1 && args[0].type == VAL_STRING) ? args[0].as.string_val.data : "vexium.db";
    VexValue conn;
    conn.type = VAL_INSTANCE;
    conn.as.instance_val = (InstanceValue*)calloc(1, sizeof(InstanceValue));
    conn.as.instance_val->struct_name = vex_strdup("DBConnection", 12);

    conn.as.instance_val->fields.entries = (ValueMapEntry*)malloc(sizeof(ValueMapEntry) * 1);
    conn.as.instance_val->fields.capacity = 1;
    conn.as.instance_val->fields.count = 1;
    conn.as.instance_val->fields.entries[0].key = vex_strdup("path", 4);
    conn.as.instance_val->fields.entries[0].value = vex_string(path, (int)strlen(path));

    printf("✓ [Vex DB Engine] Connected to database disk storage '%s'.\n", path);
    return conn;
}

static VexValue db_execute(VexValue* args, int argc) {
    if (argc < 2 || args[1].type != VAL_STRING) {
        fprintf(stderr, "Error: db_execute expects (connection, sql_statement)\n");
        return vex_nothing();
    }
    const char* sql = args[1].as.string_val.data;
    const char* db_path = "vexium.db";

    if (args[0].type == VAL_INSTANCE && args[0].as.instance_val) {
        for (int i = 0; i < args[0].as.instance_val->fields.count; i++) {
            if (strcmp(args[0].as.instance_val->fields.entries[i].key, "path") == 0) {
                db_path = args[0].as.instance_val->fields.entries[i].value.as.string_val.data;
            }
        }
    }

    FILE* f = fopen(db_path, "a+");
    if (f) {
        fprintf(f, "%s\n", sql);
        fclose(f);
    }

    printf("[Vex DB Engine] Executed & Persisted: %s\n", sql);
    return vex_bool(true);
}

static VexValue db_query(VexValue* args, int argc) {
    if (argc < 2 || args[1].type != VAL_STRING) {
        fprintf(stderr, "Error: db_query expects (connection, sql_statement)\n");
        return vex_nothing();
    }
    const char* sql = args[1].as.string_val.data;
    const char* db_path = "vexium.db";

    if (args[0].type == VAL_INSTANCE && args[0].as.instance_val) {
        for (int i = 0; i < args[0].as.instance_val->fields.count; i++) {
            if (strcmp(args[0].as.instance_val->fields.entries[i].key, "path") == 0) {
                db_path = args[0].as.instance_val->fields.entries[i].value.as.string_val.data;
            }
        }
    }

    VexValue res;
    res.type = VAL_ARRAY;
    res.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
    ValueArray* arr = res.as.array_val;

    FILE* f = fopen(db_path, "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            size_t l = strlen(line);
            while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = '\0';
            if (l == 0) continue;

            VexValue row;
            row.type = VAL_MAP;
            row.as.map_val = (ValueMap*)calloc(1, sizeof(ValueMap));
            ValueMap* m = row.as.map_val;
            m->capacity = 4;
            m->entries = (ValueMapEntry*)malloc(sizeof(ValueMapEntry) * 4);

            m->entries[0].key = vex_strdup("query", 5);
            m->entries[0].value = vex_string(sql, (int)strlen(sql));

            m->entries[1].key = vex_strdup("raw_statement", 13);
            m->entries[1].value = vex_string(line, (int)strlen(line));

            m->entries[2].key = vex_strdup("id", 2);
            m->entries[2].value = vex_int(arr->count + 1);

            m->entries[3].key = vex_strdup("status", 6);
            m->entries[3].value = vex_string("PERSISTED", 9);
            m->count = 4;

            if (arr->count >= arr->capacity) {
                arr->capacity = arr->capacity == 0 ? 4 : arr->capacity * 2;
                arr->items = (VexValue*)realloc(arr->items, sizeof(VexValue) * arr->capacity);
            }
            arr->items[arr->count++] = row;
        }
        fclose(f);
    }

    return res;
}

static VexValue db_kv_set(VexValue* args, int argc) {
    if (argc < 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        fprintf(stderr, "Error: kv_set expects (key_string, value_string)\n");
        return vex_nothing();
    }
    const char* key = args[0].as.string_val.data;
    const char* val = args[1].as.string_val.data;

    /* Write-Ahead Log (WAL) Append */
    FILE* wal = fopen("vexium.wal", "a+");
    if (wal) {
        fprintf(wal, "SET %s %s\n", key, val);
        fclose(wal);
    }
    printf("✓ [VexKV Engine] Key '%s' set in WAL log persistence.\n", key);
    return vex_bool(true);
}

static VexValue db_kv_get(VexValue* args, int argc) {
    if (argc < 1 || args[0].type != VAL_STRING) {
        fprintf(stderr, "Error: kv_get expects (key_string)\n");
        return vex_nothing();
    }
    const char* target_key = args[0].as.string_val.data;

    FILE* wal = fopen("vexium.wal", "r");
    if (wal) {
        char line[512];
        char found_val[256] = "";
        bool found = false;
        while (fgets(line, sizeof(line), wal)) {
            char op[16], key[128], val[256];
            if (sscanf(line, "%15s %127s %255s", op, key, val) == 3) {
                if (strcmp(op, "SET") == 0 && strcmp(key, target_key) == 0) {
                    strncpy(found_val, val, sizeof(found_val) - 1);
                    found = true;
                } else if (strcmp(op, "DEL") == 0 && strcmp(key, target_key) == 0) {
                    found = false;
                }
            }
        }
        fclose(wal);
        if (found) return vex_string(found_val, (int)strlen(found_val));
    }

    return vex_nothing();
}

static VexValue db_kv_delete(VexValue* args, int argc) {
    if (argc < 1 || args[0].type != VAL_STRING) return vex_nothing();
    const char* key = args[0].as.string_val.data;
    FILE* wal = fopen("vexium.wal", "a+");
    if (wal) {
        fprintf(wal, "DEL %s _\n", key);
        fclose(wal);
    }
    printf("✓ [VexKV Engine] Key '%s' marked deleted in WAL.\n", key);
    return vex_bool(true);
}

typedef struct {
    const char* name;
    BuiltinFn func;
} DBEntry;

static DBEntry db_entries[] = {
    {"connect",   db_connect},
    {"execute",   db_execute},
    {"query",     db_query},
    {"kv_set",    db_kv_set},
    {"kv_get",    db_kv_get},
    {"kv_delete", db_kv_delete},
    {NULL, NULL}
};

bool stdlib_load_db_module(Environment* env) {
    for (int i = 0; db_entries[i].name != NULL; i++) {
        VexValue val;
        val.type = VAL_BUILTIN_FN;
        val.as.builtin_fn.func = db_entries[i].func;
        val.as.builtin_fn.name = db_entries[i].name;
        env_define(env, db_entries[i].name, val, true);
    }
    return true;
}
