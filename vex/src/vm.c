#include "vm.h"
#include "opcodes.h"
#include "task.h"
#include "json.h"
#include "time_module.h"
#include "http_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>

/* ══════════════════════════════════════════════════════════════
 *  OBJECT ALLOCATION HELPERS
 * ══════════════════════════════════════════════════════════════ */

/* Global VM pointer for object tracking (set during vm_run) */
VM* running_vm = NULL;

static Obj* allocate_obj(size_t size, ObjType type) {
    Obj* obj = (Obj*)malloc(size);
    obj->type = type;
    obj->next = running_vm ? running_vm->objects : NULL;
    if (running_vm) running_vm->objects = obj;
    return obj;
}

uint32_t hash_string(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619u;
    }
    return hash;
}

ObjString* obj_string_new(const char* chars, int length) {
    ObjString* str = (ObjString*)allocate_obj(
        sizeof(ObjString) + length + 1, OBJ_STRING);
    str->length = length;
    str->hash = hash_string(chars, length);
    memcpy(str->chars, chars, length);
    str->chars[length] = '\0';
    return str;
}

ObjFunction* obj_function_new(const char* name) {
    ObjFunction* fn = (ObjFunction*)allocate_obj(sizeof(ObjFunction), OBJ_FUNCTION);
    fn->arity = 0;
    fn->upvalue_count = 0;
    if (name) {
        int len = (int)strlen(name);
        fn->name = (char*)malloc(len + 1);
        memcpy(fn->name, name, len + 1);
    } else {
        fn->name = NULL;
    }
    fn->chunk_count = 0;
    fn->chunk_capacity = 0;
    fn->code = NULL;
    fn->lines = NULL;
    fn->const_count = 0;
    fn->const_capacity = 0;
    fn->constants = NULL;
    return fn;
}

ObjArray* obj_array_new(int capacity) {
    ObjArray* arr = (ObjArray*)allocate_obj(sizeof(ObjArray), OBJ_ARRAY);
    arr->count = 0;
    arr->capacity = capacity < 8 ? 8 : capacity;
    arr->items = (Value*)malloc(arr->capacity * sizeof(Value));
    return arr;
}

ObjMap* obj_map_new(void) {
    ObjMap* map = (ObjMap*)allocate_obj(sizeof(ObjMap), OBJ_MAP);
    map->count = 0;
    map->capacity = 0;
    map->entries = NULL;
    return map;
}

void obj_map_set(ObjMap* map, ObjString* key, Value value) {
    if (map->count + 1 > map->capacity * 0.75) {
        int old_capacity = map->capacity;
        map->capacity = map->capacity < 8 ? 8 : map->capacity * 2;
        VMMapEntry* entries = calloc(map->capacity, sizeof(VMMapEntry));
        for (int i = 0; i < old_capacity; i++) {
            if (!map->entries[i].occupied) continue;
            uint32_t idx = map->entries[i].key->hash % map->capacity;
            while (entries[idx].occupied) {
                idx = (idx + 1) % map->capacity;
            }
            entries[idx] = map->entries[i];
        }
        free(map->entries);
        map->entries = entries;
    }
    
    uint32_t idx = key->hash % map->capacity;
    while (map->entries[idx].occupied && map->entries[idx].key != key) {
        idx = (idx + 1) % map->capacity;
    }
    
    if (!map->entries[idx].occupied) {
        map->count++;
        map->entries[idx].occupied = true;
        map->entries[idx].key = key;
    }
    map->entries[idx].value = value;
}

bool obj_map_get(ObjMap* map, ObjString* key, Value* out) {
    if (map->count == 0) return false;
    uint32_t idx = key->hash % map->capacity;
    for (int i = 0; i < map->capacity; i++) {
        int slot = (idx + i) % map->capacity;
        if (!map->entries[slot].occupied) return false;
        if (map->entries[slot].key->hash == key->hash &&
            map->entries[slot].key->length == key->length &&
            memcmp(map->entries[slot].key->chars, key->chars, key->length) == 0) {
            *out = map->entries[slot].value;
            return true;
        }
    }
    return false;
}

ObjStructDef* obj_struct_def_new(ObjString* name, int field_count) {
    ObjStructDef* def = (ObjStructDef*)allocate_obj(sizeof(ObjStructDef), OBJ_STRUCT_DEF);
    def->name = name;
    def->field_count = field_count;
    def->field_names = (ObjString**)calloc(field_count, sizeof(ObjString*));
    def->methods = obj_map_new();
    return def;
}

ObjInstance* obj_instance_new(ObjStructDef* def) {
    ObjInstance* inst = (ObjInstance*)allocate_obj(sizeof(ObjInstance), OBJ_INSTANCE);
    inst->struct_def = def;
    inst->fields = obj_map_new();
    return inst;
}

ObjClosure* obj_closure_new(ObjFunction* function) {
    ObjClosure* closure = (ObjClosure*)allocate_obj(sizeof(ObjClosure), OBJ_CLOSURE);
    closure->function = function;
    closure->upvalue_count = function->upvalue_count;
    if (closure->upvalue_count > 0) {
        closure->upvalues = (ObjUpvalue**)calloc(closure->upvalue_count, sizeof(ObjUpvalue*));
    } else {
        closure->upvalues = NULL;
    }
    return closure;
}

static ObjUpvalue* obj_upvalue_new(Value* slot) {
    ObjUpvalue* up = (ObjUpvalue*)allocate_obj(sizeof(ObjUpvalue), OBJ_UPVALUE);
    up->location = slot;
    up->closed = val_nothing_v();
    up->next = NULL;
    return up;
}

static ObjUpvalue* capture_upvalue(VM* vm, Value* local) {
    ObjUpvalue* prev = NULL;
    ObjUpvalue* up = vm->open_upvalues;

    while (up != NULL && up->location > local) {
        prev = up;
        up = up->next;
    }

    if (up != NULL && up->location == local) {
        return up;
    }

    ObjUpvalue* created = obj_upvalue_new(local);
    created->next = up;
    if (prev == NULL) {
        vm->open_upvalues = created;
    } else {
        prev->next = created;
    }
    return created;
}

static void close_upvalues(VM* vm, Value* last) {
    while (vm->open_upvalues != NULL && vm->open_upvalues->location >= last) {
        ObjUpvalue* up = vm->open_upvalues;
        up->closed = *up->location;
        up->location = &up->closed;
        vm->open_upvalues = up->next;
    }
}

static int map_entry_ptr_cmp(const void* a, const void* b) {
    const VMMapEntry* ea = *(const VMMapEntry* const*)a;
    const VMMapEntry* eb = *(const VMMapEntry* const*)b;
    return strcmp(ea->key->chars, eb->key->chars);
}

static ObjTaskHandle* obj_task_handle_new(Task* task) {
    ObjTaskHandle* handle = (ObjTaskHandle*)allocate_obj(sizeof(ObjTaskHandle), OBJ_TASK_HANDLE);
    handle->task = task;
    handle->completed = false;
    handle->result = val_nothing_v();
    return handle;
}

/* Chunk helpers */
void chunk_init(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    chunk->const_count = 0;
    chunk->const_capacity = 0;
    chunk->constants = NULL;
}

void chunk_write(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->count >= chunk->capacity) {
        int old = chunk->capacity;
        chunk->capacity = old < 8 ? 8 : old * 2;
        chunk->code = realloc(chunk->code, chunk->capacity);
        chunk->lines = realloc(chunk->lines, chunk->capacity * sizeof(int));
    }
    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int chunk_add_constant(Chunk* chunk, Value value) {
    if (chunk->const_count >= chunk->const_capacity) {
        int old = chunk->const_capacity;
        chunk->const_capacity = old < 8 ? 8 : old * 2;
        chunk->constants = realloc(chunk->constants, chunk->const_capacity * sizeof(Value));
    }
    chunk->constants[chunk->const_count] = value;
    return chunk->const_count++;
}

void chunk_free(Chunk* chunk) {
    free(chunk->code);
    free(chunk->lines);
    free(chunk->constants);
    chunk_init(chunk);
}

/* ══════════════════════════════════════════════════════════════
 *  VALUE PRINTING & UTILITY
 * ══════════════════════════════════════════════════════════════ */

void value_print(Value val) {
    if (is_number(val)) {
        double d = as_number(val);
        if (d == (int64_t)d && fabs(d) < 1e15) {
            printf("%lld", (long long)(int64_t)d);
        } else {
            printf("%g", d);
        }
    } else if (is_int(val)) {
        printf("%d", as_int(val));
    } else if (is_bool(val)) {
        printf(as_bool(val) ? "true" : "false");
    } else if (is_nothing(val)) {
        /* Don't print 'nothing' for implicit returns - Python style */
    } else if (is_obj(val)) {
        Obj* obj = (Obj*)as_obj(val);
        switch (obj->type) {
            case OBJ_STRING:
                printf("%s", ((ObjString*)obj)->chars);
                break;
            case OBJ_FUNCTION: {
                ObjFunction* fn = (ObjFunction*)obj;
                if (fn->name) {
                    printf("<fn %s>", fn->name);
                } else {
                    printf("<fn>");
                }
                break;
            }
            case OBJ_ARRAY: {
                ObjArray* arr = (ObjArray*)obj;
                printf("[");
                for (int i = 0; i < arr->count; i++) {
                    if (i > 0) printf(", ");
                    /* Quote strings in arrays */
                    if (is_obj(arr->items[i])) {
                        Obj* item_obj = (Obj*)as_obj(arr->items[i]);
                        if (item_obj->type == OBJ_STRING) {
                            printf("\"%s\"", ((ObjString*)item_obj)->chars);
                            continue;
                        }
                    }
                    value_print(arr->items[i]);
                }
                printf("]");
                break;
            }
            case OBJ_MAP: {
                ObjMap* map = (ObjMap*)obj;
                printf("{");
                int printed = 0;
                VMMapEntry** ordered = NULL;
                if (map->count > 0) {
                    ordered = (VMMapEntry**)malloc(sizeof(VMMapEntry*) * map->count);
                    int at = 0;
                    for (int i = 0; i < map->capacity; i++) {
                        if (map->entries[i].occupied) {
                            ordered[at++] = &map->entries[i];
                        }
                    }
                    qsort(ordered, map->count, sizeof(VMMapEntry*), map_entry_ptr_cmp);
                }
                for (int i = 0; i < map->count; i++) {
                    if (printed > 0) printf(", ");
                    printf("\"%s\": ", ordered[i]->key->chars);
                    value_print(ordered[i]->value);
                    printed++;
                }
                free(ordered);
                printf("}");
                break;
            }
            case OBJ_CLOSURE: {
                ObjClosure* cl = (ObjClosure*)obj;
                if (cl->function->name) {
                    printf("<fn %s>", cl->function->name);
                } else {
                    printf("<fn>");
                }
                break;
            }
            case OBJ_INSTANCE: {
                ObjInstance* inst = (ObjInstance*)obj;
                if (inst->struct_def && inst->struct_def->name) {
                    printf("<%s instance>", inst->struct_def->name->chars);
                } else {
                    printf("<instance>");
                }
                break;
            }
            case OBJ_TASK_HANDLE:
                printf("<task>");
                break;
            default:
                printf("<obj>");
                break;
        }
    }
}

bool value_is_truthy(Value val) {
    if (is_nothing(val)) return false;
    if (is_bool(val)) return as_bool(val);
    if (is_int(val)) return as_int(val) != 0;
    if (is_number(val)) return as_number(val) != 0.0;
    if (is_obj(val)) {
        Obj* obj = (Obj*)as_obj(val);
        if (obj->type == OBJ_STRING) return ((ObjString*)obj)->length > 0;
        if (obj->type == OBJ_ARRAY) return ((ObjArray*)obj)->count > 0;
        return true;
    }
    return true;
}

bool values_equal(Value a, Value b) {
    if (is_number(a) && is_number(b)) return as_number(a) == as_number(b);
    if (is_int(a) && is_int(b)) return as_int(a) == as_int(b);
    /* int == number coercion */
    if (is_int(a) && is_number(b)) return (double)as_int(a) == as_number(b);
    if (is_number(a) && is_int(b)) return as_number(a) == (double)as_int(b);
    if (is_bool(a) && is_bool(b)) return as_bool(a) == as_bool(b);
    if (is_nothing(a) && is_nothing(b)) return true;
    if (is_obj(a) && is_obj(b)) {
        Obj* oa = (Obj*)as_obj(a);
        Obj* ob = (Obj*)as_obj(b);
        if (oa->type == OBJ_STRING && ob->type == OBJ_STRING) {
            ObjString* sa = (ObjString*)oa;
            ObjString* sb = (ObjString*)ob;
            return sa->length == sb->length &&
                   memcmp(sa->chars, sb->chars, sa->length) == 0;
        }
    }
    return a == b;
}

const char* opcode_name(OpCode op) {
    switch (op) {
        case OP_CONST: return "OP_CONST";
        case OP_TRUE: return "OP_TRUE";
        case OP_FALSE: return "OP_FALSE";
        case OP_NOTHING: return "OP_NOTHING";
        case OP_ADD: return "OP_ADD";
        case OP_SUB: return "OP_SUB";
        case OP_MUL: return "OP_MUL";
        case OP_DIV: return "OP_DIV";
        case OP_MOD: return "OP_MOD";
        case OP_POW: return "OP_POW";
        case OP_NEG: return "OP_NEG";
        case OP_EQ: return "OP_EQ";
        case OP_NEQ: return "OP_NEQ";
        case OP_LT: return "OP_LT";
        case OP_GT: return "OP_GT";
        case OP_LTE: return "OP_LTE";
        case OP_GTE: return "OP_GTE";
        case OP_NOT: return "OP_NOT";
        case OP_DEFINE_GLOBAL: return "OP_DEFINE_GLOBAL";
        case OP_GET_GLOBAL: return "OP_GET_GLOBAL";
        case OP_SET_GLOBAL: return "OP_SET_GLOBAL";
        case OP_GET_LOCAL: return "OP_GET_LOCAL";
        case OP_SET_LOCAL: return "OP_SET_LOCAL";
        case OP_GET_UPVALUE: return "OP_GET_UPVALUE";
        case OP_SET_UPVALUE: return "OP_SET_UPVALUE";
        case OP_POP: return "OP_POP";
        case OP_DUP: return "OP_DUP";
        case OP_JMP: return "OP_JMP";
        case OP_JMP_IF_FALSE: return "OP_JMP_IF_FALSE";
        case OP_LOOP: return "OP_LOOP";
        case OP_CALL: return "OP_CALL";
        case OP_CLOSURE: return "OP_CLOSURE";
        case OP_CLOSE_UPVALUE: return "OP_CLOSE_UPVALUE";
        case OP_RETURN: return "OP_RETURN";
        case OP_CONCAT: return "OP_CONCAT";
        case OP_TO_STRING: return "OP_TO_STRING";
        case OP_PRINT: return "OP_PRINT";
        case OP_ARRAY: return "OP_ARRAY";
        case OP_ARRAY_PUSH: return "OP_ARRAY_PUSH";
        case OP_INDEX_GET: return "OP_INDEX_GET";
        case OP_INDEX_SET: return "OP_INDEX_SET";
        case OP_MAP: return "OP_MAP";
        case OP_DEFER: return "OP_DEFER";
        case OP_AWAIT: return "OP_AWAIT";
        case OP_SPAWN: return "OP_SPAWN";
        case OP_CHANNEL_CREATE: return "OP_CHANNEL_CREATE";
        case OP_CHANNEL_SEND: return "OP_CHANNEL_SEND";
        case OP_CHANNEL_RECV: return "OP_CHANNEL_RECV";
        case OP_USE_MODULE: return "OP_USE_MODULE";
        case OP_USE_SYMBOL: return "OP_USE_SYMBOL";
        case OP_TAIL_CALL: return "OP_TAIL_CALL";
        case OP_GET_FIELD: return "OP_GET_FIELD";
        case OP_SET_FIELD: return "OP_SET_FIELD";
        case OP_STRUCT: return "OP_STRUCT";
        case OP_HALT: return "OP_HALT";
        default: return "UNKNOWN";
    }
}

/* ══════════════════════════════════════════════════════════════
 *  NATIVE FUNCTION WRAPPER
 * ══════════════════════════════════════════════════════════════ */

typedef struct {
    Obj obj;
    NativeFn function;
    int arity;
    const char* name;
} ObjNative;

static ObjNative* obj_native_new(NativeFn fn, int arity, const char* name) {
    ObjNative* native = (ObjNative*)allocate_obj(sizeof(ObjNative), OBJ_FUNCTION);
    /* We reuse OBJ_FUNCTION type but with a flag — let's use a different approach */
    /* Actually, let's add a field to tell them apart. For simplicity we'll tag it. */
    native->function = fn;
    native->arity = arity;
    native->name = name;
    /* Mark as native by setting code to NULL (ObjFunction would have code) */
    return native;
}

/* Check if a function object is native */
static bool is_native(Value val) {
    if (!is_obj(val)) return false;
    Obj* obj = (Obj*)as_obj(val);
    if (obj->type != OBJ_FUNCTION) return false;
    ObjFunction* fn = (ObjFunction*)obj;
    /* Native functions have NULL code */
    return fn->code == NULL && fn->chunk_count == -1;
}

/* ══════════════════════════════════════════════════════════════
 *  BUILT-IN NATIVE FUNCTIONS
 * ══════════════════════════════════════════════════════════════ */

static Value native_len(int argc, Value* args) {
    if (argc != 1) return val_int(0);
    Value arg = args[0];
    if (is_obj(arg)) {
        Obj* obj = (Obj*)as_obj(arg);
        if (obj->type == OBJ_STRING) return val_int(((ObjString*)obj)->length);
        if (obj->type == OBJ_ARRAY) return val_int(((ObjArray*)obj)->count);
    }
    return val_int(0);
}

static Value native_push(int argc, Value* args) {
    if (argc != 2) return val_nothing_v();
    if (!is_obj(args[0])) return val_nothing_v();
    Obj* obj = (Obj*)as_obj(args[0]);
    if (obj->type != OBJ_ARRAY) return val_nothing_v();
    ObjArray* arr = (ObjArray*)obj;
    if (arr->count >= arr->capacity) {
        arr->capacity = arr->capacity < 8 ? 8 : arr->capacity * 2;
        arr->items = realloc(arr->items, arr->capacity * sizeof(Value));
    }
    arr->items[arr->count++] = args[1];
    return val_nothing_v();
}

static Value native_pop(int argc, Value* args) {
    if (argc != 1) return val_nothing_v();
    if (!is_obj(args[0])) return val_nothing_v();
    Obj* obj = (Obj*)as_obj(args[0]);
    if (obj->type != OBJ_ARRAY) return val_nothing_v();
    ObjArray* arr = (ObjArray*)obj;
    if (arr->count == 0) return val_nothing_v();
    return arr->items[--arr->count];
}

static Value native_type(int argc, Value* args) {
    if (argc != 1) return val_obj(obj_string_new("unknown", 7));
    Value v = args[0];
    const char* t;
    if (is_int(v)) t = "int";
    else if (is_number(v)) t = "float";
    else if (is_bool(v)) t = "bool";
    else if (is_nothing(v)) t = "nothing";
    else if (is_obj(v)) {
        Obj* obj = (Obj*)as_obj(v);
        switch (obj->type) {
            case OBJ_STRING: t = "str"; break;
            case OBJ_FUNCTION: t = "fn"; break;
            case OBJ_ARRAY: t = "array"; break;
            case OBJ_MAP: t = "map"; break;
            default: t = "object"; break;
        }
    } else t = "unknown";
    return val_obj(obj_string_new(t, (int)strlen(t)));
}

static Value native_str(int argc, Value* args) {
    if (argc != 1) return val_obj(obj_string_new("", 0));
    char buf[256];
    Value v = args[0];
    if (is_int(v)) {
        snprintf(buf, sizeof(buf), "%d", as_int(v));
    } else if (is_number(v)) {
        double d = as_number(v);
        if (d == (int64_t)d && fabs(d) < 1e15) {
            snprintf(buf, sizeof(buf), "%lld", (long long)(int64_t)d);
        } else {
            snprintf(buf, sizeof(buf), "%g", d);
        }
    } else if (is_bool(v)) {
        snprintf(buf, sizeof(buf), "%s", as_bool(v) ? "true" : "false");
    } else if (is_nothing(v)) {
        snprintf(buf, sizeof(buf), "nothing");
    } else if (is_obj(v)) {
        Obj* obj = (Obj*)as_obj(v);
        if (obj->type == OBJ_STRING) return v; /* already a string */
        switch (obj->type) {
            case OBJ_ARRAY: snprintf(buf, sizeof(buf), "<array>"); break;
            case OBJ_MAP: snprintf(buf, sizeof(buf), "<map>"); break;
            case OBJ_FUNCTION:
            case OBJ_CLOSURE: snprintf(buf, sizeof(buf), "<fn>"); break;
            case OBJ_INSTANCE: snprintf(buf, sizeof(buf), "<instance>"); break;
            default: snprintf(buf, sizeof(buf), "<object>"); break;
        }
    } else {
        snprintf(buf, sizeof(buf), "<value>");
    }
    return val_obj(obj_string_new(buf, (int)strlen(buf)));
}

static Value native_int_cast(int argc, Value* args) {
    if (argc != 1) return val_int(0);
    Value v = args[0];
    if (is_int(v)) return v;
    if (is_number(v)) return val_int((int32_t)as_number(v));
    if (is_bool(v)) return val_int(as_bool(v) ? 1 : 0);
    if (is_obj(v)) {
        Obj* obj = (Obj*)as_obj(v);
        if (obj->type == OBJ_STRING) {
            return val_int(atoi(((ObjString*)obj)->chars));
        }
    }
    return val_int(0);
}

static Value native_float_cast(int argc, Value* args) {
    if (argc != 1) return val_number(0.0);
    Value v = args[0];
    if (is_number(v)) return v;
    if (is_int(v)) return val_number((double)as_int(v));
    if (is_obj(v)) {
        Obj* obj = (Obj*)as_obj(v);
        if (obj->type == OBJ_STRING) {
            return val_number(atof(((ObjString*)obj)->chars));
        }
    }
    return val_number(0.0);
}

static Value native_sqrt(int argc, Value* args) {
    if (argc != 1) return val_number(0.0);
    double d;
    if (is_int(args[0])) d = (double)as_int(args[0]);
    else if (is_number(args[0])) d = as_number(args[0]);
    else return val_number(0.0);
    return val_number(sqrt(d));
}

static Value native_abs(int argc, Value* args) {
    if (argc != 1) return val_int(0);
    if (is_int(args[0])) {
        int32_t v = as_int(args[0]);
        return val_int(v < 0 ? -v : v);
    }
    if (is_number(args[0])) return val_number(fabs(as_number(args[0])));
    return val_int(0);
}

static Value native_pow_fn(int argc, Value* args) {
    if (argc != 2) return val_number(0.0);
    double base, exp;
    if (is_int(args[0])) base = (double)as_int(args[0]);
    else if (is_number(args[0])) base = as_number(args[0]);
    else return val_number(0.0);
    if (is_int(args[1])) exp = (double)as_int(args[1]);
    else if (is_number(args[1])) exp = as_number(args[1]);
    else return val_number(0.0);
    double r = pow(base, exp);
    if (r == (int64_t)r && fabs(r) < 2147483647.0) return val_int((int32_t)r);
    return val_number(r);
}

static Value native_min_fn(int argc, Value* args) {
    if (argc != 2) return val_nothing_v();
    double a = is_int(args[0]) ? (double)as_int(args[0]) : (is_number(args[0]) ? as_number(args[0]) : 0.0);
    double b = is_int(args[1]) ? (double)as_int(args[1]) : (is_number(args[1]) ? as_number(args[1]) : 0.0);
    double m = fmin(a, b);
    if (m == (int64_t)m && fabs(m) < 2147483647.0) return val_int((int32_t)m);
    return val_number(m);
}

static Value native_max_fn(int argc, Value* args) {
    if (argc != 2) return val_nothing_v();
    double a = is_int(args[0]) ? (double)as_int(args[0]) : (is_number(args[0]) ? as_number(args[0]) : 0.0);
    double b = is_int(args[1]) ? (double)as_int(args[1]) : (is_number(args[1]) ? as_number(args[1]) : 0.0);
    double m = fmax(a, b);
    if (m == (int64_t)m && fabs(m) < 2147483647.0) return val_int((int32_t)m);
    return val_number(m);
}

static Value native_floor_fn(int argc, Value* args) {
    if (argc != 1) return val_nothing_v();
    double v = is_int(args[0]) ? (double)as_int(args[0]) : (is_number(args[0]) ? as_number(args[0]) : 0.0);
    double f = floor(v);
    if (f == (int64_t)f && fabs(f) < 2147483647.0) return val_int((int32_t)f);
    return val_number(f);
}

static Value native_ceil_fn(int argc, Value* args) {
    if (argc != 1) return val_nothing_v();
    double v = is_int(args[0]) ? (double)as_int(args[0]) : (is_number(args[0]) ? as_number(args[0]) : 0.0);
    double c = ceil(v);
    if (c == (int64_t)c && fabs(c) < 2147483647.0) return val_int((int32_t)c);
    return val_number(c);
}

static Value native_upper(int argc, Value* args) {
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_STRING) {
        return val_obj(obj_string_new("", 0));
    }
    ObjString* s = (ObjString*)as_obj(args[0]);
    char* out = (char*)malloc((size_t)s->length + 1);
    for (int i = 0; i < s->length; i++) out[i] = (char)toupper((unsigned char)s->chars[i]);
    out[s->length] = '\0';
    ObjString* res = obj_string_new(out, s->length);
    free(out);
    return val_obj(res);
}

static Value native_lower(int argc, Value* args) {
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_STRING) {
        return val_obj(obj_string_new("", 0));
    }
    ObjString* s = (ObjString*)as_obj(args[0]);
    char* out = (char*)malloc((size_t)s->length + 1);
    for (int i = 0; i < s->length; i++) out[i] = (char)tolower((unsigned char)s->chars[i]);
    out[s->length] = '\0';
    ObjString* res = obj_string_new(out, s->length);
    free(out);
    return val_obj(res);
}

static Value native_trim(int argc, Value* args) {
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_STRING) {
        return val_obj(obj_string_new("", 0));
    }
    ObjString* s = (ObjString*)as_obj(args[0]);
    int start = 0;
    int end = s->length;
    while (start < end && isspace((unsigned char)s->chars[start])) start++;
    while (end > start && isspace((unsigned char)s->chars[end - 1])) end--;
    return val_obj(obj_string_new(s->chars + start, end - start));
}

static Value native_contains(int argc, Value* args) {
    if (argc != 2 || !is_obj(args[0]) || !is_obj(args[1])) return val_bool(false);
    Obj* a = (Obj*)as_obj(args[0]);
    Obj* b = (Obj*)as_obj(args[1]);
    if (a->type != OBJ_STRING || b->type != OBJ_STRING) return val_bool(false);
    ObjString* hay = (ObjString*)a;
    ObjString* needle = (ObjString*)b;
    return val_bool(strstr(hay->chars, needle->chars) != NULL);
}

static Value native_sort(int argc, Value* args) {
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY) {
        return val_nothing_v();
    }
    ObjArray* arr = (ObjArray*)as_obj(args[0]);
    for (int i = 0; i < arr->count; i++) {
        for (int j = i + 1; j < arr->count; j++) {
            double ai = is_int(arr->items[i]) ? (double)as_int(arr->items[i]) : (is_number(arr->items[i]) ? as_number(arr->items[i]) : 0.0);
            double aj = is_int(arr->items[j]) ? (double)as_int(arr->items[j]) : (is_number(arr->items[j]) ? as_number(arr->items[j]) : 0.0);
            if (aj < ai) {
                Value tmp = arr->items[i];
                arr->items[i] = arr->items[j];
                arr->items[j] = tmp;
            }
        }
    }
    return val_nothing_v();
}

static Value native_reverse(int argc, Value* args) {
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY) {
        return val_nothing_v();
    }
    ObjArray* arr = (ObjArray*)as_obj(args[0]);
    for (int i = 0, j = arr->count - 1; i < j; i++, j--) {
        Value tmp = arr->items[i];
        arr->items[i] = arr->items[j];
        arr->items[j] = tmp;
    }
    return val_nothing_v();
}

static Value native_index_of(int argc, Value* args) {
    if (argc != 2 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY) {
        return val_int(-1);
    }
    ObjArray* arr = (ObjArray*)as_obj(args[0]);
    for (int i = 0; i < arr->count; i++) {
        if (values_equal(arr->items[i], args[1])) return val_int(i);
    }
    return val_int(-1);
}

static Value native_insert(int argc, Value* args) {
    /* insert(arr, idx, val) */
    if (argc != 3 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY) {
        return val_nothing_v();
    }
    ObjArray* arr = (ObjArray*)as_obj(args[0]);
    int idx = is_int(args[1]) ? (int)as_int(args[1]) : (int)as_number(args[1]);
    if (idx < 0) idx = 0;
    if (idx > arr->count) idx = arr->count;
    if (arr->count >= arr->capacity) {
        arr->capacity = arr->capacity < 8 ? 8 : arr->capacity * 2;
        arr->items = realloc(arr->items, arr->capacity * sizeof(Value));
    }
    memmove(&arr->items[idx + 1], &arr->items[idx], (arr->count - idx) * sizeof(Value));
    arr->items[idx] = args[2];
    arr->count++;
    return val_nothing_v();
}

static Value native_slice(int argc, Value* args) {
    /* slice(arr, start) or slice(arr, start, end) */
    if (argc < 2 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY) {
        return val_nothing_v();
    }
    ObjArray* arr = (ObjArray*)as_obj(args[0]);
    int start = is_int(args[1]) ? (int)as_int(args[1]) : (int)as_number(args[1]);
    int end = arr->count;
    if (argc >= 3) {
        end = is_int(args[2]) ? (int)as_int(args[2]) : (int)as_number(args[2]);
    }
    if (start < 0) start = 0;
    if (end > arr->count) end = arr->count;
    if (start >= end) return val_obj(obj_array_new(0));
    int new_count = end - start;
    ObjArray* result = obj_array_new(new_count);
    for (int i = 0; i < new_count; i++) result->items[i] = arr->items[start + i];
    result->count = new_count;
    return val_obj(result);
}

static Value native_remove_at(int argc, Value* args) {
    /* remove(arr, idx) */
    if (argc != 2 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY) {
        return val_nothing_v();
    }
    ObjArray* arr = (ObjArray*)as_obj(args[0]);
    int idx = is_int(args[1]) ? (int)as_int(args[1]) : (int)as_number(args[1]);
    if (idx < 0 || idx >= arr->count) return val_nothing_v();
    Value removed = arr->items[idx];
    memmove(&arr->items[idx], &arr->items[idx + 1], (arr->count - idx - 1) * sizeof(Value));
    arr->count--;
    return removed;
}

static Value native_unique(int argc, Value* args) {
    /* unique(arr) -> new array with duplicates removed */
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY) {
        return val_nothing_v();
    }
    ObjArray* arr = (ObjArray*)as_obj(args[0]);
    ObjArray* result = obj_array_new(arr->count < 8 ? 8 : arr->count);
    for (int i = 0; i < arr->count; i++) {
        bool found = false;
        for (int j = 0; j < result->count; j++) {
            if (values_equal(arr->items[i], result->items[j])) { found = true; break; }
        }
        if (!found) {
            if (result->count >= result->capacity) {
                result->capacity *= 2;
                result->items = realloc(result->items, result->capacity * sizeof(Value));
            }
            result->items[result->count++] = arr->items[i];
        }
    }
    return val_obj(result);
}

static Value native_sum(int argc, Value* args) {
    /* sum(arr) -> numeric sum */
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY) {
        return val_int(0);
    }
    ObjArray* arr = (ObjArray*)as_obj(args[0]);
    double total = 0.0;
    bool all_int = true;
    for (int i = 0; i < arr->count; i++) {
        if (is_int(arr->items[i])) total += (double)as_int(arr->items[i]);
        else if (is_number(arr->items[i])) { total += as_number(arr->items[i]); all_int = false; }
    }
    return all_int ? val_int((int64_t)total) : val_number(total);
}

static Value native_any_fn(int argc, Value* args) {
    /* any(arr) -> bool */
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY) {
        return val_bool(false);
    }
    ObjArray* arr = (ObjArray*)as_obj(args[0]);
    for (int i = 0; i < arr->count; i++) {
        Value v = arr->items[i];
        if (is_bool(v) && as_bool(v)) return val_bool(true);
        if (is_int(v) && as_int(v) != 0) return val_bool(true);
        if (is_number(v) && as_number(v) != 0.0) return val_bool(true);
    }
    return val_bool(false);
}

static Value native_all_fn(int argc, Value* args) {
    /* all(arr) -> bool */
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY) {
        return val_bool(true);
    }
    ObjArray* arr = (ObjArray*)as_obj(args[0]);
    for (int i = 0; i < arr->count; i++) {
        Value v = arr->items[i];
        if (is_bool(v) && !as_bool(v)) return val_bool(false);
        if (is_int(v) && as_int(v) == 0) return val_bool(false);
        if (is_number(v) && as_number(v) == 0.0) return val_bool(false);
        if (is_nothing(v)) return val_bool(false);
    }
    return val_bool(true);
}

static Value native_flatten(int argc, Value* args) {
    /* flatten(arr) -> flat array (one level deep) */
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY) {
        return val_nothing_v();
    }
    ObjArray* arr = (ObjArray*)as_obj(args[0]);
    ObjArray* result = obj_array_new(arr->count > 4 ? arr->count * 2 : 8);
    for (int i = 0; i < arr->count; i++) {
        Value item = arr->items[i];
        if (is_obj(item) && ((Obj*)as_obj(item))->type == OBJ_ARRAY) {
            ObjArray* inner = (ObjArray*)as_obj(item);
            for (int j = 0; j < inner->count; j++) {
                if (result->count >= result->capacity) {
                    result->capacity *= 2;
                    result->items = realloc(result->items, result->capacity * sizeof(Value));
                }
                result->items[result->count++] = inner->items[j];
            }
        } else {
            if (result->count >= result->capacity) {
                result->capacity *= 2;
                result->items = realloc(result->items, result->capacity * sizeof(Value));
            }
            result->items[result->count++] = item;
        }
    }
    return val_obj(result);
}

static Value native_zip(int argc, Value* args) {
    /* zip(arr1, arr2) -> [[a0,b0],[a1,b1],...] */
    if (argc != 2 || !is_obj(args[0]) || !is_obj(args[1])) return val_nothing_v();
    if (((Obj*)as_obj(args[0]))->type != OBJ_ARRAY || ((Obj*)as_obj(args[1]))->type != OBJ_ARRAY) return val_nothing_v();
    ObjArray* a = (ObjArray*)as_obj(args[0]);
    ObjArray* b = (ObjArray*)as_obj(args[1]);
    int n = a->count < b->count ? a->count : b->count;
    ObjArray* result = obj_array_new(n < 8 ? 8 : n);
    for (int i = 0; i < n; i++) {
        ObjArray* pair = obj_array_new(2);
        pair->items[0] = a->items[i];
        pair->items[1] = b->items[i];
        pair->count = 2;
        result->items[result->count++] = val_obj(pair);
    }
    return val_obj(result);
}

static Value native_enumerate(int argc, Value* args) {
    /* enumerate(arr) -> [[0,v0],[1,v1],...] */
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY) return val_nothing_v();
    ObjArray* arr = (ObjArray*)as_obj(args[0]);
    ObjArray* result = obj_array_new(arr->count < 8 ? 8 : arr->count);
    for (int i = 0; i < arr->count; i++) {
        ObjArray* pair = obj_array_new(2);
        pair->items[0] = val_int(i);
        pair->items[1] = arr->items[i];
        pair->count = 2;
        result->items[result->count++] = val_obj(pair);
    }
    return val_obj(result);
}

static Value native_clock(int argc, Value* args) {
    (void)argc; (void)args;
    return val_number((double)clock() / CLOCKS_PER_SEC);
}

static Value native_input(int argc, Value* args) {
    if (argc >= 1 && is_obj(args[0])) {
        Obj* obj = (Obj*)as_obj(args[0]);
        if (obj->type == OBJ_STRING) {
            printf("%s", ((ObjString*)obj)->chars);
            fflush(stdout);
        }
    }
    char buf[1024];
    if (fgets(buf, sizeof(buf), stdin)) {
        int len = (int)strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[--len] = '\0';
        return val_obj(obj_string_new(buf, len));
    }
    return val_obj(obj_string_new("", 0));
}

/* ══════════════════════════════════════════════════════════════
 *  VM IMPLEMENTATION
 * ══════════════════════════════════════════════════════════════ */

void vm_init(VM* vm) {
    vm->frame_count = 0;
    vm->stack_top = vm->stack;
    vm->objects = NULL;
    vm->open_upvalues = NULL;
    vm->has_last_result = false;
    vm->last_result = val_nothing_v();
    memset(vm->globals, 0, sizeof(vm->globals));
}

VM* vm_new(void) {
    VM* vm = (VM*)malloc(sizeof(VM));
    if (vm) vm_init(vm);
    return vm;
}

void vm_free(VM* vm) {
    /* Free all heap objects */
    Obj* obj = vm->objects;
    while (obj) {
        Obj* next = obj->next;
        switch (obj->type) {
            case OBJ_STRING:
                /* ObjString uses flexible array, just free the obj */
                break;
            case OBJ_FUNCTION: {
                ObjFunction* fn = (ObjFunction*)obj;
                free(fn->code);
                free(fn->lines);
                free(fn->constants);
                free(fn->name);
                break;
            }
            case OBJ_ARRAY: {
                ObjArray* arr = (ObjArray*)obj;
                free(arr->items);
                break;
            }
            case OBJ_MAP: {
                ObjMap* map = (ObjMap*)obj;
                free(map->entries);
                break;
            }
            case OBJ_CLOSURE: {
                ObjClosure* cl = (ObjClosure*)obj;
                free(cl->upvalues);
                break;
            }
            case OBJ_STRUCT_DEF: {
                ObjStructDef* def = (ObjStructDef*)obj;
                free(def->field_names);
                /* methods map freed when its own Obj is reached */
                break;
            }
            case OBJ_INSTANCE: {
                /* fields map freed when its own Obj is reached */
                break;
            }
            case OBJ_CHANNEL: {
                Channel* ch = (Channel*)obj;
                mutex_destroy(&ch->lock);
                cond_destroy(&ch->not_empty);
                cond_destroy(&ch->not_full);
                break;
            }
            case OBJ_TASK_HANDLE: {
                ObjTaskHandle* handle = (ObjTaskHandle*)obj;
                if (handle->task) {
                    task_destroy(handle->task);
                    handle->task = NULL;
                }
                break;
            }
            default:
                break;
        }
        free(obj);
        obj = next;
    }
    vm->objects = NULL;
}

/* Stack ops */
static inline void push(VM* vm, Value val) {
    if (vm->stack_top >= vm->stack + STACK_MAX) {
        fprintf(stderr, "[VM Error] Stack overflow\n");
        exit(1);
    }
    *vm->stack_top = val;
    vm->stack_top++;
}

static inline Value pop(VM* vm) {
    vm->stack_top--;
    return *vm->stack_top;
}

static inline Value peek(VM* vm, int distance) {
    return vm->stack_top[-1 - distance];
}

/* Read bytes from instruction stream */
#define READ_BYTE()    (*frame->ip++)
#define READ_SHORT()   (frame->ip += 2, (uint16_t)((frame->ip[-2]) | (frame->ip[-1] << 8)))
#define READ_CONSTANT() (frame->closure->function->constants[READ_SHORT()])

/* Hash table lookup for globals */
static uint32_t global_hash(const char* name) {
    return hash_string(name, (int)strlen(name));
}

static int global_find_slot(VM* vm, ObjString* name) {
    uint32_t h = name->hash % GLOBALS_MAX;
    for (int i = 0; i < GLOBALS_MAX; i++) {
        int idx = (h + i) % GLOBALS_MAX;
        if (!vm->globals[idx].occupied) return idx;
        if (vm->globals[idx].name->hash == name->hash &&
            vm->globals[idx].name->length == name->length &&
            memcmp(vm->globals[idx].name->chars, name->chars, name->length) == 0) {
            return idx;
        }
    }
    return -1;  /* table full */
}

void vm_define_native(VM* vm, const char* name, NativeFn fn, int arity) {
    ObjString* name_str = obj_string_new(name, (int)strlen(name));
    /* Create a "native function" object — we'll store it as an ObjFunction
       with code = NULL and chunk_count = -1 as a marker */
    ObjFunction* native = obj_function_new(name);
    native->arity = arity;
    native->code = NULL;
    native->chunk_count = -1;  /* marker: this is native */
    /* Store the native fn pointer in the constants array (hack but works) */
    native->const_count = 1;
    native->const_capacity = 1;
    native->constants = malloc(sizeof(Value));
    /* Store fn pointer as raw bits - cast through uintptr_t */
    uint64_t fn_bits;
    memcpy(&fn_bits, &fn, sizeof(fn));
    native->constants[0] = fn_bits;

    int slot = global_find_slot(vm, name_str);
    if (slot >= 0) {
        vm->globals[slot].name = name_str;
        vm->globals[slot].value = val_obj(native);
        vm->globals[slot].occupied = true;
    }
}

static void register_builtins(VM* vm) {
    vm_define_native(vm, "len", native_len, 1);
    vm_define_native(vm, "push", native_push, 2);
    vm_define_native(vm, "pop", native_pop, 1);
    vm_define_native(vm, "type", native_type, 1);
    vm_define_native(vm, "str", native_str, 1);
    vm_define_native(vm, "int", native_int_cast, 1);
    vm_define_native(vm, "float", native_float_cast, 1);
    vm_define_native(vm, "sqrt", native_sqrt, 1);
    vm_define_native(vm, "abs", native_abs, 1);
    vm_define_native(vm, "pow", native_pow_fn, 2);
    vm_define_native(vm, "min", native_min_fn, 2);
    vm_define_native(vm, "max", native_max_fn, 2);
    vm_define_native(vm, "floor", native_floor_fn, 1);
    vm_define_native(vm, "ceil", native_ceil_fn, 1);
    vm_define_native(vm, "upper", native_upper, 1);
    vm_define_native(vm, "lower", native_lower, 1);
    vm_define_native(vm, "trim", native_trim, 1);
    vm_define_native(vm, "contains", native_contains, 2);
    vm_define_native(vm, "sort", native_sort, 1);
    vm_define_native(vm, "reverse", native_reverse, 1);
    vm_define_native(vm, "index_of", native_index_of, 2);
    vm_define_native(vm, "insert", native_insert, 3);
    vm_define_native(vm, "slice", native_slice, -1);
    vm_define_native(vm, "remove", native_remove_at, 2);
    vm_define_native(vm, "unique", native_unique, 1);
    vm_define_native(vm, "sum", native_sum, 1);
    vm_define_native(vm, "any", native_any_fn, 1);
    vm_define_native(vm, "all", native_all_fn, 1);
    vm_define_native(vm, "flatten", native_flatten, 1);
    vm_define_native(vm, "zip", native_zip, 2);
    vm_define_native(vm, "enumerate", native_enumerate, 1);
    vm_define_native(vm, "clock", native_clock, 0);
    vm_define_native(vm, "input", native_input, 1);

    /* Math constants */
    ObjString* pi_name = obj_string_new("PI", 2);
    int pi_slot = global_find_slot(vm, pi_name);
    if (pi_slot >= 0) {
        vm->globals[pi_slot].name = pi_name;
        vm->globals[pi_slot].value = val_number(3.14159265358979323846);
        vm->globals[pi_slot].occupied = true;
    }
    ObjString* e_name = obj_string_new("E", 1);
    int e_slot = global_find_slot(vm, e_name);
    if (e_slot >= 0) {
        vm->globals[e_slot].name = e_name;
        vm->globals[e_slot].value = val_number(2.71828182845904523536);
        vm->globals[e_slot].occupied = true;
    }
}

static bool vm_lookup_global(VM* vm, const char* name, Value* out) {
    for (int i = 0; i < GLOBALS_MAX; i++) {
        if (!vm->globals[i].occupied || !vm->globals[i].name) continue;
        if (strcmp(vm->globals[i].name->chars, name) == 0) {
            if (out) *out = vm->globals[i].value;
            return true;
        }
    }
    return false;
}

static bool vm_set_global(VM* vm, const char* name, Value value) {
    ObjString* name_str = obj_string_new(name, (int)strlen(name));
    int slot = global_find_slot(vm, name_str);
    if (slot < 0) return false;
    vm->globals[slot].name = name_str;
    vm->globals[slot].value = value;
    vm->globals[slot].occupied = true;
    return true;
}

static bool vm_alias_global(VM* vm, const char* alias_name, const char* source_name) {
    Value src;
    if (!vm_lookup_global(vm, source_name, &src)) return false;
    return vm_set_global(vm, alias_name, src);
}

static bool vm_import_module(VM* vm, const char* module_name) {
    if (strcmp(module_name, "math") == 0 ||
        strcmp(module_name, "string") == 0 ||
        strcmp(module_name, "collections") == 0 ||
        strcmp(module_name, "sys") == 0 ||
        strcmp(module_name, "fs") == 0) {
        return true;
    }
    if (strcmp(module_name, "json") == 0) {
        vm_register_json_module(vm);
        return true;
    }
    if (strcmp(module_name, "time") == 0) {
        vm_register_time_module(vm);
        return true;
    }
    if (strcmp(module_name, "http") == 0) {
        vm_register_http_module(vm);
        return true;
    }
    return false;
}

static bool vm_import_symbol(VM* vm, const char* module_name, const char* symbol_name) {
    if (!vm_import_module(vm, module_name)) return false;

    if (strcmp(module_name, "json") == 0) {
        if (strcmp(symbol_name, "parse") == 0) return vm_alias_global(vm, "parse", "json_parse");
        if (strcmp(symbol_name, "stringify") == 0) return vm_alias_global(vm, "stringify", "json_stringify");
    }

    if (strcmp(module_name, "time") == 0) {
        if (strcmp(symbol_name, "now") == 0) return vm_alias_global(vm, "now", "time_now");
        if (strcmp(symbol_name, "format") == 0) return vm_alias_global(vm, "format", "time_format");
        if (strcmp(symbol_name, "sleep") == 0) return vm_alias_global(vm, "sleep", "time_sleep");
        if (strcmp(symbol_name, "clock") == 0) return vm_alias_global(vm, "clock", "time_clock");
    }

    if (strcmp(module_name, "http") == 0) {
        if (strcmp(symbol_name, "get") == 0) return vm_alias_global(vm, "get", "http_get");
        if (strcmp(symbol_name, "post") == 0) return vm_alias_global(vm, "post", "http_post");
    }

    if (strcmp(module_name, "collections") == 0) {
        /* All collections functions are already registered as globals */
        const char* coll_syms[] = {
            "sort", "reverse", "index_of", "insert", "slice", "remove",
            "unique", "sum", "any", "all", "flatten", "zip", "enumerate", NULL
        };
        for (int i = 0; coll_syms[i] != NULL; i++) {
            if (strcmp(symbol_name, coll_syms[i]) == 0) {
                return vm_lookup_global(vm, symbol_name, NULL);
            }
        }
        return false;
    }

    if (vm_lookup_global(vm, symbol_name, NULL)) {
        return true;
    }

    return false;
}

/* ══════════════════════════════════════════════════════════════
 *  THE DISPATCH LOOP
 * ══════════════════════════════════════════════════════════════ */

static void runtime_error(VM* vm, CallFrame* frame, const char* fmt, ...) {
    int line = 0;
    if (frame && frame->closure && frame->closure->function && frame->closure->function->lines) {
        int offset = (int)(frame->ip - frame->closure->function->code - 1);
        if (offset >= 0 && offset < frame->closure->function->chunk_count) {
            line = frame->closure->function->lines[offset];
        }
    }
    fprintf(stderr, "[Runtime Error] Line %d: ", line);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

/* Helper: get numeric value as double */
static double to_double(Value v) {
    if (is_int(v)) return (double)as_int(v);
    if (is_number(v)) return as_number(v);
    return 0.0;
}

VMResult vm_run(VM* vm, ObjFunction* fn) {
    running_vm = vm;
    vm->has_last_result = false;
    vm->last_result = val_nothing_v();
    register_builtins(vm);

    /* Set up the initial call frame */
    ObjClosure* closure = obj_closure_new(fn);
    push(vm, val_obj(closure));
    CallFrame* frame = &vm->frames[vm->frame_count++];
    frame->closure = closure;
    frame->ip = fn->code;
    frame->slots = vm->stack;

    for (;;) {
        uint8_t instruction = READ_BYTE();

#ifdef DEBUG_VM
        /* Debug: print instruction */
        printf("          ");
        for (Value* slot = vm->stack; slot < vm->stack_top; slot++) {
            printf("[ ");
            value_print(*slot);
            printf(" ]");
        }
        printf("\n");
        int offset = (int)(frame->ip - frame->function->code - 1);
        printf("%04d %s\n", offset, opcode_name(instruction));
#endif

        switch (instruction) {

        case OP_CONST: {
            Value constant = READ_CONSTANT();
            push(vm, constant);
            break;
        }

        case OP_TRUE:    push(vm, val_bool(true)); break;
        case OP_FALSE:   push(vm, val_bool(false)); break;
        case OP_NOTHING: push(vm, val_nothing_v()); break;

        /* ── Arithmetic ── */
        case OP_ADD: {
            Value b = pop(vm);
            Value a = pop(vm);
            /* String concatenation */
            if (is_obj(a) && is_obj(b)) {
                Obj* oa = (Obj*)as_obj(a);
                Obj* ob = (Obj*)as_obj(b);
                if (oa->type == OBJ_STRING && ob->type == OBJ_STRING) {
                    ObjString* sa = (ObjString*)oa;
                    ObjString* sb = (ObjString*)ob;
                    int len = sa->length + sb->length;
                    char* buf = malloc(len + 1);
                    memcpy(buf, sa->chars, sa->length);
                    memcpy(buf + sa->length, sb->chars, sb->length);
                    buf[len] = '\0';
                    ObjString* result = obj_string_new(buf, len);
                    free(buf);
                    push(vm, val_obj(result));
                    break;
                }
            }
            /* Numeric addition */
            if (is_int(a) && is_int(b)) {
                push(vm, val_int(as_int(a) + as_int(b)));
            } else {
                push(vm, val_number(to_double(a) + to_double(b)));
            }
            break;
        }

        case OP_SUB: {
            Value b = pop(vm);
            Value a = pop(vm);
            if (is_int(a) && is_int(b))
                push(vm, val_int(as_int(a) - as_int(b)));
            else
                push(vm, val_number(to_double(a) - to_double(b)));
            break;
        }

        case OP_MUL: {
            Value b = pop(vm);
            Value a = pop(vm);
            /* String repetition: "ha" * 3 => "hahaha" */
            if (is_obj(a) && ((Obj*)as_obj(a))->type == OBJ_STRING &&
                (is_int(b) || is_number(b))) {
                ObjString* sa = (ObjString*)as_obj(a);
                int n = is_int(b) ? as_int(b) : (int)as_number(b);
                if (n <= 0) {
                    push(vm, val_obj(obj_string_new("", 0)));
                } else {
                    int len = sa->length * n;
                    char* buf = malloc(len + 1);
                    for (int i = 0; i < n; i++)
                        memcpy(buf + i * sa->length, sa->chars, sa->length);
                    buf[len] = '\0';
                    push(vm, val_obj(obj_string_new(buf, len)));
                    free(buf);
                }
            } else if (is_int(a) && is_int(b)) {
                push(vm, val_int(as_int(a) * as_int(b)));
            } else {
                push(vm, val_number(to_double(a) * to_double(b)));
            }
            break;
        }

        case OP_DIV: {
            Value b = pop(vm);
            Value a = pop(vm);
            double db = to_double(b);
            if (db == 0.0) {
                runtime_error(vm, frame, "Division by zero.");
                return VM_RUNTIME_ERROR;
            }
            push(vm, val_number(to_double(a) / db));
            break;
        }

        case OP_MOD: {
            Value b = pop(vm);
            Value a = pop(vm);
            if (is_int(a) && is_int(b)) {
                if (as_int(b) == 0) {
                    runtime_error(vm, frame, "Modulo by zero.");
                    return VM_RUNTIME_ERROR;
                }
                push(vm, val_int(as_int(a) % as_int(b)));
            } else {
                push(vm, val_number(fmod(to_double(a), to_double(b))));
            }
            break;
        }

        case OP_POW: {
            Value b = pop(vm);
            Value a = pop(vm);
            double result = pow(to_double(a), to_double(b));
            if (is_int(a) && is_int(b) && as_int(b) >= 0 &&
                result == (int32_t)result) {
                push(vm, val_int((int32_t)result));
            } else {
                push(vm, val_number(result));
            }
            break;
        }

        case OP_NEG: {
            Value a = pop(vm);
            if (is_int(a)) push(vm, val_int(-as_int(a)));
            else push(vm, val_number(-to_double(a)));
            break;
        }

        /* ── Comparison ── */
        case OP_EQ: {
            Value b = pop(vm);
            Value a = pop(vm);
            push(vm, val_bool(values_equal(a, b)));
            break;
        }

        case OP_NEQ: {
            Value b = pop(vm);
            Value a = pop(vm);
            push(vm, val_bool(!values_equal(a, b)));
            break;
        }

        case OP_LT: {
            Value b = pop(vm);
            Value a = pop(vm);
            push(vm, val_bool(to_double(a) < to_double(b)));
            break;
        }

        case OP_GT: {
            Value b = pop(vm);
            Value a = pop(vm);
            push(vm, val_bool(to_double(a) > to_double(b)));
            break;
        }

        case OP_LTE: {
            Value b = pop(vm);
            Value a = pop(vm);
            push(vm, val_bool(to_double(a) <= to_double(b)));
            break;
        }

        case OP_GTE: {
            Value b = pop(vm);
            Value a = pop(vm);
            push(vm, val_bool(to_double(a) >= to_double(b)));
            break;
        }

        case OP_NOT: {
            Value a = pop(vm);
            push(vm, val_bool(!value_is_truthy(a)));
            break;
        }

        /* ── Variables ── */
        case OP_DEFINE_GLOBAL: {
            Value name_val = READ_CONSTANT();
            ObjString* name = (ObjString*)as_obj(name_val);
            Value value = pop(vm);
            int slot = global_find_slot(vm, name);
            if (slot >= 0) {
                vm->globals[slot].name = name;
                vm->globals[slot].value = value;
                vm->globals[slot].occupied = true;
            } else {
                runtime_error(vm, frame, "Too many global variables.");
                return VM_RUNTIME_ERROR;
            }
            break;
        }

        case OP_GET_GLOBAL: {
            Value name_val = READ_CONSTANT();
            ObjString* name = (ObjString*)as_obj(name_val);
            int slot = global_find_slot(vm, name);
            if (slot >= 0 && vm->globals[slot].occupied) {
                push(vm, vm->globals[slot].value);
            } else {
                runtime_error(vm, frame, "Undefined variable '%s'.", name->chars);
                return VM_RUNTIME_ERROR;
            }
            break;
        }

        case OP_SET_GLOBAL: {
            Value name_val = READ_CONSTANT();
            ObjString* name = (ObjString*)as_obj(name_val);
            Value value = peek(vm, 0);  /* don't pop — assignment is an expression */
            int slot = global_find_slot(vm, name);
            if (slot >= 0 && vm->globals[slot].occupied) {
                vm->globals[slot].value = value;
            } else {
                runtime_error(vm, frame, "Undefined variable '%s'.", name->chars);
                return VM_RUNTIME_ERROR;
            }
            break;
        }

        case OP_GET_LOCAL: {
            uint8_t slot = READ_BYTE();
            push(vm, frame->slots[slot]);
            break;
        }

        case OP_SET_LOCAL: {
            uint8_t slot = READ_BYTE();
            frame->slots[slot] = peek(vm, 0);
            break;
        }

        case OP_GET_UPVALUE: {
            uint8_t slot = READ_BYTE();
            push(vm, *frame->closure->upvalues[slot]->location);
            break;
        }

        case OP_SET_UPVALUE: {
            uint8_t slot = READ_BYTE();
            *frame->closure->upvalues[slot]->location = peek(vm, 0);
            break;
        }

        /* ── Stack ── */
        case OP_POP: pop(vm); break;

        case OP_CLOSE_UPVALUE:
            close_upvalues(vm, vm->stack_top - 1);
            pop(vm);
            break;

        case OP_DUP: push(vm, peek(vm, 0)); break;

        /* ── Control flow ── */
        case OP_JMP: {
            uint16_t offset = READ_SHORT();
            frame->ip += offset;
            break;
        }

        case OP_JMP_IF_FALSE: {
            uint16_t offset = READ_SHORT();
            if (!value_is_truthy(peek(vm, 0))) {
                frame->ip += offset;
            }
            break;
        }

        case OP_LOOP: {
            uint16_t offset = READ_SHORT();
            frame->ip -= offset;
            break;
        }

        /* ── Function calls ── */
        case OP_CALL: {
            uint8_t argc = READ_BYTE();
            Value callee = peek(vm, argc);

            if (!is_obj(callee)) {
                runtime_error(vm, frame, "Can only call functions.");
                return VM_RUNTIME_ERROR;
            }

            Obj* obj = (Obj*)as_obj(callee);
            ObjClosure* closure;
            ObjFunction* callee_fn;
            if (obj->type == OBJ_FUNCTION) {
                callee_fn = (ObjFunction*)obj;
                closure = obj_closure_new(callee_fn);
            } else if (obj->type == OBJ_CLOSURE) {
                closure = (ObjClosure*)obj;
                callee_fn = closure->function;
            } else {
                runtime_error(vm, frame, "Can only call functions and closures.");
                return VM_RUNTIME_ERROR;
            }

            /* Check if native */
            if (callee_fn->code == NULL && callee_fn->chunk_count == -1) {
                /* Native function call */
                NativeFn native_fn;
                memcpy(&native_fn, &callee_fn->constants[0], sizeof(NativeFn));
                Value result = native_fn(argc, vm->stack_top - argc);
                vm->stack_top -= argc + 1;  /* pop args + callee */
                push(vm, result);
                break;
            }

            /* Vex function call */
            if (vm->frame_count >= FRAMES_MAX) {
                runtime_error(vm, frame, "Stack overflow (too many nested calls).");
                return VM_RUNTIME_ERROR;
            }

            CallFrame* new_frame = &vm->frames[vm->frame_count++];
            new_frame->closure = closure;
            new_frame->ip = callee_fn->code;
            new_frame->slots = vm->stack_top - argc - 1;

            frame = new_frame;
            break;
        }

        case OP_CLOSURE: {
            Value fn_val = READ_CONSTANT();
            if (!is_obj(fn_val) || ((Obj*)as_obj(fn_val))->type != OBJ_FUNCTION) {
                runtime_error(vm, frame, "Closure constant is not a function.");
                return VM_RUNTIME_ERROR;
            }

            ObjFunction* fn_obj = (ObjFunction*)as_obj(fn_val);
            ObjClosure* closure = obj_closure_new(fn_obj);
            push(vm, val_obj(closure));

            for (int i = 0; i < closure->upvalue_count; i++) {
                uint8_t is_local = READ_BYTE();
                uint8_t index = READ_BYTE();
                if (is_local) {
                    closure->upvalues[i] = capture_upvalue(vm, frame->slots + index);
                } else {
                    closure->upvalues[i] = frame->closure->upvalues[index];
                }
            }
            break;
        }

        case OP_RETURN: {
            Value result = pop(vm);

            close_upvalues(vm, frame->slots);

            vm->frame_count--;
            if (vm->frame_count == 0) {
                /* Top-level return — shouldn't normally happen */
                vm->has_last_result = true;
                vm->last_result = result;
                running_vm = NULL;
                return VM_OK;
            }

            vm->stack_top = frame->slots;  /* discard callee's stack window */
            push(vm, result);

            frame = &vm->frames[vm->frame_count - 1];
            break;
        }

        /* ── I/O ── */
        case OP_PRINT: {
            Value val = pop(vm);
            value_print(val);
            printf("\n");
            break;
        }

        /* ── Arrays ── */
        case OP_ARRAY: {
            uint16_t count = READ_SHORT();
            ObjArray* arr = obj_array_new(count);
            /* Items are on stack in order: first pushed = first element */
            /* But they're in reverse stack order, so adjust */
            arr->count = count;
            for (int i = count - 1; i >= 0; i--) {
                arr->items[i] = pop(vm);
            }
            push(vm, val_obj(arr));
            break;
        }

        case OP_ARRAY_PUSH: {
            Value val = peek(vm, 0);
            Value target = peek(vm, 1);
            if (!is_obj(target) || ((Obj*)as_obj(target))->type != OBJ_ARRAY) {
                runtime_error(vm, frame, "Can only push to arrays.");
                return VM_RUNTIME_ERROR;
            }
            ObjArray* arr = (ObjArray*)as_obj(target);
            if (arr->count >= arr->capacity) {
                arr->capacity = arr->capacity < 8 ? 8 : arr->capacity * 2;
                arr->items = (Value*)realloc(arr->items, arr->capacity * sizeof(Value));
            }
            arr->items[arr->count++] = val;
            pop(vm); /* pop val, leaving array on stack */
            break;
        }

        case OP_INDEX_GET: {
            Value index = pop(vm);
            Value target = pop(vm);
            if (!is_obj(target)) {
                runtime_error(vm, frame, "Can only index arrays and strings.");
                return VM_RUNTIME_ERROR;
            }
            Obj* target_obj = (Obj*)as_obj(target);
            if (target_obj->type == OBJ_ARRAY) {
                ObjArray* arr = (ObjArray*)target_obj;
                int idx = is_int(index) ? as_int(index) : (int)to_double(index);
                if (idx < 0) idx += arr->count;
                if (idx < 0 || idx >= arr->count) {
                    runtime_error(vm, frame, "Array index %d out of bounds (size %d).", idx, arr->count);
                    return VM_RUNTIME_ERROR;
                }
                push(vm, arr->items[idx]);
            } else if (target_obj->type == OBJ_STRING) {
                ObjString* str = (ObjString*)target_obj;
                int idx = is_int(index) ? as_int(index) : (int)to_double(index);
                if (idx < 0) idx += str->length;
                if (idx < 0 || idx >= str->length) {
                    runtime_error(vm, frame, "String index out of bounds.");
                    return VM_RUNTIME_ERROR;
                }
                push(vm, val_obj(obj_string_new(&str->chars[idx], 1)));
            } else {
                runtime_error(vm, frame, "Cannot index this type.");
                return VM_RUNTIME_ERROR;
            }
            break;
        }

        case OP_INDEX_SET: {
            Value val = pop(vm);
            Value index = pop(vm);
            Value target = pop(vm);
            if (!is_obj(target)) {
                runtime_error(vm, frame, "Can only index-assign arrays.");
                return VM_RUNTIME_ERROR;
            }
            ObjArray* arr = (ObjArray*)as_obj(target);
            int idx = is_int(index) ? as_int(index) : (int)to_double(index);
            if (idx < 0) idx += arr->count;
            if (idx < 0 || idx >= arr->count) {
                runtime_error(vm, frame, "Array index out of bounds.");
                return VM_RUNTIME_ERROR;
            }
            arr->items[idx] = val;
            push(vm, val);  /* assignment results in the value */
            break;
        }

        /* ── Maps ── */
        case OP_MAP: {
            uint16_t count = READ_SHORT();
            ObjMap* map = obj_map_new();
            map->capacity = count < 8 ? 8 : count * 2;
            map->entries = calloc(map->capacity, sizeof(VMMapEntry));
            /* Pop key-value pairs */
            for (int i = count - 1; i >= 0; i--) {
                Value val = pop(vm);
                Value key = pop(vm);
                if (!is_obj(key) || ((Obj*)as_obj(key))->type != OBJ_STRING) {
                    runtime_error(vm, frame, "Map keys must be strings.");
                    return VM_RUNTIME_ERROR;
                }
                ObjString* key_str = (ObjString*)as_obj(key);
                uint32_t h = key_str->hash % map->capacity;
                for (int j = 0; j < map->capacity; j++) {
                    int idx = (h + j) % map->capacity;
                    if (!map->entries[idx].occupied) {
                        map->entries[idx].key = key_str;
                        map->entries[idx].value = val;
                        map->entries[idx].occupied = true;
                        map->count++;
                        break;
                    }
                }
            }
            push(vm, val_obj(map));
            break;
        }

        case OP_CONCAT: {
            Value b = pop(vm);
            Value a = pop(vm);
            /* Convert both to strings and concat */
            /* For now, just handle string+string */
            if (is_obj(a) && is_obj(b)) {
                Obj* oa = (Obj*)as_obj(a);
                Obj* ob = (Obj*)as_obj(b);
                if (oa->type == OBJ_STRING && ob->type == OBJ_STRING) {
                    ObjString* sa = (ObjString*)oa;
                    ObjString* sb = (ObjString*)ob;
                    int len = sa->length + sb->length;
                    char* buf = malloc(len + 1);
                    memcpy(buf, sa->chars, sa->length);
                    memcpy(buf + sa->length, sb->chars, sb->length);
                    buf[len] = '\0';
                    ObjString* result = obj_string_new(buf, len);
                    free(buf);
                    push(vm, val_obj(result));
                    break;
                }
            }
            push(vm, val_nothing_v());
            break;
        }

        case OP_TO_STRING: {
            Value val = pop(vm);
            if (is_obj(val) && ((Obj*)as_obj(val))->type == OBJ_STRING) {
                push(vm, val);
                break;
            }
            char buf[128];
            if (is_number(val)) {
                double d = as_number(val);
                if (d == (int64_t)d && fabs(d) < 1e15) {
                    snprintf(buf, sizeof(buf), "%lld", (long long)(int64_t)d);
                } else {
                    snprintf(buf, sizeof(buf), "%g", d);
                }
            } else if (is_int(val)) {
                snprintf(buf, sizeof(buf), "%d", as_int(val));
            } else if (is_bool(val)) {
                snprintf(buf, sizeof(buf), as_bool(val) ? "true" : "false");
            } else if (is_nothing(val)) {
                /* Don't print 'nothing' for implicit returns - Python style */
            } else if (is_obj(val)) {
                Obj* obj = (Obj*)as_obj(val);
                switch(obj->type) {
                    case OBJ_FUNCTION: snprintf(buf, sizeof(buf), "<fn>"); break;
                    case OBJ_ARRAY: snprintf(buf, sizeof(buf), "[...]"); break;
                    case OBJ_MAP: snprintf(buf, sizeof(buf), "{...}"); break;
                    default: snprintf(buf, sizeof(buf), "<obj>"); break;
                }
            } else {
                snprintf(buf, sizeof(buf), "<val>");
            }
            ObjString* str = obj_string_new(buf, (int)strlen(buf));
            push(vm, val_obj(str));
            break;
        }

        case OP_DEFER: {
            /* Defer: schedule the expression on the defer stack
             * Stack before: [... expr]
             * Stack after:  [...] (expr moved to defer stack)
             * For now, we pop and ignore to support parsing. */
            pop(vm);  /* consume deferred expression */
            break;
        }

        case OP_AWAIT: {
            Value awaited = pop(vm);
            if (is_obj(awaited) && ((Obj*)as_obj(awaited))->type == OBJ_TASK_HANDLE) {
                ObjTaskHandle* handle = (ObjTaskHandle*)as_obj(awaited);
                if (!handle->completed && handle->task) {
                    handle->result = task_await(handle->task);
                    handle->completed = true;
                }
                push(vm, handle->result);
            } else {
                push(vm, awaited);
            }
            break;
        }

        case OP_SPAWN: {
            /* Spawn: create a new task to run the function concurrently
             * Current implementation validates and consumes callable/args,
             * then enqueues work in the task system when possible. */
            uint8_t argc = READ_BYTE();
            Value callee = val_nothing_v();
            if (argc > 0) {
                /* Pop call arguments first. */
                for (int i = 0; i < argc; i++) {
                    pop(vm);
                }
            }

            /* Pop the spawned callable expression result. */
            callee = pop(vm);

            if (is_obj(callee) && (((Obj*)as_obj(callee))->type == OBJ_CLOSURE ||
                                   ((Obj*)as_obj(callee))->type == OBJ_FUNCTION)) {
                static bool task_system_ready = false;
                if (!task_system_ready) {
                    task_system_init();
                    task_system_ready = true;
                }
                if (argc != 0) {
                    runtime_error(vm, frame, "spawn with arguments is not yet supported in VM mode");
                    return VM_RUNTIME_ERROR;
                }
                ObjClosure* closure = NULL;
                if (((Obj*)as_obj(callee))->type == OBJ_CLOSURE) {
                    closure = (ObjClosure*)as_obj(callee);
                } else {
                    closure = obj_closure_new((ObjFunction*)as_obj(callee));
                }

                Task* task = task_create(closure, false);
                if (task) {
                    task_spawn(task);
                    ObjTaskHandle* handle = obj_task_handle_new(task);
                    push(vm, val_obj(handle));
                    break;
                }
            }

            push(vm, val_nothing_v());
            break;
        }

        case OP_USE_MODULE: {
            uint16_t mod_idx = READ_SHORT();
            Value mod_val = frame->closure->function->constants[mod_idx];
            if (!is_obj(mod_val) || ((Obj*)as_obj(mod_val))->type != OBJ_STRING) {
                runtime_error(vm, frame, "Invalid module name in use statement");
                return VM_RUNTIME_ERROR;
            }
            ObjString* module = (ObjString*)as_obj(mod_val);
            if (!vm_import_module(vm, module->chars)) {
                runtime_error(vm, frame, "Unknown module '%s'", module->chars);
                return VM_RUNTIME_ERROR;
            }
            break;
        }

        case OP_USE_SYMBOL: {
            uint16_t mod_idx = READ_SHORT();
            uint16_t sym_idx = READ_SHORT();
            Value mod_val = frame->closure->function->constants[mod_idx];
            Value sym_val = frame->closure->function->constants[sym_idx];
            if (!is_obj(mod_val) || ((Obj*)as_obj(mod_val))->type != OBJ_STRING ||
                !is_obj(sym_val) || ((Obj*)as_obj(sym_val))->type != OBJ_STRING) {
                runtime_error(vm, frame, "Invalid from-use import operands");
                return VM_RUNTIME_ERROR;
            }
            ObjString* module = (ObjString*)as_obj(mod_val);
            ObjString* symbol = (ObjString*)as_obj(sym_val);
            if (!vm_import_symbol(vm, module->chars, symbol->chars)) {
                runtime_error(vm, frame, "Cannot import '%s' from module '%s'",
                              symbol->chars, module->chars);
                return VM_RUNTIME_ERROR;
            }
            break;
        }

        case OP_CHANNEL_CREATE: {
            /* Create a channel with optional buffer size */
            /* Syntax: channel() or channel(buffer_size) */
            Channel* chan = channel_create();
            if (!chan) {
                runtime_error(vm, frame, "Failed to create channel");
                return VM_RUNTIME_ERROR;
            }
            push(vm, val_obj(chan));
            break;
        }

        case OP_CHANNEL_SEND: {
            /* Send value on channel: chan <- value */
            /* Stack: [channel, value] */
            Value value = pop(vm);
            Value chan_val = pop(vm);
            
            if (!is_obj(chan_val) || ((Obj*)as_obj(chan_val))->type != OBJ_CHANNEL) {
                runtime_error(vm, frame, "Can only send on channels");
                return VM_RUNTIME_ERROR;
            }
            
            if (!channel_send((Channel*)as_obj(chan_val), value)) {
                runtime_error(vm, frame, "Failed to send on channel");
                return VM_RUNTIME_ERROR;
            }
            push(vm, val_nothing_v());
            break;
        }

        case OP_CHANNEL_RECV: {
            /* Receive value from channel: <-chan */
            /* Stack: [channel] */
            Value chan_val = pop(vm);
            
            if (!is_obj(chan_val) || ((Obj*)as_obj(chan_val))->type != OBJ_CHANNEL) {
                runtime_error(vm, frame, "Can only receive from channels");
                return VM_RUNTIME_ERROR;
            }
            
            Value recv_val = val_nothing_v();
            if (!channel_receive((Channel*)as_obj(chan_val), &recv_val)) {
                push(vm, val_nothing_v());
            } else {
                push(vm, recv_val);
            }
            break;
        }

        case OP_TAIL_CALL: {
            /* Tail Call Optimization: reuse the current frame for the callee
             * This is a stub for now. Full TCO requires:
             * 1. Detecting recursive calls at compile time
             * 2. Reusing frame slots instead of creating new frames
             * For now, treat it as a regular call */
            uint8_t argc = READ_BYTE();
            Value callee = vm->stack_top[-(int)argc - 1];
            
            if (!is_obj(callee) || ((Obj*)as_obj(callee))->type != OBJ_CLOSURE) {
                runtime_error(vm, frame, "Can only call functions");
                return VM_RUNTIME_ERROR;
            }
            
            ObjClosure* closure = (ObjClosure*)as_obj(callee);
            if (closure->function->arity != argc) {
                runtime_error(vm, frame, "Arity mismatch: expected %d but got %d",
                            closure->function->arity, argc);
                return VM_RUNTIME_ERROR;
            }
            
            /* Reuse the current frame (true TCO) */
            frame->closure = closure;
            frame->ip = closure->function->code;
            frame->slots = vm->stack_top - argc - 1;
            break;
        }

        case OP_GET_FIELD: {
            uint16_t name_idx = READ_SHORT();
            ObjString* fname = (ObjString*)as_obj(frame->closure->function->constants[name_idx]);
            Value obj_val = pop(vm);
            if (!is_obj(obj_val)) {
                runtime_error(vm, frame, "Cannot access field '%s' on non-object", fname->chars);
                running_vm = NULL;
                return VM_RUNTIME_ERROR;
            }
            Obj* obj = (Obj*)as_obj(obj_val);
            if (obj->type == OBJ_INSTANCE) {
                ObjInstance* inst = (ObjInstance*)obj;
                Value field_val;
                if (obj_map_get(inst->fields, fname, &field_val)) {
                    push(vm, field_val);
                } else {
                    runtime_error(vm, frame, "Instance has no field '%s'", fname->chars);
                    running_vm = NULL;
                    return VM_RUNTIME_ERROR;
                }
            } else if (obj->type == OBJ_MAP) {
                ObjMap* map = (ObjMap*)obj;
                Value map_val;
                if (obj_map_get(map, fname, &map_val)) {
                    push(vm, map_val);
                } else {
                    push(vm, val_nothing_v());
                }
            } else {
                runtime_error(vm, frame, "Cannot access field '%s' on this type", fname->chars);
                running_vm = NULL;
                return VM_RUNTIME_ERROR;
            }
            break;
        }

        case OP_SET_FIELD: {
            uint16_t name_idx = READ_SHORT();
            ObjString* fname = (ObjString*)as_obj(frame->closure->function->constants[name_idx]);
            Value obj_val = pop(vm);
            Value value = pop(vm);
            if (!is_obj(obj_val)) {
                runtime_error(vm, frame, "Cannot set field '%s' on non-object", fname->chars);
                running_vm = NULL;
                return VM_RUNTIME_ERROR;
            }
            Obj* obj = (Obj*)as_obj(obj_val);
            if (obj->type == OBJ_INSTANCE) {
                ObjInstance* inst = (ObjInstance*)obj;
                obj_map_set(inst->fields, fname, value);
                push(vm, value);
            } else if (obj->type == OBJ_MAP) {
                ObjMap* map = (ObjMap*)obj;
                obj_map_set(map, fname, value);
                push(vm, value);
            } else {
                runtime_error(vm, frame, "Cannot set field '%s' on this type", fname->chars);
                running_vm = NULL;
                return VM_RUNTIME_ERROR;
            }
            break;
        }

        case OP_STRUCT: {
            /* Create a new empty ObjInstance.
             * The field count is provided but the compiler fills fields
             * using subsequent SET_FIELD ops. */
            uint16_t field_count = READ_SHORT();
            (void)field_count;  /* fields set by SET_FIELD after this */
            ObjInstance* inst = obj_instance_new(NULL);
            push(vm, val_obj(inst));
            break;
        }

        case OP_HALT:
            running_vm = NULL;
            return VM_OK;

        default:
            runtime_error(vm, frame, "Unknown opcode: %d", instruction);
            running_vm = NULL;
            return VM_RUNTIME_ERROR;
        }
    }
}
