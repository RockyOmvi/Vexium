#include "vm.h"
#include "opcodes.h"
#include "task.h"
#include "json.h"
#include "time_module.h"
#include "http_module.h"
#include "tensor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef USE_CUDA
#include <cuda_runtime_api.h>
#include <cuda.h>
#endif
#ifdef USE_NVRTC
#include <nvrtc.h>
#endif
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

static Value clone_value_for_vm_internal(VM* target_vm, Value input, bool* ok, int depth);

static ObjFunction* clone_function_for_vm_internal(VM* target_vm, ObjFunction* src, bool* ok, int depth) {
    (void)target_vm;
    if (!ok || !*ok || !src) return NULL;
    if (depth > 64) {
        *ok = false;
        return NULL;
    }

    ObjFunction* dst = obj_function_new(src->name);
    if (!dst) {
        *ok = false;
        return NULL;
    }

    dst->arity = src->arity;
    dst->upvalue_count = src->upvalue_count;

    /* Native function object payload is stored opaquely in constants. */
    if (src->code == NULL && src->chunk_count == -1) {
        dst->chunk_count = -1;
        dst->chunk_capacity = 0;
        dst->code = NULL;
        dst->lines = NULL;
        dst->const_count = src->const_count;
        dst->const_capacity = src->const_count;
        if (src->const_count > 0) {
            dst->constants = (Value*)malloc(sizeof(Value) * (size_t)src->const_count);
            if (!dst->constants) {
                *ok = false;
                return NULL;
            }
            memcpy(dst->constants, src->constants, sizeof(Value) * (size_t)src->const_count);
        }
        return dst;
    }

    dst->chunk_count = src->chunk_count;
    dst->chunk_capacity = src->chunk_count;
    if (src->chunk_count > 0) {
        dst->code = (uint8_t*)malloc((size_t)src->chunk_count);
        dst->lines = (int*)malloc(sizeof(int) * (size_t)src->chunk_count);
        if (!dst->code || !dst->lines) {
            *ok = false;
            return NULL;
        }
        memcpy(dst->code, src->code, (size_t)src->chunk_count);
        memcpy(dst->lines, src->lines, sizeof(int) * (size_t)src->chunk_count);
    }

    dst->const_count = src->const_count;
    dst->const_capacity = src->const_count;
    if (src->const_count > 0) {
        dst->constants = (Value*)malloc(sizeof(Value) * (size_t)src->const_count);
        if (!dst->constants) {
            *ok = false;
            return NULL;
        }
        for (int i = 0; i < src->const_count; i++) {
            dst->constants[i] = clone_value_for_vm_internal(target_vm, src->constants[i], ok, depth + 1);
            if (!*ok) return NULL;
        }
    }

    return dst;
}

static Value clone_value_for_vm_internal(VM* target_vm, Value input, bool* ok, int depth) {
    if (!ok || !*ok) return val_nothing_v();
    if (depth > 64) {
        *ok = false;
        return val_nothing_v();
    }

    if (is_number(input) || is_int(input) || is_bool(input) || is_nothing(input)) {
        return input;
    }

    if (!is_obj(input)) {
        *ok = false;
        return val_nothing_v();
    }

    Obj* obj = (Obj*)as_obj(input);
    switch (obj->type) {
        case OBJ_STRING: {
            ObjString* s = (ObjString*)obj;
            return val_obj(obj_string_new(s->chars, s->length));
        }

        case OBJ_ARRAY: {
            ObjArray* src = (ObjArray*)obj;
            ObjArray* dst = obj_array_new(src->count < 8 ? 8 : src->count);
            for (int i = 0; i < src->count; i++) {
                Value cloned = clone_value_for_vm_internal(target_vm, src->items[i], ok, depth + 1);
                if (!*ok) return val_nothing_v();
                if (dst->count >= dst->capacity) {
                    dst->capacity *= 2;
                    dst->items = (Value*)realloc(dst->items, sizeof(Value) * (size_t)dst->capacity);
                    if (!dst->items) {
                        *ok = false;
                        return val_nothing_v();
                    }
                }
                dst->items[dst->count++] = cloned;
            }
            return val_obj(dst);
        }

        case OBJ_MAP: {
            ObjMap* src = (ObjMap*)obj;
            ObjMap* dst = obj_map_new();
            for (int i = 0; i < src->capacity; i++) {
                if (!src->entries[i].occupied) continue;
                ObjString* key = src->entries[i].key;
                ObjString* key_copy = obj_string_new(key->chars, key->length);
                Value val_copy = clone_value_for_vm_internal(target_vm, src->entries[i].value, ok, depth + 1);
                if (!*ok) return val_nothing_v();
                obj_map_set(dst, key_copy, val_copy);
            }
            return val_obj(dst);
        }

        case OBJ_TENSOR: {
            Tensor* src = (Tensor*)obj;
            Tensor* dst = tensor_clone(src);
            if (!dst) {
                *ok = false;
                return val_nothing_v();
            }
            return val_obj(dst);
        }

        case OBJ_FUNCTION: {
            ObjFunction* src = (ObjFunction*)obj;
            ObjFunction* dst = clone_function_for_vm_internal(target_vm, src, ok, depth + 1);
            if (!dst) return val_nothing_v();
            return val_obj(dst);
        }

        case OBJ_CLOSURE: {
            ObjClosure* src = (ObjClosure*)obj;
            if (src->upvalue_count != 0) {
                *ok = false;
                return val_nothing_v();
            }
            ObjFunction* fn_copy = clone_function_for_vm_internal(target_vm, src->function, ok, depth + 1);
            if (!fn_copy) return val_nothing_v();
            ObjClosure* cl_copy = obj_closure_new(fn_copy);
            if (!cl_copy) {
                *ok = false;
                return val_nothing_v();
            }
            return val_obj(cl_copy);
        }

        case OBJ_INSTANCE: {
            ObjInstance* src = (ObjInstance*)obj;
            ObjInstance* dst = obj_instance_new(src->struct_def);
            ObjMap* src_fields = src->fields;
            for (int i = 0; i < src_fields->capacity; i++) {
                if (!src_fields->entries[i].occupied) continue;
                ObjString* key = src_fields->entries[i].key;
                ObjString* key_copy = obj_string_new(key->chars, key->length);
                Value val_copy = clone_value_for_vm_internal(target_vm, src_fields->entries[i].value, ok, depth + 1);
                if (!*ok) return val_nothing_v();
                obj_map_set(dst->fields, key_copy, val_copy);
            }
            return val_obj(dst);
        }

        default:
            /* Unsupported cross-VM object transfer for now (channels/task handles/etc.). */
            *ok = false;
            return val_nothing_v();
    }
}

bool vm_clone_value_to_vm(VM* target_vm, Value input, Value* out) {
    if (!target_vm || !out) return false;
    VM* prev_vm = running_vm;
    running_vm = target_vm;

    bool ok = true;
    Value cloned = clone_value_for_vm_internal(target_vm, input, &ok, 0);
    if (ok) {
        *out = cloned;
    }

    running_vm = prev_vm;
    return ok;
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

static ObjTaskHandle* obj_task_handle_completed(Value result) {
    ObjTaskHandle* handle = (ObjTaskHandle*)allocate_obj(sizeof(ObjTaskHandle), OBJ_TASK_HANDLE);
    handle->task = NULL;
    handle->completed = true;
    handle->result = result;
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
    if (argc < 1 || !is_obj(args[0])) {
        return val_nothing_v();
    }

    Obj* src_obj = (Obj*)as_obj(args[0]);
    int length = 0;
    if (src_obj->type == OBJ_ARRAY) length = ((ObjArray*)src_obj)->count;
    else if (src_obj->type == OBJ_STRING) length = ((ObjString*)src_obj)->length;
    else return val_nothing_v();

    int start = 0;
    int end = length;
    int step = 1;
    if (argc >= 2 && !is_nothing(args[1])) start = is_int(args[1]) ? (int)as_int(args[1]) : (int)as_number(args[1]);
    if (argc >= 3 && !is_nothing(args[2])) end = is_int(args[2]) ? (int)as_int(args[2]) : (int)as_number(args[2]);
    if (argc >= 4 && !is_nothing(args[3])) step = is_int(args[3]) ? (int)as_int(args[3]) : (int)as_number(args[3]);
    if (step == 0) step = 1;

    if (start < 0) start += length;
    if (end < 0) end += length;

    if (step > 0) {
        if (start < 0) start = 0;
        if (start > length) start = length;
        if (end < 0) end = 0;
        if (end > length) end = length;
    } else {
        if (start < -1) start = -1;
        if (start >= length) start = length - 1;
        if (end < -1) end = -1;
        if (end >= length) end = length - 1;
    }

    if (src_obj->type == OBJ_ARRAY) {
        ObjArray* arr = (ObjArray*)src_obj;
        ObjArray* result = obj_array_new(length > 0 ? length : 0);
        if (step > 0) {
            for (int i = start; i < end; i += step) result->items[result->count++] = arr->items[i];
        } else {
            for (int i = start; i > end; i += step) result->items[result->count++] = arr->items[i];
        }
        return val_obj(result);
    }

    ObjString* src = (ObjString*)src_obj;
    char* buf = (char*)malloc((size_t)length + 1);
    int out = 0;
    if (step > 0) {
        for (int i = start; i < end; i += step) buf[out++] = src->chars[i];
    } else {
        for (int i = start; i > end; i += step) buf[out++] = src->chars[i];
    }
    ObjString* result = obj_string_new(buf, out);
    free(buf);
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

static Value native_gpu_available(int argc, Value* args) {
    UNUSED(argc);
    UNUSED(args);
#ifdef USE_CUDA
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    return val_bool(err == cudaSuccess && count > 0);
#else
    return val_bool(false);
#endif
}

static Value native_gpu_device_count(int argc, Value* args) {
    UNUSED(argc);
    UNUSED(args);
#ifdef USE_CUDA
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    if (err != cudaSuccess) return val_int(0);
    return val_int(count);
#else
    return val_int(0);
#endif
}

static Value native_gpu_backend(int argc, Value* args) {
    UNUSED(argc);
    UNUSED(args);
#ifdef USE_CUDA
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    if (err == cudaSuccess && count > 0) return val_obj(obj_string_new("cuda", 4));
#endif
    return val_obj(obj_string_new("cpu", 3));
}

typedef struct {
    char* name;
    char* entry_name;
    char* source;
    char* ptx;
} VMGpuKernelEntry;

typedef struct {
    int64_t handle;
    double* data;
    int64_t capacity;
    int64_t used;
    void* device_ptr;
} VMGpuBufferEntry;

static VMGpuKernelEntry* g_vm_gpu_kernels = NULL;
static int g_vm_gpu_kernel_count = 0;
static int g_vm_gpu_kernel_capacity = 0;
static VMGpuBufferEntry* g_vm_gpu_buffers = NULL;
static int g_vm_gpu_buffer_count = 0;
static int g_vm_gpu_buffer_capacity = 0;
static int64_t g_vm_gpu_next_buffer_handle = 1;

static int vm_gpu_buffer_find(int64_t handle) {
    for (int i = 0; i < g_vm_gpu_buffer_count; i++) {
        if (g_vm_gpu_buffers[i].handle == handle) return i;
    }
    return -1;
}

static int64_t vm_gpu_buffer_alloc(int64_t capacity) {
    if (capacity <= 0) return 0;

    if (g_vm_gpu_buffer_count >= g_vm_gpu_buffer_capacity) {
        int new_cap = g_vm_gpu_buffer_capacity == 0 ? 8 : g_vm_gpu_buffer_capacity * 2;
        VMGpuBufferEntry* grown = (VMGpuBufferEntry*)realloc(g_vm_gpu_buffers, sizeof(VMGpuBufferEntry) * new_cap);
        if (!grown) return 0;
        g_vm_gpu_buffers = grown;
        g_vm_gpu_buffer_capacity = new_cap;
    }

    VMGpuBufferEntry* entry = &g_vm_gpu_buffers[g_vm_gpu_buffer_count];
    entry->handle = g_vm_gpu_next_buffer_handle++;
    entry->data = (double*)calloc((size_t)capacity, sizeof(double));
    if (!entry->data) return 0;
    entry->capacity = capacity;
    entry->used = 0;
    entry->device_ptr = NULL;
#ifdef USE_CUDA
    cudaError_t alloc_err = cudaMalloc(&entry->device_ptr, (size_t)capacity * sizeof(double));
    if (alloc_err != cudaSuccess) {
        entry->device_ptr = NULL;
    }
#endif
    g_vm_gpu_buffer_count++;
    return entry->handle;
}

static bool vm_gpu_buffer_free(int64_t handle) {
    int idx = vm_gpu_buffer_find(handle);
    if (idx < 0) return false;

#ifdef USE_CUDA
    if (g_vm_gpu_buffers[idx].device_ptr) {
        cudaFree(g_vm_gpu_buffers[idx].device_ptr);
    }
#endif
    free(g_vm_gpu_buffers[idx].data);
    g_vm_gpu_buffer_count--;
    if (idx != g_vm_gpu_buffer_count) {
        g_vm_gpu_buffers[idx] = g_vm_gpu_buffers[g_vm_gpu_buffer_count];
    }
    return true;
}

static int vm_gpu_kernel_find(const char* name) {
    for (int i = 0; i < g_vm_gpu_kernel_count; i++) {
        if (strcmp(g_vm_gpu_kernels[i].name, name) == 0) return i;
    }
    return -1;
}

static bool vm_gpu_kernel_store(const char* name, const char* source) {
    int idx = vm_gpu_kernel_find(name);
    if (idx >= 0) {
        free(g_vm_gpu_kernels[idx].entry_name);
        g_vm_gpu_kernels[idx].entry_name = safe_strdup(name);
        free(g_vm_gpu_kernels[idx].source);
        g_vm_gpu_kernels[idx].source = safe_strdup(source);
        free(g_vm_gpu_kernels[idx].ptx);
        g_vm_gpu_kernels[idx].ptx = NULL;
        return g_vm_gpu_kernels[idx].entry_name != NULL && g_vm_gpu_kernels[idx].source != NULL;
    }

    if (g_vm_gpu_kernel_count >= g_vm_gpu_kernel_capacity) {
        int new_cap = g_vm_gpu_kernel_capacity == 0 ? 8 : g_vm_gpu_kernel_capacity * 2;
        VMGpuKernelEntry* grown = (VMGpuKernelEntry*)realloc(g_vm_gpu_kernels, sizeof(VMGpuKernelEntry) * new_cap);
        if (!grown) return false;
        g_vm_gpu_kernels = grown;
        g_vm_gpu_kernel_capacity = new_cap;
    }

    g_vm_gpu_kernels[g_vm_gpu_kernel_count].name = safe_strdup(name);
    g_vm_gpu_kernels[g_vm_gpu_kernel_count].entry_name = safe_strdup(name);
    g_vm_gpu_kernels[g_vm_gpu_kernel_count].source = safe_strdup(source);
    g_vm_gpu_kernels[g_vm_gpu_kernel_count].ptx = NULL;
    if (!g_vm_gpu_kernels[g_vm_gpu_kernel_count].name || !g_vm_gpu_kernels[g_vm_gpu_kernel_count].entry_name ||
        !g_vm_gpu_kernels[g_vm_gpu_kernel_count].source) return false;
    g_vm_gpu_kernel_count++;
    return true;
}

static bool vm_gpu_kernel_set_entry_name(const char* name, const char* entry_name) {
    int idx = vm_gpu_kernel_find(name);
    if (idx < 0) return false;
    free(g_vm_gpu_kernels[idx].entry_name);
    g_vm_gpu_kernels[idx].entry_name = safe_strdup(entry_name);
    return g_vm_gpu_kernels[idx].entry_name != NULL;
}

static bool vm_gpu_kernel_set_ptx(const char* name, const char* ptx) {
    int idx = vm_gpu_kernel_find(name);
    if (idx < 0) return false;
    free(g_vm_gpu_kernels[idx].ptx);
    g_vm_gpu_kernels[idx].ptx = safe_strdup(ptx);
    return g_vm_gpu_kernels[idx].ptx != NULL;
}

#ifdef USE_NVRTC
static bool vm_gpu_nvrtc_compile(const char* source, char** ptx_out, char** log_out) {
    nvrtcProgram prog;
    nvrtcResult r = nvrtcCreateProgram(&prog, source, "vexium_kernel.cu", 0, NULL, NULL);
    if (r != NVRTC_SUCCESS) return false;

    const char* opts[] = {"--gpu-architecture=compute_52"};
    r = nvrtcCompileProgram(prog, 1, opts);

    size_t log_size = 0;
    nvrtcGetProgramLogSize(prog, &log_size);
    if (log_size > 1 && log_out) {
        *log_out = (char*)malloc(log_size);
        if (*log_out) nvrtcGetProgramLog(prog, *log_out);
    }

    if (r == NVRTC_SUCCESS && ptx_out) {
        size_t ptx_size = 0;
        if (nvrtcGetPTXSize(prog, &ptx_size) == NVRTC_SUCCESS && ptx_size > 1) {
            *ptx_out = (char*)malloc(ptx_size);
            if (*ptx_out) {
                if (nvrtcGetPTX(prog, *ptx_out) != NVRTC_SUCCESS) {
                    free(*ptx_out);
                    *ptx_out = NULL;
                }
            }
        }
    }

    nvrtcDestroyProgram(&prog);
    return r == NVRTC_SUCCESS;
}
#endif

#if defined(USE_CUDA) && defined(USE_NVRTC)
static bool vm_gpu_launch_ptx_kernel(const char* ptx, const char* kernel_name, int blocks, int threads,
                                     void** kernel_params) {
    CUdevice dev;
    CUcontext ctx = NULL;
    CUcontext prev_ctx = NULL;
    CUmodule mod = NULL;
    CUfunction fn;
    bool ok = false;
    bool retained_primary = false;

    if (cuInit(0) != CUDA_SUCCESS) return false;
    if (cuCtxGetCurrent(&prev_ctx) != CUDA_SUCCESS) return false;

    if (prev_ctx) {
        ctx = prev_ctx;
    } else {
        if (cuDeviceGet(&dev, 0) != CUDA_SUCCESS) return false;
        if (cuDevicePrimaryCtxRetain(&ctx, dev) != CUDA_SUCCESS) return false;
        retained_primary = true;
        if (cuCtxSetCurrent(ctx) != CUDA_SUCCESS) goto cleanup;
    }

    if (cuModuleLoadDataEx(&mod, ptx, 0, NULL, NULL) != CUDA_SUCCESS) goto cleanup;
    if (cuModuleGetFunction(&fn, mod, kernel_name) != CUDA_SUCCESS) goto cleanup;
    if (cuLaunchKernel(fn, (unsigned int)blocks, 1, 1, (unsigned int)threads, 1, 1,
                       0, NULL, kernel_params, NULL) != CUDA_SUCCESS) {
        goto cleanup;
    }
    if (cuCtxSynchronize() != CUDA_SUCCESS) goto cleanup;
    ok = true;

cleanup:
    if (mod) cuModuleUnload(mod);
    if (retained_primary && prev_ctx) {
        cuCtxSetCurrent(prev_ctx);
    }
    if (retained_primary && ctx) {
        cuDevicePrimaryCtxRelease(dev);
    }
    return ok;
}

static bool vm_gpu_prepare_kernel_params(ObjArray* launch_args,
                                         void*** kernel_params_out,
                                         CUdeviceptr** ptr_storage_out,
                                         int64_t** int_storage_out,
                                         double** float_storage_out,
                                         int* out_count) {
    int count = launch_args ? launch_args->count : 0;
    *kernel_params_out = NULL;
    *ptr_storage_out = NULL;
    *int_storage_out = NULL;
    *float_storage_out = NULL;
    *out_count = count;
    if (count == 0) return true;

    void** kernel_params = (void**)calloc((size_t)count, sizeof(void*));
    CUdeviceptr* ptr_storage = (CUdeviceptr*)calloc((size_t)count, sizeof(CUdeviceptr));
    int64_t* int_storage = (int64_t*)calloc((size_t)count, sizeof(int64_t));
    double* float_storage = (double*)calloc((size_t)count, sizeof(double));
    if (!kernel_params || !ptr_storage || !int_storage || !float_storage) {
        free(kernel_params);
        free(ptr_storage);
        free(int_storage);
        free(float_storage);
        return false;
    }

    for (int i = 0; i < count; i++) {
        Value v = launch_args->items[i];
        if (is_int(v)) {
            int64_t iv = as_int(v);
            int buf_idx = vm_gpu_buffer_find(iv);
            if (buf_idx >= 0 && g_vm_gpu_buffers[buf_idx].device_ptr) {
                ptr_storage[i] = (CUdeviceptr)(uintptr_t)g_vm_gpu_buffers[buf_idx].device_ptr;
                kernel_params[i] = &ptr_storage[i];
            } else {
                int_storage[i] = iv;
                kernel_params[i] = &int_storage[i];
            }
            continue;
        }
        if (is_number(v)) {
            float_storage[i] = as_number(v);
            kernel_params[i] = &float_storage[i];
            continue;
        }

        free(kernel_params);
        free(ptr_storage);
        free(int_storage);
        free(float_storage);
        return false;
    }

    *kernel_params_out = kernel_params;
    *ptr_storage_out = ptr_storage;
    *int_storage_out = int_storage;
    *float_storage_out = float_storage;
    return true;
}
#endif

static double to_double(Value v);

static bool value_is_numeric(Value v) {
    return is_int(v) || is_number(v);
}

static Value native_gpu_dot(int argc, Value* args) {
    if (argc != 2 || !is_obj(args[0]) || !is_obj(args[1])) return val_nothing_v();
    if (((Obj*)as_obj(args[0]))->type != OBJ_ARRAY || ((Obj*)as_obj(args[1]))->type != OBJ_ARRAY) return val_nothing_v();

    ObjArray* a = (ObjArray*)as_obj(args[0]);
    ObjArray* b = (ObjArray*)as_obj(args[1]);
    if (a->count != b->count) return val_nothing_v();

    double sum = 0.0;
    bool all_int = true;
    for (int i = 0; i < a->count; i++) {
        if (!value_is_numeric(a->items[i]) || !value_is_numeric(b->items[i])) return val_nothing_v();
        if (!is_int(a->items[i]) || !is_int(b->items[i])) all_int = false;
        sum += to_double(a->items[i]) * to_double(b->items[i]);
    }
    return all_int ? val_int((int64_t)sum) : val_number(sum);
}

static Value native_gpu_matmul(int argc, Value* args) {
    if (argc != 2 || !is_obj(args[0]) || !is_obj(args[1])) return val_nothing_v();
    if (((Obj*)as_obj(args[0]))->type != OBJ_ARRAY || ((Obj*)as_obj(args[1]))->type != OBJ_ARRAY) return val_nothing_v();

    ObjArray* a_rows = (ObjArray*)as_obj(args[0]);
    ObjArray* b_rows = (ObjArray*)as_obj(args[1]);
    if (a_rows->count == 0 || b_rows->count == 0) return val_nothing_v();

    if (!is_obj(a_rows->items[0]) || !is_obj(b_rows->items[0])) return val_nothing_v();
    if (((Obj*)as_obj(a_rows->items[0]))->type != OBJ_ARRAY || ((Obj*)as_obj(b_rows->items[0]))->type != OBJ_ARRAY) return val_nothing_v();

    ObjArray* a0 = (ObjArray*)as_obj(a_rows->items[0]);
    ObjArray* b0 = (ObjArray*)as_obj(b_rows->items[0]);
    int m = a_rows->count;
    int k = a0->count;
    int k2 = b_rows->count;
    int n = b0->count;
    if (k != k2) return val_nothing_v();

    ObjArray* result = obj_array_new(m < 8 ? 8 : m);
    for (int i = 0; i < m; i++) {
        if (!is_obj(a_rows->items[i]) || ((Obj*)as_obj(a_rows->items[i]))->type != OBJ_ARRAY) return val_nothing_v();
        ObjArray* arow = (ObjArray*)as_obj(a_rows->items[i]);
        if (arow->count != k) return val_nothing_v();

        ObjArray* out_row = obj_array_new(n < 8 ? 8 : n);
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            bool all_int = true;
            for (int t = 0; t < k; t++) {
                if (!is_obj(b_rows->items[t]) || ((Obj*)as_obj(b_rows->items[t]))->type != OBJ_ARRAY) return val_nothing_v();
                ObjArray* brow = (ObjArray*)as_obj(b_rows->items[t]);
                if (brow->count != n) return val_nothing_v();

                Value av = arow->items[t];
                Value bv = brow->items[j];
                if (!value_is_numeric(av) || !value_is_numeric(bv)) return val_nothing_v();
                if (!is_int(av) || !is_int(bv)) all_int = false;
                sum += to_double(av) * to_double(bv);
            }

            if (out_row->count >= out_row->capacity) {
                out_row->capacity *= 2;
                out_row->items = realloc(out_row->items, out_row->capacity * sizeof(Value));
            }
            out_row->items[out_row->count++] = all_int ? val_int((int64_t)sum) : val_number(sum);
        }

        if (result->count >= result->capacity) {
            result->capacity *= 2;
            result->items = realloc(result->items, result->capacity * sizeof(Value));
        }
        result->items[result->count++] = val_obj(out_row);
    }

    return val_obj(result);
}

static Value native_gpu_kernel_compile(int argc, Value* args) {
    const char* name = NULL;
    const char* entry_name = NULL;
    const char* source = NULL;
    if (argc == 1 && is_obj(args[0]) && ((Obj*)as_obj(args[0]))->type == OBJ_STRING) {
        name = "default_kernel";
        entry_name = "default_kernel";
        source = ((ObjString*)as_obj(args[0]))->chars;
    } else if (argc == 2 && is_obj(args[0]) && is_obj(args[1]) &&
               ((Obj*)as_obj(args[0]))->type == OBJ_STRING &&
               ((Obj*)as_obj(args[1]))->type == OBJ_STRING) {
        name = ((ObjString*)as_obj(args[0]))->chars;
        entry_name = name;
        source = ((ObjString*)as_obj(args[1]))->chars;
    } else if (argc == 3 && is_obj(args[0]) && is_obj(args[1]) && is_obj(args[2]) &&
               ((Obj*)as_obj(args[0]))->type == OBJ_STRING &&
               ((Obj*)as_obj(args[1]))->type == OBJ_STRING &&
               ((Obj*)as_obj(args[2]))->type == OBJ_STRING) {
        name = ((ObjString*)as_obj(args[0]))->chars;
        entry_name = ((ObjString*)as_obj(args[1]))->chars;
        source = ((ObjString*)as_obj(args[2]))->chars;
    } else {
        return val_bool(false);
    }

    if (name[0] == '\0' || entry_name[0] == '\0' || source[0] == '\0') return val_bool(false);
    if (!vm_gpu_kernel_store(name, source)) return val_bool(false);
    if (!vm_gpu_kernel_set_entry_name(name, entry_name)) return val_bool(false);
#if defined(USE_CUDA) && defined(USE_NVRTC)
    if (strstr(source, "__global__") != NULL || strstr(source, "extern") != NULL) {
        char* ptx = NULL;
        char* log = NULL;
        bool ok = vm_gpu_nvrtc_compile(source, &ptx, &log);
        if (!ok && log) {
            fprintf(stderr, "[gpu] NVRTC compile failed for '%s':\n%s\n", name, log);
        }
        if (ok && ptx) {
            if (!vm_gpu_kernel_set_ptx(name, ptx)) ok = false;
        }
        free(ptx);
        free(log);
        return val_bool(ok);
    }
    return val_bool(true);
#else
    return val_bool(true);
#endif
}

static Value native_gpu_kernel_launch(int argc, Value* args) {
    if ((argc != 3 && argc != 4) || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_STRING) {
        return val_bool(false);
    }

    const char* name = ((ObjString*)as_obj(args[0]))->chars;
    int blocks = is_int(args[1]) ? (int)as_int(args[1]) : (int)as_number(args[1]);
    int threads = is_int(args[2]) ? (int)as_int(args[2]) : (int)as_number(args[2]);
    if (blocks <= 0 || threads <= 0) return val_bool(false);
    int kernel_idx = vm_gpu_kernel_find(name);
    if (kernel_idx < 0) return val_bool(false);

    ObjArray* launch_args = NULL;
    if (argc == 4) {
        if (!is_obj(args[3]) || ((Obj*)as_obj(args[3]))->type != OBJ_ARRAY) return val_bool(false);
        launch_args = (ObjArray*)as_obj(args[3]);
    }
#if defined(USE_CUDA) && defined(USE_NVRTC)
    if (g_vm_gpu_kernels[kernel_idx].ptx) {
        const char* entry_name = g_vm_gpu_kernels[kernel_idx].entry_name ? g_vm_gpu_kernels[kernel_idx].entry_name : name;
        void** kernel_params = NULL;
        CUdeviceptr* ptr_storage = NULL;
        int64_t* int_storage = NULL;
        double* float_storage = NULL;
        int param_count = 0;
        if (!vm_gpu_prepare_kernel_params(launch_args, &kernel_params, &ptr_storage, &int_storage, &float_storage, &param_count)) {
            return val_bool(false);
        }
        bool ok = vm_gpu_launch_ptx_kernel(g_vm_gpu_kernels[kernel_idx].ptx, entry_name, blocks, threads,
                                           param_count > 0 ? kernel_params : NULL);
        free(kernel_params);
        free(ptr_storage);
        free(int_storage);
        free(float_storage);
        return val_bool(ok);
    }
    return val_bool(true);
#elif defined(USE_CUDA)
    return val_bool(true);
#else
    return val_bool(true);
#endif
}

static Value native_gpu_alloc(int argc, Value* args) {
    if (argc != 1 || !is_int(args[0])) return val_int(0);
    return val_int(vm_gpu_buffer_alloc(as_int(args[0])));
}

static Value native_gpu_free(int argc, Value* args) {
    if (argc != 1 || !is_int(args[0])) return val_bool(false);
    return val_bool(vm_gpu_buffer_free(as_int(args[0])));
}

static Value native_gpu_write(int argc, Value* args) {
    if (argc != 2 || !is_int(args[0]) || !is_obj(args[1]) || ((Obj*)as_obj(args[1]))->type != OBJ_ARRAY) {
        return val_bool(false);
    }

    int idx = vm_gpu_buffer_find(as_int(args[0]));
    if (idx < 0) return val_bool(false);

    VMGpuBufferEntry* entry = &g_vm_gpu_buffers[idx];
    ObjArray* arr = (ObjArray*)as_obj(args[1]);
    if ((int64_t)arr->count > entry->capacity) return val_bool(false);

    for (int i = 0; i < arr->count; i++) {
        if (!value_is_numeric(arr->items[i])) return val_bool(false);
        entry->data[i] = to_double(arr->items[i]);
    }
    entry->used = arr->count;
#ifdef USE_CUDA
    if (entry->device_ptr && entry->used > 0) {
        cudaError_t err = cudaMemcpy(entry->device_ptr, entry->data,
                                     (size_t)entry->used * sizeof(double), cudaMemcpyHostToDevice);
        if (err != cudaSuccess) return val_bool(false);
    }
#endif
    return val_bool(true);
}

static Value native_gpu_read(int argc, Value* args) {
    if (argc != 1 && argc != 2) return val_nothing_v();
    if (!is_int(args[0])) return val_nothing_v();

    int idx = vm_gpu_buffer_find(as_int(args[0]));
    if (idx < 0) return val_nothing_v();

    VMGpuBufferEntry* entry = &g_vm_gpu_buffers[idx];
    int64_t count = entry->used;
    if (argc == 2) {
        if (!is_int(args[1]) || as_int(args[1]) < 0) return val_nothing_v();
        count = as_int(args[1]);
    }
    if (count > entry->used) count = entry->used;
    if (count > entry->capacity) count = entry->capacity;

#ifdef USE_CUDA
    if (entry->device_ptr && count > 0) {
        cudaError_t err = cudaMemcpy(entry->data, entry->device_ptr,
                                     (size_t)count * sizeof(double), cudaMemcpyDeviceToHost);
        if (err != cudaSuccess) return val_nothing_v();
    }
#endif

    ObjArray* out = obj_array_new((int)(count < 8 ? 8 : count));
    for (int64_t i = 0; i < count; i++) {
        if (out->count >= out->capacity) {
            out->capacity *= 2;
            out->items = realloc(out->items, out->capacity * sizeof(Value));
        }
        out->items[out->count++] = val_number(entry->data[i]);
    }
    return val_obj(out);
}

typedef struct {
    int64_t handle;
    Tensor* tensor;
} VMAiTensorEntry;

static VMAiTensorEntry* g_vm_ai_tensors = NULL;
static int g_vm_ai_tensor_count = 0;
static int g_vm_ai_tensor_capacity = 0;
static int64_t g_vm_ai_next_tensor_handle = 1;

static int vm_ai_tensor_find(int64_t handle) {
    for (int i = 0; i < g_vm_ai_tensor_count; i++) {
        if (g_vm_ai_tensors[i].handle == handle) return i;
    }
    return -1;
}

static int64_t vm_ai_tensor_store(Tensor* t) {
    if (!t) return 0;
    if (g_vm_ai_tensor_count >= g_vm_ai_tensor_capacity) {
        int new_cap = g_vm_ai_tensor_capacity == 0 ? 8 : g_vm_ai_tensor_capacity * 2;
        VMAiTensorEntry* grown = (VMAiTensorEntry*)realloc(g_vm_ai_tensors, sizeof(VMAiTensorEntry) * new_cap);
        if (!grown) {
            tensor_free(t);
            return 0;
        }
        g_vm_ai_tensors = grown;
        g_vm_ai_tensor_capacity = new_cap;
    }

    int64_t handle = g_vm_ai_next_tensor_handle++;
    g_vm_ai_tensors[g_vm_ai_tensor_count].handle = handle;
    g_vm_ai_tensors[g_vm_ai_tensor_count].tensor = t;
    g_vm_ai_tensor_count++;
    return handle;
}

static bool vm_ai_shape_from_array(ObjArray* arr, int** shape_out, int* ndim_out) {
    if (!arr || arr->count <= 0) return false;
    int* shape = (int*)malloc(sizeof(int) * arr->count);
    if (!shape) return false;

    for (int i = 0; i < arr->count; i++) {
        if (!is_int(arr->items[i]) || as_int(arr->items[i]) <= 0) {
            free(shape);
            return false;
        }
        shape[i] = (int)as_int(arr->items[i]);
    }

    *shape_out = shape;
    *ndim_out = arr->count;
    return true;
}

static bool vm_ai_value_to_double(Value v, double* out) {
    if (out == NULL) return false;
    if (is_number(v)) {
        *out = as_number(v);
        return true;
    }
    if (is_int(v)) {
        *out = (double)as_int(v);
        return true;
    }
    return false;
}

static Tensor* vm_ai_tensor_from_array_value(ObjArray* arr) {
    if (arr == NULL || arr->count <= 0) return NULL;

    if (is_obj(arr->items[0]) && ((Obj*)as_obj(arr->items[0]))->type == OBJ_ARRAY) {
        int rows = arr->count;
        int cols = ((ObjArray*)as_obj(arr->items[0]))->count;
        int shape[2] = {rows, cols};
        Tensor* t = NULL;
        if (cols <= 0) return NULL;
        for (int r = 0; r < rows; r++) {
            if (!is_obj(arr->items[r]) || ((Obj*)as_obj(arr->items[r]))->type != OBJ_ARRAY || ((ObjArray*)as_obj(arr->items[r]))->count != cols) {
                return NULL;
            }
        }
        t = tensor_new(TENSOR_FLOAT32, 2, shape);
        if (t == NULL) return NULL;
        for (int r = 0; r < rows; r++) {
            ObjArray* row = (ObjArray*)as_obj(arr->items[r]);
            for (int c = 0; c < cols; c++) {
                double val = 0.0;
                if (!vm_ai_value_to_double(row->items[c], &val)) {
                    tensor_free(t);
                    return NULL;
                }
                t->data.f32_data[r * cols + c] = (float)val;
            }
        }
        return t;
    }

    {
        int n = arr->count;
        int shape[1] = {n};
        Tensor* t = tensor_new(TENSOR_FLOAT32, 1, shape);
        if (t == NULL) return NULL;
        for (int i = 0; i < n; i++) {
            double val = 0.0;
            if (!vm_ai_value_to_double(arr->items[i], &val)) {
                tensor_free(t);
                return NULL;
            }
            t->data.f32_data[i] = (float)val;
        }
        return t;
    }
}

static bool vm_ai_tensor_resolve_arg(Value arg, Tensor** tensor_out, bool* owns_tensor) {
    if (tensor_out == NULL || owns_tensor == NULL) return false;
    *tensor_out = NULL;
    *owns_tensor = false;

    if (is_int(arg)) {
        int idx = vm_ai_tensor_find(as_int(arg));
        if (idx < 0) return false;
        *tensor_out = g_vm_ai_tensors[idx].tensor;
        return true;
    }

    if (is_obj(arg) && ((Obj*)as_obj(arg))->type == OBJ_ARRAY) {
        Tensor* tmp = vm_ai_tensor_from_array_value((ObjArray*)as_obj(arg));
        if (tmp == NULL) return false;
        *tensor_out = tmp;
        *owns_tensor = true;
        return true;
    }

    return false;
}

static Value native_ai_tensor_from_array(int argc, Value* args) {
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY) return val_int(0);
    return val_int(vm_ai_tensor_store(vm_ai_tensor_from_array_value((ObjArray*)as_obj(args[0]))));
}

static Value native_ai_tensor_to_array(int argc, Value* args) {
    if (argc != 1 || !is_int(args[0])) return val_nothing_v();
    {
        int idx = vm_ai_tensor_find(as_int(args[0]));
        Tensor* t;
        if (idx < 0) return val_nothing_v();
        t = g_vm_ai_tensors[idx].tensor;

        if (t->ndim == 1) {
            int n = t->shape[0];
            ObjArray* out = obj_array_new(n > 0 ? n : 1);
            for (int i = 0; i < n; i++) {
                if (out->count >= out->capacity) {
                    out->capacity *= 2;
                    out->items = realloc(out->items, out->capacity * sizeof(Value));
                }
                out->items[out->count++] = val_number(t->data.f32_data[i]);
            }
            return val_obj(out);
        }

        if (t->ndim == 2) {
            int rows = t->shape[0];
            int cols = t->shape[1];
            ObjArray* out = obj_array_new(rows > 0 ? rows : 1);
            for (int r = 0; r < rows; r++) {
                ObjArray* row = obj_array_new(cols > 0 ? cols : 1);
                for (int c = 0; c < cols; c++) {
                    if (row->count >= row->capacity) {
                        row->capacity *= 2;
                        row->items = realloc(row->items, row->capacity * sizeof(Value));
                    }
                    row->items[row->count++] = val_number(t->data.f32_data[r * cols + c]);
                }
                if (out->count >= out->capacity) {
                    out->capacity *= 2;
                    out->items = realloc(out->items, out->capacity * sizeof(Value));
                }
                out->items[out->count++] = val_obj(row);
            }
            return val_obj(out);
        }
    }
    return val_nothing_v();
}

static Value native_ai_tensor_zeros(int argc, Value* args) {
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY) return val_int(0);
    int* shape = NULL;
    int ndim = 0;
    if (!vm_ai_shape_from_array((ObjArray*)as_obj(args[0]), &shape, &ndim)) return val_int(0);
    Tensor* t = tensor_zeros(TENSOR_FLOAT32, ndim, shape);
    free(shape);
    return val_int(vm_ai_tensor_store(t));
}

static Value native_ai_tensor_rand(int argc, Value* args) {
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY) return val_int(0);
    int* shape = NULL;
    int ndim = 0;
    if (!vm_ai_shape_from_array((ObjArray*)as_obj(args[0]), &shape, &ndim)) return val_int(0);
    Tensor* t = tensor_rand(ndim, shape);
    free(shape);
    return val_int(vm_ai_tensor_store(t));
}

static Value native_ai_tensor_matmul(int argc, Value* args) {
    Tensor *a = NULL, *b = NULL, *out = NULL;
    bool own_a = false, own_b = false;
    int64_t handle;
    if (argc != 2) return val_int(0);
    if (!vm_ai_tensor_resolve_arg(args[0], &a, &own_a) || !vm_ai_tensor_resolve_arg(args[1], &b, &own_b)) {
        if (own_a && a) tensor_free(a);
        if (own_b && b) tensor_free(b);
        return val_int(0);
    }
    out = tensor_matmul(a, b);
    if (own_a && a) tensor_free(a);
    if (own_b && b) tensor_free(b);
    handle = vm_ai_tensor_store(out);
    return val_int(handle);
}

static Value native_ai_tensor_sum(int argc, Value* args) {
    Tensor* t = NULL;
    bool own_t = false;
    double sum;
    if (argc != 1) return val_nothing_v();
    if (!vm_ai_tensor_resolve_arg(args[0], &t, &own_t)) return val_nothing_v();
    sum = tensor_sum(t);
    if (own_t) tensor_free(t);
    return val_number(sum);
}

static Value native_ai_tensor_shape(int argc, Value* args) {
    if (argc != 1 || !is_int(args[0])) return val_nothing_v();
    int idx = vm_ai_tensor_find(as_int(args[0]));
    if (idx < 0) return val_nothing_v();

    Tensor* t = g_vm_ai_tensors[idx].tensor;
    ObjArray* out = obj_array_new(t->ndim < 8 ? 8 : t->ndim);
    for (int i = 0; i < t->ndim; i++) {
        if (out->count >= out->capacity) {
            out->capacity *= 2;
            out->items = realloc(out->items, out->capacity * sizeof(Value));
        }
        out->items[out->count++] = val_int(t->shape[i]);
    }
    return val_obj(out);
}

static Value native_ai_tensor_free(int argc, Value* args) {
    if (argc != 1 || !is_int(args[0])) return val_bool(false);
    int idx = vm_ai_tensor_find(as_int(args[0]));
    if (idx < 0) return val_bool(false);

    tensor_free(g_vm_ai_tensors[idx].tensor);
    g_vm_ai_tensor_count--;
    if (idx != g_vm_ai_tensor_count) {
        g_vm_ai_tensors[idx] = g_vm_ai_tensors[g_vm_ai_tensor_count];
    }
    return val_bool(true);
}

static Value native_ai_tokenize(int argc, Value* args) {
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_STRING) return val_nothing_v();
    ObjString* s = (ObjString*)as_obj(args[0]);
    ObjArray* out = obj_array_new(8);

    int i = 0;
    while (i < s->length) {
        while (i < s->length && isspace((unsigned char)s->chars[i])) i++;
        if (i >= s->length) break;
        int start = i;
        while (i < s->length && !isspace((unsigned char)s->chars[i])) i++;

        if (out->count >= out->capacity) {
            out->capacity *= 2;
            out->items = realloc(out->items, out->capacity * sizeof(Value));
        }
        out->items[out->count++] = val_obj(obj_string_new(s->chars + start, i - start));
    }
    return val_obj(out);
}

/* ── Extended AI VM: tensor creation ── */
static Value native_ai_tensor_ones(int argc, Value* args) {
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY) return val_int(0);
    int* shape = NULL; int ndim = 0;
    if (!vm_ai_shape_from_array((ObjArray*)as_obj(args[0]), &shape, &ndim)) return val_int(0);
    Tensor* t = tensor_ones(TENSOR_FLOAT32, ndim, shape);
    free(shape);
    return val_int(vm_ai_tensor_store(t));
}

static Value native_ai_tensor_randn(int argc, Value* args) {
    if (argc < 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY) return val_int(0);
    int* shape = NULL; int ndim = 0;
    if (!vm_ai_shape_from_array((ObjArray*)as_obj(args[0]), &shape, &ndim)) return val_int(0);
    double mean = 0.0, std_val = 1.0;
    if (argc >= 2) {
        if (is_number(args[1])) mean = as_number(args[1]);
        else if (is_int(args[1])) mean = (double)as_int(args[1]);
    }
    if (argc >= 3) {
        if (is_number(args[2])) std_val = as_number(args[2]);
        else if (is_int(args[2])) std_val = (double)as_int(args[2]);
    }
    Tensor* t = tensor_randn(ndim, shape, mean, std_val);
    free(shape);
    return val_int(vm_ai_tensor_store(t));
}

static Value native_ai_tensor_fill(int argc, Value* args) {
    if (argc < 2 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY) return val_int(0);
    double v = is_number(args[1]) ? as_number(args[1]) : is_int(args[1]) ? (double)as_int(args[1]) : 0.0;
    int* shape = NULL; int ndim = 0;
    if (!vm_ai_shape_from_array((ObjArray*)as_obj(args[0]), &shape, &ndim)) return val_int(0);
    Tensor* t = tensor_full(TENSOR_FLOAT32, v, ndim, shape);
    free(shape);
    return val_int(vm_ai_tensor_store(t));
}

static Value native_ai_tensor_clone(int argc, Value* args) {
    if (argc != 1 || !is_int(args[0])) return val_int(0);
    int idx = vm_ai_tensor_find(as_int(args[0]));
    if (idx < 0) return val_int(0);
    return val_int(vm_ai_tensor_store(tensor_clone(g_vm_ai_tensors[idx].tensor)));
}

/* ── Extended AI VM: binary element-wise ops ── */
static Value native_ai_tensor_add(int argc, Value* args) {
    Tensor *a = NULL, *b = NULL;
    bool own_a = false, own_b = false;
    Tensor* out;
    if (argc != 2) return val_int(0);
    if (!vm_ai_tensor_resolve_arg(args[0], &a, &own_a) || !vm_ai_tensor_resolve_arg(args[1], &b, &own_b)) {
        if (own_a && a) tensor_free(a);
        if (own_b && b) tensor_free(b);
        return val_int(0);
    }
    out = tensor_clone(a);
    if (!out) {
        if (own_a && a) tensor_free(a);
        if (own_b && b) tensor_free(b);
        return val_int(0);
    }
    tensor_iadd(out, b);
    if (own_a && a) tensor_free(a);
    if (own_b && b) tensor_free(b);
    return val_int(vm_ai_tensor_store(out));
}

static Value native_ai_tensor_sub(int argc, Value* args) {
    Tensor *a = NULL, *b = NULL;
    bool own_a = false, own_b = false;
    Tensor* out;
    if (argc != 2) return val_int(0);
    if (!vm_ai_tensor_resolve_arg(args[0], &a, &own_a) || !vm_ai_tensor_resolve_arg(args[1], &b, &own_b)) {
        if (own_a && a) tensor_free(a);
        if (own_b && b) tensor_free(b);
        return val_int(0);
    }
    out = tensor_clone(a);
    if (!out) {
        if (own_a && a) tensor_free(a);
        if (own_b && b) tensor_free(b);
        return val_int(0);
    }
    tensor_isub(out, b);
    if (own_a && a) tensor_free(a);
    if (own_b && b) tensor_free(b);
    return val_int(vm_ai_tensor_store(out));
}

static Value native_ai_tensor_mul(int argc, Value* args) {
    Tensor *a = NULL, *b = NULL;
    bool own_a = false, own_b = false;
    Tensor* out;
    if (argc != 2) return val_int(0);
    if (!vm_ai_tensor_resolve_arg(args[0], &a, &own_a) || !vm_ai_tensor_resolve_arg(args[1], &b, &own_b)) {
        if (own_a && a) tensor_free(a);
        if (own_b && b) tensor_free(b);
        return val_int(0);
    }
    out = tensor_clone(a);
    if (!out) {
        if (own_a && a) tensor_free(a);
        if (own_b && b) tensor_free(b);
        return val_int(0);
    }
    tensor_imul(out, b);
    if (own_a && a) tensor_free(a);
    if (own_b && b) tensor_free(b);
    return val_int(vm_ai_tensor_store(out));
}

static Value native_ai_tensor_div(int argc, Value* args) {
    Tensor *a = NULL, *b = NULL;
    bool own_a = false, own_b = false;
    Tensor* out;
    if (argc != 2) return val_int(0);
    if (!vm_ai_tensor_resolve_arg(args[0], &a, &own_a) || !vm_ai_tensor_resolve_arg(args[1], &b, &own_b)) {
        if (own_a && a) tensor_free(a);
        if (own_b && b) tensor_free(b);
        return val_int(0);
    }
    out = tensor_clone(a);
    if (!out) {
        if (own_a && a) tensor_free(a);
        if (own_b && b) tensor_free(b);
        return val_int(0);
    }
    tensor_idiv(out, b);
    if (own_a && a) tensor_free(a);
    if (own_b && b) tensor_free(b);
    return val_int(vm_ai_tensor_store(out));
}

static Value native_ai_tensor_scale(int argc, Value* args) {
    Tensor* t = NULL;
    bool own_t = false;
    if (argc != 2) return val_int(0);
    double s = is_number(args[1]) ? as_number(args[1]) : is_int(args[1]) ? (double)as_int(args[1]) : 1.0;
    Tensor* out;
    if (!vm_ai_tensor_resolve_arg(args[0], &t, &own_t)) return val_int(0);
    out = tensor_clone(t);
    if (!out) {
        if (own_t) tensor_free(t);
        return val_int(0);
    }
    tensor_imul_scalar(out, s);
    if (own_t) tensor_free(t);
    return val_int(vm_ai_tensor_store(out));
}

/* ── Extended AI VM: activations ── */
static Value native_ai_tensor_relu(int argc, Value* args) {
    Tensor* t = NULL;
    bool own_t = false;
    Tensor* out;
    if (argc != 1) return val_int(0);
    if (!vm_ai_tensor_resolve_arg(args[0], &t, &own_t)) return val_int(0);
    out = tensor_relu(t);
    if (own_t) tensor_free(t);
    return val_int(vm_ai_tensor_store(out));
}

static Value native_ai_tensor_sigmoid(int argc, Value* args) {
    Tensor* t = NULL;
    bool own_t = false;
    Tensor* out;
    if (argc != 1) return val_int(0);
    if (!vm_ai_tensor_resolve_arg(args[0], &t, &own_t)) return val_int(0);
    out = tensor_sigmoid(t);
    if (own_t) tensor_free(t);
    return val_int(vm_ai_tensor_store(out));
}

static Value native_ai_tensor_tanh(int argc, Value* args) {
    Tensor* t = NULL;
    bool own_t = false;
    Tensor* out;
    if (argc != 1) return val_int(0);
    if (!vm_ai_tensor_resolve_arg(args[0], &t, &own_t)) return val_int(0);
    out = tensor_tanh(t);
    if (own_t) tensor_free(t);
    return val_int(vm_ai_tensor_store(out));
}

static Value native_ai_tensor_softmax(int argc, Value* args) {
    Tensor* t = NULL;
    bool own_t = false;
    Tensor* out;
    if (argc != 1) return val_int(0);
    if (!vm_ai_tensor_resolve_arg(args[0], &t, &own_t)) return val_int(0);
    out = tensor_softmax(t);
    if (own_t) tensor_free(t);
    return val_int(vm_ai_tensor_store(out));
}

/* ── Extended AI VM: shape ops ── */
static Value native_ai_tensor_reshape(int argc, Value* args) {
    Tensor* t = NULL;
    bool own_t = false;
    if (argc != 2 || !is_obj(args[1]) || ((Obj*)as_obj(args[1]))->type != OBJ_ARRAY) return val_int(0);
    if (!vm_ai_tensor_resolve_arg(args[0], &t, &own_t)) return val_int(0);
    int* shape = NULL; int ndim = 0;
    if (!vm_ai_shape_from_array((ObjArray*)as_obj(args[1]), &shape, &ndim)) return val_int(0);
    Tensor* out = tensor_reshape(t, ndim, shape);
    free(shape);
    if (own_t) tensor_free(t);
    return val_int(vm_ai_tensor_store(out));
}

static Value native_ai_tensor_transpose(int argc, Value* args) {
    Tensor* t = NULL;
    bool own_t = false;
    Tensor* out;
    if (argc != 1) return val_int(0);
    if (!vm_ai_tensor_resolve_arg(args[0], &t, &own_t)) return val_int(0);
    out = tensor_transpose(t);
    if (own_t) tensor_free(t);
    return val_int(vm_ai_tensor_store(out));
}

/* ── Extended AI VM: reductions ── */
static Value native_ai_tensor_mean(int argc, Value* args) {
    Tensor* t = NULL;
    bool own_t = false;
    double val;
    if (argc != 1) return val_nothing_v();
    if (!vm_ai_tensor_resolve_arg(args[0], &t, &own_t)) return val_nothing_v();
    val = tensor_mean(t);
    if (own_t) tensor_free(t);
    return val_number(val);
}

static Value native_ai_tensor_std(int argc, Value* args) {
    Tensor* t = NULL;
    bool own_t = false;
    double val;
    if (argc != 1) return val_nothing_v();
    if (!vm_ai_tensor_resolve_arg(args[0], &t, &own_t)) return val_nothing_v();
    val = tensor_std(t);
    if (own_t) tensor_free(t);
    return val_number(val);
}

static Value native_ai_tensor_max(int argc, Value* args) {
    Tensor* t = NULL;
    bool own_t = false;
    double val;
    if (argc != 1) return val_nothing_v();
    if (!vm_ai_tensor_resolve_arg(args[0], &t, &own_t)) return val_nothing_v();
    val = tensor_max(t);
    if (own_t) tensor_free(t);
    return val_number(val);
}

static Value native_ai_tensor_min(int argc, Value* args) {
    Tensor* t = NULL;
    bool own_t = false;
    double val;
    if (argc != 1) return val_nothing_v();
    if (!vm_ai_tensor_resolve_arg(args[0], &t, &own_t)) return val_nothing_v();
    val = tensor_min(t);
    if (own_t) tensor_free(t);
    return val_number(val);
}

/* ── Extended AI VM: optimizer ── */
typedef struct {
    int64_t handle;
    void* opt;
    OptimizerType opt_type;
} VMAiOptimizerEntry;

static VMAiOptimizerEntry* g_vm_ai_optimizers = NULL;
static int g_vm_ai_opt_count = 0;
static int g_vm_ai_opt_capacity = 0;
static int64_t g_vm_ai_next_opt_handle = 1;

static int vm_ai_opt_find(int64_t handle) {
    for (int i = 0; i < g_vm_ai_opt_count; i++) {
        if (g_vm_ai_optimizers[i].handle == handle) return i;
    }
    return -1;
}

static int64_t vm_ai_opt_store(void* opt, OptimizerType type) {
    if (!opt) return 0;
    if (g_vm_ai_opt_count >= g_vm_ai_opt_capacity) {
        int new_cap = g_vm_ai_opt_capacity == 0 ? 8 : g_vm_ai_opt_capacity * 2;
        VMAiOptimizerEntry* grown = (VMAiOptimizerEntry*)realloc(g_vm_ai_optimizers, sizeof(VMAiOptimizerEntry) * new_cap);
        if (!grown) { optimizer_free(opt, type); return 0; }
        g_vm_ai_optimizers = grown;
        g_vm_ai_opt_capacity = new_cap;
    }
    int64_t handle = g_vm_ai_next_opt_handle++;
    g_vm_ai_optimizers[g_vm_ai_opt_count].handle = handle;
    g_vm_ai_optimizers[g_vm_ai_opt_count].opt = opt;
    g_vm_ai_optimizers[g_vm_ai_opt_count].opt_type = type;
    g_vm_ai_opt_count++;
    return handle;
}

static Value native_ai_optimizer_create(int argc, Value* args) {
    if (argc < 2 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_STRING) return val_int(0);
    const char* type_str = ((ObjString*)as_obj(args[0]))->chars;
    double lr = is_number(args[1]) ? as_number(args[1]) : is_int(args[1]) ? (double)as_int(args[1]) : 0.001;
    OptimizerType type = OPTIMIZER_SGD;
    if (strcmp(type_str, "adam") == 0)         type = OPTIMIZER_ADAM;
    else if (strcmp(type_str, "rmsprop") == 0) type = OPTIMIZER_RMSPROP;
    void* opt = optimizer_create(type, lr);
    return val_int(vm_ai_opt_store(opt, type));
}

static Value native_ai_optimizer_step(int argc, Value* args) {
    if (argc != 3 || !is_int(args[0]) || !is_int(args[1]) || !is_int(args[2])) return val_bool(false);
    int oi = vm_ai_opt_find(as_int(args[0]));
    int pi = vm_ai_tensor_find(as_int(args[1]));
    int gi = vm_ai_tensor_find(as_int(args[2]));
    if (oi < 0 || pi < 0 || gi < 0) return val_bool(false);
    optimizer_update(g_vm_ai_optimizers[oi].opt, g_vm_ai_optimizers[oi].opt_type,
                     g_vm_ai_tensors[pi].tensor, g_vm_ai_tensors[gi].tensor);
    return val_bool(true);
}

static Value native_ai_optimizer_free(int argc, Value* args) {
    if (argc != 1 || !is_int(args[0])) return val_bool(false);
    int idx = vm_ai_opt_find(as_int(args[0]));
    if (idx < 0) return val_bool(false);
    optimizer_free(g_vm_ai_optimizers[idx].opt, g_vm_ai_optimizers[idx].opt_type);
    g_vm_ai_opt_count--;
    if (idx != g_vm_ai_opt_count) g_vm_ai_optimizers[idx] = g_vm_ai_optimizers[g_vm_ai_opt_count];
    return val_bool(true);
}

/* ── Extended AI VM: neural network model ── */
typedef struct {
    int64_t handle;
    NNModel* model;
} VMAiModelEntry;

static VMAiModelEntry* g_vm_ai_models = NULL;
static int g_vm_ai_model_count = 0;
static int g_vm_ai_model_capacity = 0;
static int64_t g_vm_ai_next_model_handle = 1;

static int vm_ai_model_find(int64_t handle) {
    for (int i = 0; i < g_vm_ai_model_count; i++) {
        if (g_vm_ai_models[i].handle == handle) return i;
    }
    return -1;
}

static int64_t vm_ai_model_store(NNModel* m) {
    if (!m) return 0;
    if (g_vm_ai_model_count >= g_vm_ai_model_capacity) {
        int new_cap = g_vm_ai_model_capacity == 0 ? 8 : g_vm_ai_model_capacity * 2;
        VMAiModelEntry* grown = (VMAiModelEntry*)realloc(g_vm_ai_models, sizeof(VMAiModelEntry) * new_cap);
        if (!grown) { nn_model_free(m); return 0; }
        g_vm_ai_models = grown;
        g_vm_ai_model_capacity = new_cap;
    }
    int64_t handle = g_vm_ai_next_model_handle++;
    g_vm_ai_models[g_vm_ai_model_count].handle = handle;
    g_vm_ai_models[g_vm_ai_model_count].model = m;
    g_vm_ai_model_count++;
    return handle;
}

static Value native_ai_nn_new(int argc, Value* args) {
    const char* name = "model";
    if (argc >= 1 && is_obj(args[0]) && ((Obj*)as_obj(args[0]))->type == OBJ_STRING)
        name = ((ObjString*)as_obj(args[0]))->chars;
    return val_int(vm_ai_model_store(nn_model_new(name)));
}

static Value native_ai_nn_add_dense(int argc, Value* args) {
    if (argc < 2 || !is_int(args[0]) || !is_int(args[1])) return val_bool(false);
    int idx = vm_ai_model_find(as_int(args[0]));
    if (idx < 0) return val_bool(false);
    int units = (int)as_int(args[1]);
    ActivationType act = ACTIVATION_NONE;
    if (argc >= 3 && is_obj(args[2]) && ((Obj*)as_obj(args[2]))->type == OBJ_STRING) {
        const char* s = ((ObjString*)as_obj(args[2]))->chars;
        if (strcmp(s, "relu") == 0)         act = ACTIVATION_RELU;
        else if (strcmp(s, "sigmoid") == 0) act = ACTIVATION_SIGMOID;
        else if (strcmp(s, "tanh") == 0)    act = ACTIVATION_TANH;
        else if (strcmp(s, "softmax") == 0) act = ACTIVATION_SOFTMAX;
    }
    int in_sz = 0;
    if (argc >= 4 && is_int(args[3])) in_sz = (int)as_int(args[3]);
    nn_model_add_dense(g_vm_ai_models[idx].model, units, act, in_sz);
    return val_bool(true);
}

static Value native_ai_nn_add_conv2d(int argc, Value* args) {
    if (argc < 3 || !is_int(args[0]) || !is_int(args[1]) || !is_int(args[2])) return val_bool(false);
    int idx = vm_ai_model_find(as_int(args[0]));
    if (idx < 0) return val_bool(false);
    int filters = (int)as_int(args[1]);
    int kernel = (int)as_int(args[2]);
    int stride = (argc >= 4 && is_int(args[3])) ? (int)as_int(args[3]) : 1;
    int padding = (argc >= 5 && is_int(args[4])) ? (int)as_int(args[4]) : 0;
    ActivationType act = ACTIVATION_NONE;
    if (argc >= 6 && is_obj(args[5]) && ((Obj*)as_obj(args[5]))->type == OBJ_STRING) {
        const char* s = ((ObjString*)as_obj(args[5]))->chars;
        if (strcmp(s, "relu") == 0)         act = ACTIVATION_RELU;
        else if (strcmp(s, "sigmoid") == 0) act = ACTIVATION_SIGMOID;
        else if (strcmp(s, "tanh") == 0)    act = ACTIVATION_TANH;
        else if (strcmp(s, "softmax") == 0) act = ACTIVATION_SOFTMAX;
    }
    nn_model_add_conv2d(g_vm_ai_models[idx].model, filters, kernel, stride, padding, act);
    return val_bool(true);
}

static Value native_ai_nn_add_dropout(int argc, Value* args) {
    double rate = 0.0;
    int idx;
    if (argc != 2 || !is_int(args[0])) return val_bool(false);
    idx = vm_ai_model_find(as_int(args[0]));
    if (idx < 0) return val_bool(false);
    if (is_number(args[1])) rate = as_number(args[1]);
    else if (is_int(args[1])) rate = (double)as_int(args[1]);
    else return val_bool(false);
    nn_model_add_dropout(g_vm_ai_models[idx].model, rate);
    return val_bool(true);
}

static Value native_ai_nn_add_maxpool2d(int argc, Value* args) {
    if (argc < 2 || !is_int(args[0]) || !is_int(args[1])) return val_bool(false);
    int idx = vm_ai_model_find(as_int(args[0]));
    if (idx < 0) return val_bool(false);
    int pool = (int)as_int(args[1]);
    int stride = (argc >= 3 && is_int(args[2])) ? (int)as_int(args[2]) : pool;
    nn_model_add_maxpool2d(g_vm_ai_models[idx].model, pool, stride);
    return val_bool(true);
}

static Value native_ai_nn_forward(int argc, Value* args) {
    Tensor* x = NULL;
    bool own_x = false;
    if (argc != 2 || !is_int(args[0])) return val_int(0);
    int mi = vm_ai_model_find(as_int(args[0]));
    Tensor* out;
    if (mi < 0) return val_int(0);
    if (!vm_ai_tensor_resolve_arg(args[1], &x, &own_x)) return val_int(0);
    out = nn_model_forward(g_vm_ai_models[mi].model, x);
    if (own_x) tensor_free(x);
    return val_int(vm_ai_tensor_store(out));
}

static Value native_ai_nn_predict(int argc, Value* args) {
    Tensor* x = NULL;
    bool own_x = false;
    if (argc != 2 || !is_int(args[0])) return val_int(0);
    int mi = vm_ai_model_find(as_int(args[0]));
    Tensor* out;
    if (mi < 0) return val_int(0);
    if (!vm_ai_tensor_resolve_arg(args[1], &x, &own_x)) return val_int(0);
    out = nn_model_predict(g_vm_ai_models[mi].model, x);
    if (own_x) tensor_free(x);
    return val_int(vm_ai_tensor_store(out));
}

static Value native_ai_nn_train_step(int argc, Value* args) {
    Tensor *x = NULL, *y = NULL;
    bool own_x = false, own_y = false;
    double loss;
    if (argc < 3 || !is_int(args[0])) return val_number(0.0);
    int mi = vm_ai_model_find(as_int(args[0]));
    if (mi < 0) return val_number(0.0);
    if (!vm_ai_tensor_resolve_arg(args[1], &x, &own_x) || !vm_ai_tensor_resolve_arg(args[2], &y, &own_y)) {
        if (own_x && x) tensor_free(x);
        if (own_y && y) tensor_free(y);
        return val_number(0.0);
    }
    int batch = (argc >= 4 && is_int(args[3])) ? (int)as_int(args[3]) : 32;
    LossType loss_type = LOSS_MSE;
    if (argc >= 5 && is_obj(args[4]) && ((Obj*)as_obj(args[4]))->type == OBJ_STRING) {
        const char* loss_name = ((ObjString*)as_obj(args[4]))->chars;
        if (strcmp(loss_name, "cross_entropy") == 0 || strcmp(loss_name, "ce") == 0) {
            loss_type = LOSS_CROSS_ENTROPY;
        }
    }
    loss = nn_model_train_with_loss(g_vm_ai_models[mi].model, x, y, batch, loss_type);
    if (own_x && x) tensor_free(x);
    if (own_y && y) tensor_free(y);
    return val_number(loss);
}

static Value native_ai_nn_set_lr(int argc, Value* args) {
    double learning_rate = 0.0;
    int idx;
    if (argc != 2 || !is_int(args[0])) return val_bool(false);
    idx = vm_ai_model_find(as_int(args[0]));
    if (idx < 0) return val_bool(false);
    if (is_number(args[1])) learning_rate = as_number(args[1]);
    else if (is_int(args[1])) learning_rate = (double)as_int(args[1]);
    else return val_bool(false);
    nn_model_set_learning_rate(g_vm_ai_models[idx].model, learning_rate);
    return val_bool(true);
}

static Value native_ai_nn_get_lr(int argc, Value* args) {
    int idx;
    if (argc != 1 || !is_int(args[0])) return val_number(0.0);
    idx = vm_ai_model_find(as_int(args[0]));
    if (idx < 0) return val_number(0.0);
    return val_number(nn_model_get_learning_rate(g_vm_ai_models[idx].model));
}

static Value native_ai_nn_save(int argc, Value* args) {
    if (argc != 2 || !is_int(args[0]) || !is_obj(args[1]) || ((Obj*)as_obj(args[1]))->type != OBJ_STRING) return val_bool(false);
    int idx = vm_ai_model_find(as_int(args[0]));
    if (idx < 0) return val_bool(false);
    return val_bool(nn_model_save(g_vm_ai_models[idx].model, ((ObjString*)as_obj(args[1]))->chars));
}

static Value native_ai_nn_load(int argc, Value* args) {
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_STRING) return val_int(0);
    return val_int(vm_ai_model_store(nn_model_load(((ObjString*)as_obj(args[0]))->chars)));
}

static Value native_ai_nn_free(int argc, Value* args) {
    if (argc != 1 || !is_int(args[0])) return val_bool(false);
    int idx = vm_ai_model_find(as_int(args[0]));
    if (idx < 0) return val_bool(false);
    nn_model_free(g_vm_ai_models[idx].model);
    g_vm_ai_model_count--;
    if (idx != g_vm_ai_model_count) g_vm_ai_models[idx] = g_vm_ai_models[g_vm_ai_model_count];
    return val_bool(true);
}

static Value native_ai_reset(int argc, Value* args) {
    int i;
    UNUSED(args);
    if (argc != 0) return val_bool(false);

    for (i = 0; i < g_vm_ai_tensor_count; i++) {
        tensor_free(g_vm_ai_tensors[i].tensor);
        g_vm_ai_tensors[i].tensor = NULL;
    }
    g_vm_ai_tensor_count = 0;

    for (i = 0; i < g_vm_ai_opt_count; i++) {
        optimizer_free(g_vm_ai_optimizers[i].opt, g_vm_ai_optimizers[i].opt_type);
        g_vm_ai_optimizers[i].opt = NULL;
    }
    g_vm_ai_opt_count = 0;

    for (i = 0; i < g_vm_ai_model_count; i++) {
        nn_model_free(g_vm_ai_models[i].model);
        g_vm_ai_models[i].model = NULL;
    }
    g_vm_ai_model_count = 0;

    return val_bool(true);
}

static Value native_ai_nn_onnx_export(int argc, Value* args) {
    if (argc != 2 || !is_int(args[0]) || !is_obj(args[1]) || ((Obj*)as_obj(args[1]))->type != OBJ_STRING) return val_bool(false);
    int idx = vm_ai_model_find(as_int(args[0]));
    if (idx < 0) return val_bool(false);
    ObjString* path = (ObjString*)as_obj(args[1]);
    return val_bool(nn_model_export_onnx(g_vm_ai_models[idx].model, path->chars));
}

static Value native_ai_nn_export_vxnn(int argc, Value* args) {
    return native_ai_nn_onnx_export(argc, args);
}

static Value native_ai_nn_onnx_import(int argc, Value* args) {
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_STRING) return val_int(0);
    ObjString* path = (ObjString*)as_obj(args[0]);
    NNModel* m = nn_model_import_onnx(path->chars);
    if (!m) return val_int(0);
    return val_int(vm_ai_model_store(m));
}

static Value native_ai_nn_import_vxnn(int argc, Value* args) {
    return native_ai_nn_onnx_import(argc, args);
}

/* ── Transformer primitives VM ── */
static Value native_ai_attention(int argc, Value* args) {
    if (argc != 3 || !is_int(args[0]) || !is_int(args[1]) || !is_int(args[2])) return val_int(0);
    int qi = vm_ai_tensor_find(as_int(args[0]));
    int ki = vm_ai_tensor_find(as_int(args[1]));
    int vi = vm_ai_tensor_find(as_int(args[2]));
    if (qi < 0 || ki < 0 || vi < 0) return val_int(0);
    Tensor* qt = g_vm_ai_tensors[qi].tensor;
    Tensor* kt = g_vm_ai_tensors[ki].tensor;
    Tensor* vt = g_vm_ai_tensors[vi].tensor;
    Tensor* k_t = tensor_transpose(kt);
    if (!k_t) return val_int(0);
    Tensor* scores = tensor_matmul(qt, k_t); tensor_free(k_t);
    if (!scores) return val_int(0);
    int dk = qt->shape[qt->ndim - 1];
    tensor_imul_scalar(scores, 1.0 / sqrt((double)(dk > 0 ? dk : 1)));
    Tensor* probs = tensor_softmax(scores); tensor_free(scores);
    if (!probs) return val_int(0);
    Tensor* out = tensor_matmul(probs, vt); tensor_free(probs);
    return val_int(vm_ai_tensor_store(out));
}

static Value native_ai_layernorm(int argc, Value* args) {
    if (argc < 1 || !is_int(args[0])) return val_int(0);
    float eps = (argc >= 2) ? (float)(is_int(args[1]) ? (double)as_int(args[1]) : as_number(args[1])) : 1e-5f;
    int idx = vm_ai_tensor_find(as_int(args[0]));
    if (idx < 0) return val_int(0);
    Tensor* t = g_vm_ai_tensors[idx].tensor;
    Tensor* out = tensor_clone(t);
    if (!out) return val_int(0);
    int last_dim = t->shape[t->ndim - 1];
    int n_rows = (int)(t->size / last_dim);
    for (int r = 0; r < n_rows; r++) {
        float* row = out->data.f32_data + r * last_dim;
        float mean = 0.0f;
        for (int i = 0; i < last_dim; i++) mean += row[i];
        mean /= last_dim;
        float var = 0.0f;
        for (int i = 0; i < last_dim; i++) { float d = row[i] - mean; var += d * d; }
        var /= last_dim;
        float inv_std = 1.0f / sqrtf(var + eps);
        for (int i = 0; i < last_dim; i++) row[i] = (row[i] - mean) * inv_std;
    }
    return val_int(vm_ai_tensor_store(out));
}

static Value native_ai_embedding_lookup(int argc, Value* args) {
    if (argc != 2 || !is_int(args[0]) || !is_int(args[1])) return val_int(0);
    int ti = vm_ai_tensor_find(as_int(args[0]));
    if (ti < 0) return val_int(0);
    Tensor* tbl = g_vm_ai_tensors[ti].tensor;
    if (tbl->ndim < 2) return val_int(0);
    int64_t tok = as_int(args[1]);
    if (tok < 0 || tok >= tbl->shape[0]) return val_int(0);
    int embed_dim = tbl->shape[1];
    int shape1[1] = { embed_dim };
    Tensor* row = tensor_new(TENSOR_FLOAT32, 1, shape1);
    if (!row) return val_int(0);
    memcpy(row->data.f32_data, tbl->data.f32_data + tok * embed_dim, sizeof(float) * embed_dim);
    return val_int(vm_ai_tensor_store(row));
}

/* ── Data module VM ── */
static int vm_data_csv_field(const char* line, int pos, int len, char* buf, int bufsz) {
    int j = 0;
    if (pos < len && line[pos] == '"') {
        pos++;
        while (pos < len && j < bufsz - 1) {
            if (line[pos] == '"') {
                if (pos + 1 < len && line[pos + 1] == '"') { buf[j++] = '"'; pos += 2; }
                else { pos++; break; }
            } else { buf[j++] = line[pos++]; }
        }
    } else {
        while (pos < len && line[pos] != ',' && j < bufsz - 1) buf[j++] = line[pos++];
    }
    buf[j] = '\0';
    if (pos < len && line[pos] == ',') pos++;
    return pos;
}

static Value vm_data_csv_coerce(const char* s) {
    if (!s || !*s) return val_obj(obj_string_new(s ? s : "", 0));
    char* end;
    double d = strtod(s, &end);
    if (end != s && *end == '\0') {
        if (d == (double)(int64_t)d && !strchr(s, '.'))
            return val_int((int64_t)d);
        return val_number(d);
    }
    return val_obj(obj_string_new(s, (int)strlen(s)));
}

static Value native_data_csv_read(int argc, Value* args) {
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_STRING)
        return val_nothing_v();
    const char* path = ((ObjString*)as_obj(args[0]))->chars;
    FILE* fp = fopen(path, "r");
    if (!fp) return val_nothing_v();

    ObjArray* result = obj_array_new(16);
    char line[8192];
    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return val_obj(result); }
    int ll = (int)strlen(line);
    while (ll > 0 && (line[ll-1] == '\n' || line[ll-1] == '\r')) line[--ll] = '\0';

    char** hdr = NULL; int ncols = 0;
    { int pos = 0; char fld[512];
      while (pos <= ll) {
          pos = vm_data_csv_field(line, pos, ll, fld, sizeof(fld));
          hdr = (char**)realloc(hdr, sizeof(char*) * (ncols + 1));
          hdr[ncols] = (char*)malloc(strlen(fld) + 1);
          strcpy(hdr[ncols], fld);
          ncols++;
          if (pos >= ll) break;
      }
    }

    while (fgets(line, sizeof(line), fp)) {
        ll = (int)strlen(line);
        while (ll > 0 && (line[ll-1] == '\n' || line[ll-1] == '\r')) line[--ll] = '\0';
        if (ll == 0) continue;
        ObjMap* row = obj_map_new();
        int pos = 0, col = 0; char fld[8192];
        while (col < ncols) {
            pos = vm_data_csv_field(line, pos, ll, fld, sizeof(fld));
            ObjString* key = obj_string_new(hdr[col], (int)strlen(hdr[col]));
            obj_map_set(row, key, vm_data_csv_coerce(fld));
            col++;
            if (pos >= ll) break;
        }
        if (result->count >= result->capacity) {
            result->capacity *= 2;
            result->items = realloc(result->items, result->capacity * sizeof(Value));
        }
        result->items[result->count++] = val_obj(row);
    }
    for (int i = 0; i < ncols; i++) free(hdr[i]);
    free(hdr); fclose(fp);
    return val_obj(result);
}

static Value native_data_csv_write(int argc, Value* args) {
    if (argc != 2 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_STRING ||
        !is_obj(args[1]) || ((Obj*)as_obj(args[1]))->type != OBJ_ARRAY)
        return val_bool(false);
    const char* path = ((ObjString*)as_obj(args[0]))->chars;
    ObjArray* rows = (ObjArray*)as_obj(args[1]);
    if (rows->count == 0) return val_bool(true);
    FILE* fp = fopen(path, "w");
    if (!fp) return val_bool(false);
    if (!is_obj(rows->items[0]) || ((Obj*)as_obj(rows->items[0]))->type != OBJ_MAP) {
        fclose(fp); return val_bool(false);
    }
    ObjMap* first = (ObjMap*)as_obj(rows->items[0]);
    /* Collect keys from hash table (occupied entries only) */
    int ncols = 0;
    ObjString** col_keys = (ObjString**)malloc(sizeof(ObjString*) * (first->count + 1));
    for (int i = 0; i < first->capacity; i++) {
        if (!first->entries[i].occupied) continue;
        col_keys[ncols++] = first->entries[i].key;
    }
    for (int c = 0; c < ncols; c++) {
        if (c > 0) fputc(',', fp);
        fprintf(fp, "%s", col_keys[c]->chars);
    }
    fputc('\n', fp);
    for (int r = 0; r < (int)rows->count; r++) {
        if (!is_obj(rows->items[r]) || ((Obj*)as_obj(rows->items[r]))->type != OBJ_MAP) continue;
        ObjMap* row = (ObjMap*)as_obj(rows->items[r]);
        for (int c = 0; c < ncols; c++) {
            if (c > 0) fputc(',', fp);
            Value v = val_nothing_v();
            obj_map_get(row, col_keys[c], &v);
            if (is_obj(v) && ((Obj*)as_obj(v))->type == OBJ_STRING)
                fprintf(fp, "%s", ((ObjString*)as_obj(v))->chars);
            else if (is_int(v))    fprintf(fp, "%lld", (long long)as_int(v));
            else if (is_number(v)) fprintf(fp, "%g", as_number(v));
            else if (is_bool(v))   fprintf(fp, "%s", as_bool(v) ? "true" : "false");
        }
        fputc('\n', fp);
    }
    free(col_keys);
    fclose(fp);
    return val_bool(true);
}

static Value native_data_split(int argc, Value* args) {
    if (argc < 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY)
        return val_nothing_v();
    ObjArray* src = (ObjArray*)as_obj(args[0]);
    double ratio = 0.8;
    if (argc >= 2) {
        if (is_number(args[1])) ratio = as_number(args[1]);
        else if (is_int(args[1])) ratio = (double)as_int(args[1]);
    }
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    int train_n = (int)(src->count * ratio);
    int test_n  = (int)src->count - train_n;
    ObjArray* ta = obj_array_new(train_n > 0 ? train_n : 1);
    ObjArray* va = obj_array_new(test_n  > 0 ? test_n  : 1);
    for (int i = 0; i < train_n; i++) {
        if (ta->count >= ta->capacity) { ta->capacity *= 2; ta->items = realloc(ta->items, ta->capacity * sizeof(Value)); }
        ta->items[ta->count++] = src->items[i];
    }
    for (int i = train_n; i < (int)src->count; i++) {
        if (va->count >= va->capacity) { va->capacity *= 2; va->items = realloc(va->items, va->capacity * sizeof(Value)); }
        va->items[va->count++] = src->items[i];
    }
    ObjArray* res = obj_array_new(2);
    res->items[res->count++] = val_obj(ta);
    res->items[res->count++] = val_obj(va);
    return val_obj(res);
}

static Value native_data_shuffle(int argc, Value* args) {
    if (argc != 1 || !is_obj(args[0]) || ((Obj*)as_obj(args[0]))->type != OBJ_ARRAY)
        return val_nothing_v();
    ObjArray* src = (ObjArray*)as_obj(args[0]);
    if (src->count == 0) return args[0];
    ObjArray* out = obj_array_new((int)src->count);
    for (int i = 0; i < (int)src->count; i++) out->items[out->count++] = src->items[i];
    for (int i = (int)src->count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Value tmp = out->items[i]; out->items[i] = out->items[j]; out->items[j] = tmp;
    }
    return val_obj(out);
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
    vm_define_native(vm, "gpu_available", native_gpu_available, 0);
    vm_define_native(vm, "gpu_device_count", native_gpu_device_count, 0);
    vm_define_native(vm, "gpu_backend", native_gpu_backend, 0);
    vm_define_native(vm, "gpu_dot", native_gpu_dot, 2);
    vm_define_native(vm, "gpu_matmul", native_gpu_matmul, 2);
    vm_define_native(vm, "gpu_kernel_compile", native_gpu_kernel_compile, -1);
    vm_define_native(vm, "gpu_kernel_launch", native_gpu_kernel_launch, -1);
    vm_define_native(vm, "gpu_alloc", native_gpu_alloc, 1);
    vm_define_native(vm, "gpu_free", native_gpu_free, 1);
    vm_define_native(vm, "gpu_write", native_gpu_write, 2);
    vm_define_native(vm, "gpu_read", native_gpu_read, -1);
    vm_define_native(vm, "data_csv_read",  native_data_csv_read, 1);
    vm_define_native(vm, "data_csv_write", native_data_csv_write, 2);
    vm_define_native(vm, "data_split",     native_data_split, -1);
    vm_define_native(vm, "data_shuffle",   native_data_shuffle, 1);
    vm_define_native(vm, "ai_tensor_zeros",     native_ai_tensor_zeros, 1);
    vm_define_native(vm, "ai_tensor_ones",      native_ai_tensor_ones, 1);
    vm_define_native(vm, "ai_tensor_rand",      native_ai_tensor_rand, 1);
    vm_define_native(vm, "ai_tensor_randn",     native_ai_tensor_randn, -1);
    vm_define_native(vm, "ai_tensor_fill",      native_ai_tensor_fill, -1);
    vm_define_native(vm, "ai_tensor_from_array",native_ai_tensor_from_array, 1);
    vm_define_native(vm, "ai_tensor_to_array",  native_ai_tensor_to_array, 1);
    vm_define_native(vm, "ai_tensor_clone",     native_ai_tensor_clone, 1);
    vm_define_native(vm, "ai_tensor_matmul",    native_ai_tensor_matmul, 2);
    vm_define_native(vm, "ai_tensor_add",       native_ai_tensor_add, 2);
    vm_define_native(vm, "ai_tensor_sub",       native_ai_tensor_sub, 2);
    vm_define_native(vm, "ai_tensor_mul",       native_ai_tensor_mul, 2);
    vm_define_native(vm, "ai_tensor_div",       native_ai_tensor_div, 2);
    vm_define_native(vm, "ai_tensor_scale",     native_ai_tensor_scale, 2);
    vm_define_native(vm, "ai_tensor_relu",      native_ai_tensor_relu, 1);
    vm_define_native(vm, "ai_tensor_sigmoid",   native_ai_tensor_sigmoid, 1);
    vm_define_native(vm, "ai_tensor_tanh",      native_ai_tensor_tanh, 1);
    vm_define_native(vm, "ai_tensor_softmax",   native_ai_tensor_softmax, 1);
    vm_define_native(vm, "ai_tensor_reshape",   native_ai_tensor_reshape, 2);
    vm_define_native(vm, "ai_tensor_transpose", native_ai_tensor_transpose, 1);
    vm_define_native(vm, "ai_tensor_sum",       native_ai_tensor_sum, 1);
    vm_define_native(vm, "ai_tensor_mean",      native_ai_tensor_mean, 1);
    vm_define_native(vm, "ai_tensor_std",       native_ai_tensor_std, 1);
    vm_define_native(vm, "ai_tensor_max",       native_ai_tensor_max, 1);
    vm_define_native(vm, "ai_tensor_min",       native_ai_tensor_min, 1);
    vm_define_native(vm, "ai_tensor_shape",     native_ai_tensor_shape, 1);
    vm_define_native(vm, "ai_tensor_free",      native_ai_tensor_free, 1);
    vm_define_native(vm, "ai_optimizer_create", native_ai_optimizer_create, -1);
    vm_define_native(vm, "ai_optimizer_step",   native_ai_optimizer_step, 3);
    vm_define_native(vm, "ai_optimizer_free",   native_ai_optimizer_free, 1);
    vm_define_native(vm, "ai_nn_new",           native_ai_nn_new, -1);
    vm_define_native(vm, "ai_nn_add_dense",     native_ai_nn_add_dense, -1);
    vm_define_native(vm, "ai_nn_add_conv2d",    native_ai_nn_add_conv2d, -1);
    vm_define_native(vm, "ai_nn_add_dropout",   native_ai_nn_add_dropout, 2);
    vm_define_native(vm, "ai_nn_add_maxpool2d", native_ai_nn_add_maxpool2d, -1);
    vm_define_native(vm, "ai_nn_forward",       native_ai_nn_forward, 2);
    vm_define_native(vm, "ai_nn_predict",       native_ai_nn_predict, 2);
    vm_define_native(vm, "ai_nn_train_step",    native_ai_nn_train_step, -1);
    vm_define_native(vm, "ai_nn_set_lr",        native_ai_nn_set_lr, 2);
    vm_define_native(vm, "ai_nn_get_lr",        native_ai_nn_get_lr, 1);
    vm_define_native(vm, "ai_nn_save",          native_ai_nn_save, 2);
    vm_define_native(vm, "ai_nn_load",          native_ai_nn_load, 1);
    vm_define_native(vm, "ai_nn_free",          native_ai_nn_free, 1);
    vm_define_native(vm, "ai_reset",            native_ai_reset, 0);
    vm_define_native(vm, "ai_nn_onnx_export",   native_ai_nn_onnx_export, 2);
    vm_define_native(vm, "ai_nn_onnx_import",   native_ai_nn_onnx_import, 1);
    vm_define_native(vm, "ai_nn_export_vxnn",   native_ai_nn_export_vxnn, 2);
    vm_define_native(vm, "ai_nn_import_vxnn",   native_ai_nn_import_vxnn, 1);
    vm_define_native(vm, "ai_tokenize",         native_ai_tokenize, 1);
    vm_define_native(vm, "ai_attention",        native_ai_attention, 3);
    vm_define_native(vm, "ai_layernorm",        native_ai_layernorm, -1);
    vm_define_native(vm, "ai_embedding_lookup", native_ai_embedding_lookup, 2);

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
    if (strcmp(module_name, "gpu") == 0) {
        vm_alias_global(vm, "available", "gpu_available");
        vm_alias_global(vm, "device_count", "gpu_device_count");
        vm_alias_global(vm, "backend", "gpu_backend");
        vm_alias_global(vm, "dot", "gpu_dot");
        vm_alias_global(vm, "matmul", "gpu_matmul");
        vm_alias_global(vm, "kernel_compile", "gpu_kernel_compile");
        vm_alias_global(vm, "kernel_launch", "gpu_kernel_launch");
        vm_alias_global(vm, "alloc", "gpu_alloc");
        vm_alias_global(vm, "free", "gpu_free");
        vm_alias_global(vm, "write", "gpu_write");
        vm_alias_global(vm, "read", "gpu_read");
        return true;
    }
    if (strcmp(module_name, "data") == 0) {
        vm_alias_global(vm, "csv_read",  "data_csv_read");
        vm_alias_global(vm, "csv_write", "data_csv_write");
        vm_alias_global(vm, "split",     "data_split");
        vm_alias_global(vm, "shuffle",   "data_shuffle");
        return true;
    }
    if (strcmp(module_name, "ai") == 0) {
        vm_alias_global(vm, "tensor_zeros",     "ai_tensor_zeros");
        vm_alias_global(vm, "tensor_ones",      "ai_tensor_ones");
        vm_alias_global(vm, "tensor_rand",      "ai_tensor_rand");
        vm_alias_global(vm, "tensor_randn",     "ai_tensor_randn");
        vm_alias_global(vm, "tensor_fill",      "ai_tensor_fill");
        vm_alias_global(vm, "tensor_from_array","ai_tensor_from_array");
        vm_alias_global(vm, "tensor_to_array",  "ai_tensor_to_array");
        vm_alias_global(vm, "tensor_clone",     "ai_tensor_clone");
        vm_alias_global(vm, "tensor_matmul",    "ai_tensor_matmul");
        vm_alias_global(vm, "tensor_add",       "ai_tensor_add");
        vm_alias_global(vm, "tensor_sub",       "ai_tensor_sub");
        vm_alias_global(vm, "tensor_mul",       "ai_tensor_mul");
        vm_alias_global(vm, "tensor_div",       "ai_tensor_div");
        vm_alias_global(vm, "tensor_scale",     "ai_tensor_scale");
        vm_alias_global(vm, "tensor_relu",      "ai_tensor_relu");
        vm_alias_global(vm, "tensor_sigmoid",   "ai_tensor_sigmoid");
        vm_alias_global(vm, "tensor_tanh",      "ai_tensor_tanh");
        vm_alias_global(vm, "tensor_softmax",   "ai_tensor_softmax");
        vm_alias_global(vm, "tensor_reshape",   "ai_tensor_reshape");
        vm_alias_global(vm, "tensor_transpose", "ai_tensor_transpose");
        vm_alias_global(vm, "tensor_sum",       "ai_tensor_sum");
        vm_alias_global(vm, "tensor_mean",      "ai_tensor_mean");
        vm_alias_global(vm, "tensor_std",       "ai_tensor_std");
        vm_alias_global(vm, "tensor_max",       "ai_tensor_max");
        vm_alias_global(vm, "tensor_min",       "ai_tensor_min");
        vm_alias_global(vm, "tensor_shape",     "ai_tensor_shape");
        vm_alias_global(vm, "tensor_free",      "ai_tensor_free");
        vm_alias_global(vm, "optimizer_create", "ai_optimizer_create");
        vm_alias_global(vm, "optimizer_step",   "ai_optimizer_step");
        vm_alias_global(vm, "optimizer_free",   "ai_optimizer_free");
        vm_alias_global(vm, "nn_new",           "ai_nn_new");
        vm_alias_global(vm, "nn_add_dense",     "ai_nn_add_dense");
        vm_alias_global(vm, "nn_add_conv2d",    "ai_nn_add_conv2d");
        vm_alias_global(vm, "nn_add_dropout",   "ai_nn_add_dropout");
        vm_alias_global(vm, "nn_add_maxpool2d", "ai_nn_add_maxpool2d");
        vm_alias_global(vm, "nn_forward",       "ai_nn_forward");
        vm_alias_global(vm, "nn_predict",       "ai_nn_predict");
        vm_alias_global(vm, "nn_train_step",    "ai_nn_train_step");
        vm_alias_global(vm, "nn_set_lr",        "ai_nn_set_lr");
        vm_alias_global(vm, "nn_get_lr",        "ai_nn_get_lr");
        vm_alias_global(vm, "nn_save",          "ai_nn_save");
        vm_alias_global(vm, "nn_load",          "ai_nn_load");
        vm_alias_global(vm, "nn_free",          "ai_nn_free");
        vm_alias_global(vm, "reset",            "ai_reset");
        vm_alias_global(vm, "nn_onnx_export",   "ai_nn_onnx_export");
        vm_alias_global(vm, "nn_onnx_import",   "ai_nn_onnx_import");
        vm_alias_global(vm, "nn_export_vxnn",   "ai_nn_export_vxnn");
        vm_alias_global(vm, "nn_import_vxnn",   "ai_nn_import_vxnn");
        vm_alias_global(vm, "tokenize",         "ai_tokenize");
        vm_alias_global(vm, "attention",        "ai_attention");
        vm_alias_global(vm, "layernorm",        "ai_layernorm");
        vm_alias_global(vm, "embedding_lookup", "ai_embedding_lookup");
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

    if (strcmp(module_name, "gpu") == 0) {
        if (strcmp(symbol_name, "available") == 0) return vm_alias_global(vm, "available", "gpu_available");
        if (strcmp(symbol_name, "device_count") == 0) return vm_alias_global(vm, "device_count", "gpu_device_count");
        if (strcmp(symbol_name, "backend") == 0) return vm_alias_global(vm, "backend", "gpu_backend");
        if (strcmp(symbol_name, "dot") == 0) return vm_alias_global(vm, "dot", "gpu_dot");
        if (strcmp(symbol_name, "matmul") == 0) return vm_alias_global(vm, "matmul", "gpu_matmul");
        if (strcmp(symbol_name, "kernel_compile") == 0) return vm_alias_global(vm, "kernel_compile", "gpu_kernel_compile");
        if (strcmp(symbol_name, "kernel_launch") == 0) return vm_alias_global(vm, "kernel_launch", "gpu_kernel_launch");
        if (strcmp(symbol_name, "alloc") == 0) return vm_alias_global(vm, "alloc", "gpu_alloc");
        if (strcmp(symbol_name, "free") == 0) return vm_alias_global(vm, "free", "gpu_free");
        if (strcmp(symbol_name, "write") == 0) return vm_alias_global(vm, "write", "gpu_write");
        if (strcmp(symbol_name, "read") == 0) return vm_alias_global(vm, "read", "gpu_read");
        return false;
    }

    if (strcmp(module_name, "data") == 0) {
        if (strcmp(symbol_name, "csv_read") == 0)  return vm_alias_global(vm, "csv_read",  "data_csv_read");
        if (strcmp(symbol_name, "csv_write") == 0) return vm_alias_global(vm, "csv_write", "data_csv_write");
        if (strcmp(symbol_name, "split") == 0)     return vm_alias_global(vm, "split",     "data_split");
        if (strcmp(symbol_name, "shuffle") == 0)   return vm_alias_global(vm, "shuffle",   "data_shuffle");
        return false;
    }
    if (strcmp(module_name, "ai") == 0) {
        if (strcmp(symbol_name, "tensor_zeros") == 0)     return vm_alias_global(vm, "tensor_zeros",     "ai_tensor_zeros");
        if (strcmp(symbol_name, "tensor_ones") == 0)      return vm_alias_global(vm, "tensor_ones",      "ai_tensor_ones");
        if (strcmp(symbol_name, "tensor_rand") == 0)      return vm_alias_global(vm, "tensor_rand",      "ai_tensor_rand");
        if (strcmp(symbol_name, "tensor_randn") == 0)     return vm_alias_global(vm, "tensor_randn",     "ai_tensor_randn");
        if (strcmp(symbol_name, "tensor_fill") == 0)      return vm_alias_global(vm, "tensor_fill",      "ai_tensor_fill");
        if (strcmp(symbol_name, "tensor_from_array") == 0)return vm_alias_global(vm, "tensor_from_array","ai_tensor_from_array");
        if (strcmp(symbol_name, "tensor_to_array") == 0)  return vm_alias_global(vm, "tensor_to_array",  "ai_tensor_to_array");
        if (strcmp(symbol_name, "tensor_clone") == 0)     return vm_alias_global(vm, "tensor_clone",     "ai_tensor_clone");
        if (strcmp(symbol_name, "tensor_matmul") == 0)    return vm_alias_global(vm, "tensor_matmul",    "ai_tensor_matmul");
        if (strcmp(symbol_name, "tensor_add") == 0)       return vm_alias_global(vm, "tensor_add",       "ai_tensor_add");
        if (strcmp(symbol_name, "tensor_sub") == 0)       return vm_alias_global(vm, "tensor_sub",       "ai_tensor_sub");
        if (strcmp(symbol_name, "tensor_mul") == 0)       return vm_alias_global(vm, "tensor_mul",       "ai_tensor_mul");
        if (strcmp(symbol_name, "tensor_div") == 0)       return vm_alias_global(vm, "tensor_div",       "ai_tensor_div");
        if (strcmp(symbol_name, "tensor_scale") == 0)     return vm_alias_global(vm, "tensor_scale",     "ai_tensor_scale");
        if (strcmp(symbol_name, "tensor_relu") == 0)      return vm_alias_global(vm, "tensor_relu",      "ai_tensor_relu");
        if (strcmp(symbol_name, "tensor_sigmoid") == 0)   return vm_alias_global(vm, "tensor_sigmoid",   "ai_tensor_sigmoid");
        if (strcmp(symbol_name, "tensor_tanh") == 0)      return vm_alias_global(vm, "tensor_tanh",      "ai_tensor_tanh");
        if (strcmp(symbol_name, "tensor_softmax") == 0)   return vm_alias_global(vm, "tensor_softmax",   "ai_tensor_softmax");
        if (strcmp(symbol_name, "tensor_reshape") == 0)   return vm_alias_global(vm, "tensor_reshape",   "ai_tensor_reshape");
        if (strcmp(symbol_name, "tensor_transpose") == 0) return vm_alias_global(vm, "tensor_transpose", "ai_tensor_transpose");
        if (strcmp(symbol_name, "tensor_sum") == 0)       return vm_alias_global(vm, "tensor_sum",       "ai_tensor_sum");
        if (strcmp(symbol_name, "tensor_mean") == 0)      return vm_alias_global(vm, "tensor_mean",      "ai_tensor_mean");
        if (strcmp(symbol_name, "tensor_std") == 0)       return vm_alias_global(vm, "tensor_std",       "ai_tensor_std");
        if (strcmp(symbol_name, "tensor_max") == 0)       return vm_alias_global(vm, "tensor_max",       "ai_tensor_max");
        if (strcmp(symbol_name, "tensor_min") == 0)       return vm_alias_global(vm, "tensor_min",       "ai_tensor_min");
        if (strcmp(symbol_name, "tensor_shape") == 0)     return vm_alias_global(vm, "tensor_shape",     "ai_tensor_shape");
        if (strcmp(symbol_name, "tensor_free") == 0)      return vm_alias_global(vm, "tensor_free",      "ai_tensor_free");
        if (strcmp(symbol_name, "optimizer_create") == 0) return vm_alias_global(vm, "optimizer_create", "ai_optimizer_create");
        if (strcmp(symbol_name, "optimizer_step") == 0)   return vm_alias_global(vm, "optimizer_step",   "ai_optimizer_step");
        if (strcmp(symbol_name, "optimizer_free") == 0)   return vm_alias_global(vm, "optimizer_free",   "ai_optimizer_free");
        if (strcmp(symbol_name, "nn_new") == 0)           return vm_alias_global(vm, "nn_new",           "ai_nn_new");
        if (strcmp(symbol_name, "nn_add_dense") == 0)     return vm_alias_global(vm, "nn_add_dense",     "ai_nn_add_dense");
        if (strcmp(symbol_name, "nn_add_conv2d") == 0)    return vm_alias_global(vm, "nn_add_conv2d",    "ai_nn_add_conv2d");
        if (strcmp(symbol_name, "nn_add_dropout") == 0)   return vm_alias_global(vm, "nn_add_dropout",   "ai_nn_add_dropout");
        if (strcmp(symbol_name, "nn_add_maxpool2d") == 0) return vm_alias_global(vm, "nn_add_maxpool2d", "ai_nn_add_maxpool2d");
        if (strcmp(symbol_name, "nn_forward") == 0)       return vm_alias_global(vm, "nn_forward",       "ai_nn_forward");
        if (strcmp(symbol_name, "nn_predict") == 0)       return vm_alias_global(vm, "nn_predict",       "ai_nn_predict");
        if (strcmp(symbol_name, "nn_train_step") == 0)    return vm_alias_global(vm, "nn_train_step",    "ai_nn_train_step");
        if (strcmp(symbol_name, "nn_set_lr") == 0)        return vm_alias_global(vm, "nn_set_lr",        "ai_nn_set_lr");
        if (strcmp(symbol_name, "nn_get_lr") == 0)        return vm_alias_global(vm, "nn_get_lr",        "ai_nn_get_lr");
        if (strcmp(symbol_name, "nn_save") == 0)          return vm_alias_global(vm, "nn_save",          "ai_nn_save");
        if (strcmp(symbol_name, "nn_load") == 0)          return vm_alias_global(vm, "nn_load",          "ai_nn_load");
        if (strcmp(symbol_name, "nn_free") == 0)          return vm_alias_global(vm, "nn_free",          "ai_nn_free");
        if (strcmp(symbol_name, "reset") == 0)            return vm_alias_global(vm, "reset",            "ai_reset");
        if (strcmp(symbol_name, "nn_onnx_export") == 0)   return vm_alias_global(vm, "nn_onnx_export",   "ai_nn_onnx_export");
        if (strcmp(symbol_name, "nn_onnx_import") == 0)   return vm_alias_global(vm, "nn_onnx_import",   "ai_nn_onnx_import");
        if (strcmp(symbol_name, "nn_export_vxnn") == 0)   return vm_alias_global(vm, "nn_export_vxnn",   "ai_nn_export_vxnn");
        if (strcmp(symbol_name, "nn_import_vxnn") == 0)   return vm_alias_global(vm, "nn_import_vxnn",   "ai_nn_import_vxnn");
        if (strcmp(symbol_name, "tokenize") == 0)         return vm_alias_global(vm, "tokenize",         "ai_tokenize");
        if (strcmp(symbol_name, "attention") == 0)        return vm_alias_global(vm, "attention",        "ai_attention");
        if (strcmp(symbol_name, "layernorm") == 0)        return vm_alias_global(vm, "layernorm",        "ai_layernorm");
        if (strcmp(symbol_name, "embedding_lookup") == 0) return vm_alias_global(vm, "embedding_lookup", "ai_embedding_lookup");
        return false;
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

/* Macro to ensure running_vm is reset on error returns */
#define VM_RETURN_ERROR(result) do { \
    running_vm = NULL; \
    return (result); \
} while (0)

#define VM_RETURN_OK() do { \
    running_vm = NULL; \
    return VM_OK; \
} while (0)

VMResult vm_run_closure(VM* vm, ObjClosure* closure, Value* args, int argc) {
    running_vm = vm;
    vm->has_last_result = false;
    vm->last_result = val_nothing_v();
    register_builtins(vm);

    if (!closure || !closure->function) {
        VM_RETURN_ERROR(VM_RUNTIME_ERROR);
    }
    if (closure->function->arity != argc) {
        VM_RETURN_ERROR(VM_RUNTIME_ERROR);
    }

    /* Set up the initial call frame */
    push(vm, val_obj(closure));
    for (int i = 0; i < argc; i++) {
        push(vm, args[i]);
    }
    CallFrame* frame = &vm->frames[vm->frame_count++];
    frame->closure = closure;
    frame->ip = closure->function->code;
    frame->slots = vm->stack_top - argc - 1;

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
                if (oa->type == OBJ_ARRAY && ob->type == OBJ_ARRAY) {
                    ObjArray* aa = (ObjArray*)oa;
                    ObjArray* ab = (ObjArray*)ob;
                    int total = aa->count + ab->count;
                    ObjArray* result = obj_array_new(total);
                    for (int i = 0; i < aa->count; i++) result->items[result->count++] = aa->items[i];
                    for (int i = 0; i < ab->count; i++) result->items[result->count++] = ab->items[i];
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
            if (is_obj(a) && ((Obj*)as_obj(a))->type == OBJ_ARRAY && (is_int(b) || is_number(b))) {
                ObjArray* src = (ObjArray*)as_obj(a);
                int n = is_int(b) ? (int)as_int(b) : (int)as_number(b);
                if (n <= 0) {
                    push(vm, val_obj(obj_array_new(0)));
                } else {
                    int total = src->count * n;
                    ObjArray* out = obj_array_new(total);
                    for (int i = 0; i < n; i++) {
                        for (int j = 0; j < src->count; j++) {
                            out->items[out->count++] = src->items[j];
                        }
                    }
                    push(vm, val_obj(out));
                }
            } else if (is_obj(a) && ((Obj*)as_obj(a))->type == OBJ_STRING &&
                (is_int(b) || is_number(b))) {
                ObjString* sa = (ObjString*)as_obj(a);
                int n = is_int(b) ? (int)as_int(b) : (int)as_number(b);
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
            } else if ((is_int(a) || is_number(a)) && is_obj(b) && ((Obj*)as_obj(b))->type == OBJ_STRING) {
                ObjString* sb = (ObjString*)as_obj(b);
                int n = is_int(a) ? (int)as_int(a) : (int)as_number(a);
                if (n <= 0) {
                    push(vm, val_obj(obj_string_new("", 0)));
                } else {
                    int len = sb->length * n;
                    char* buf = malloc(len + 1);
                    for (int i = 0; i < n; i++)
                        memcpy(buf + i * sb->length, sb->chars, sb->length);
                    buf[len] = '\0';
                    push(vm, val_obj(obj_string_new(buf, len)));
                    free(buf);
                }
            } else if ((is_int(a) || is_number(a)) && is_obj(b) && ((Obj*)as_obj(b))->type == OBJ_ARRAY) {
                ObjArray* src = (ObjArray*)as_obj(b);
                int n = is_int(a) ? (int)as_int(a) : (int)as_number(a);
                if (n <= 0) {
                    push(vm, val_obj(obj_array_new(0)));
                } else {
                    int total = src->count * n;
                    ObjArray* out = obj_array_new(total);
                    for (int i = 0; i < n; i++) {
                        for (int j = 0; j < src->count; j++) {
                            out->items[out->count++] = src->items[j];
                        }
                    }
                    push(vm, val_obj(out));
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
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
                    VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
            }

            /* Handle variadic functions (arity < 0): pack extra args into array */
            int effective_argc = (int)argc;
            if (callee_fn->arity < 0) {
                int required = (-callee_fn->arity) - 1;
                if (effective_argc < required) {
                    runtime_error(vm, frame,
                        "Not enough arguments: expected at least %d, got %d",
                        required, effective_argc);
                    VM_RETURN_ERROR(VM_RUNTIME_ERROR);
                }
                int extra = effective_argc - required;
                ObjArray* rest_arr = obj_array_new(extra < 4 ? 4 : extra);
                Value* extra_start = vm->stack_top - extra;
                for (int i = 0; i < extra; i++) {
                    rest_arr->items[i] = extra_start[i];
                }
                rest_arr->count = extra;
                vm->stack_top -= extra;
                push(vm, val_obj(rest_arr));
                effective_argc = required + 1;
            }

            CallFrame* new_frame = &vm->frames[vm->frame_count++];
            new_frame->closure = closure;
            new_frame->ip = callee_fn->code;
            new_frame->slots = vm->stack_top - effective_argc - 1;

            frame = new_frame;
            break;
        }

        case OP_CLOSURE: {
            Value fn_val = READ_CONSTANT();
            if (!is_obj(fn_val) || ((Obj*)as_obj(fn_val))->type != OBJ_FUNCTION) {
                runtime_error(vm, frame, "Closure constant is not a function.");
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
                VM_RETURN_OK();
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
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
            }
            Obj* target_obj = (Obj*)as_obj(target);
            if (target_obj->type == OBJ_ARRAY) {
                ObjArray* arr = (ObjArray*)target_obj;
                int idx = is_int(index) ? as_int(index) : (int)to_double(index);
                if (idx < 0) idx += arr->count;
                if (idx < 0 || idx >= arr->count) {
                    runtime_error(vm, frame, "Array index %d out of bounds (size %d).", idx, arr->count);
                    VM_RETURN_ERROR(VM_RUNTIME_ERROR);
                }
                push(vm, arr->items[idx]);
            } else if (target_obj->type == OBJ_STRING) {
                ObjString* str = (ObjString*)target_obj;
                int idx = is_int(index) ? as_int(index) : (int)to_double(index);
                if (idx < 0) idx += str->length;
                if (idx < 0 || idx >= str->length) {
                    runtime_error(vm, frame, "String index out of bounds.");
                    VM_RETURN_ERROR(VM_RUNTIME_ERROR);
                }
                push(vm, val_obj(obj_string_new(&str->chars[idx], 1)));
            } else {
                runtime_error(vm, frame, "Cannot index this type.");
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
            }
            break;
        }

        case OP_INDEX_SET: {
            Value val = pop(vm);
            Value index = pop(vm);
            Value target = pop(vm);
            if (!is_obj(target)) {
                runtime_error(vm, frame, "Can only index-assign arrays.");
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
            }
            ObjArray* arr = (ObjArray*)as_obj(target);
            int idx = is_int(index) ? as_int(index) : (int)to_double(index);
            if (idx < 0) idx += arr->count;
            if (idx < 0 || idx >= arr->count) {
                runtime_error(vm, frame, "Array index out of bounds.");
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
                    VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
            Value* spawn_args = NULL;
            Value callee = val_nothing_v();

            if (argc > 0) {
                spawn_args = (Value*)malloc(sizeof(Value) * argc);
            }
            if (argc > 0) {
                /* Pop call arguments first. */
                for (int i = argc - 1; i >= 0; i--) {
                    spawn_args[i] = pop(vm);
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
                ObjClosure* closure = NULL;
                if (((Obj*)as_obj(callee))->type == OBJ_CLOSURE) {
                    closure = (ObjClosure*)as_obj(callee);
                } else {
                    closure = obj_closure_new((ObjFunction*)as_obj(callee));
                }

                /* Fast path for native callables: execute now and return completed handle. */
                if (closure->function->code == NULL && closure->function->chunk_count == -1) {
                    NativeFn native_fn;
                    memcpy(&native_fn, &closure->function->constants[0], sizeof(NativeFn));
                    Value native_result = native_fn((int)argc, spawn_args ? spawn_args : vm->stack_top);
                    if (spawn_args) free(spawn_args);
                    ObjTaskHandle* done = obj_task_handle_completed(native_result);
                    push(vm, val_obj(done));
                    break;
                }

                Task* task = task_create(closure, false, spawn_args, (int)argc);
                if (task) {
                    if (spawn_args) free(spawn_args);
                    task_spawn(task);
                    ObjTaskHandle* handle = obj_task_handle_new(task);
                    push(vm, val_obj(handle));
                    break;
                }
            }

            if (spawn_args) free(spawn_args);

            push(vm, val_nothing_v());
            break;
        }

        case OP_USE_MODULE: {
            uint16_t mod_idx = READ_SHORT();
            Value mod_val = frame->closure->function->constants[mod_idx];
            if (!is_obj(mod_val) || ((Obj*)as_obj(mod_val))->type != OBJ_STRING) {
                runtime_error(vm, frame, "Invalid module name in use statement");
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
            }
            ObjString* module = (ObjString*)as_obj(mod_val);
            if (!vm_import_module(vm, module->chars)) {
                runtime_error(vm, frame, "Unknown module '%s'", module->chars);
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
            }
            ObjString* module = (ObjString*)as_obj(mod_val);
            ObjString* symbol = (ObjString*)as_obj(sym_val);
            if (!vm_import_symbol(vm, module->chars, symbol->chars)) {
                runtime_error(vm, frame, "Cannot import '%s' from module '%s'",
                              symbol->chars, module->chars);
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
            }
            break;
        }

        case OP_CHANNEL_CREATE: {
            /* Create a channel with optional buffer size */
            /* Syntax: channel() or channel(buffer_size) */
            Channel* chan = channel_create();
            if (!chan) {
                runtime_error(vm, frame, "Failed to create channel");
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
            }
            
            if (!channel_send((Channel*)as_obj(chan_val), value)) {
                runtime_error(vm, frame, "Failed to send on channel");
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
            /* True tail call for Vex functions: reuse current frame. */
            uint8_t argc = READ_BYTE();
            Value callee = peek(vm, argc);

            if (!is_obj(callee)) {
                runtime_error(vm, frame, "Can only call functions.");
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
            }

            /* Handle variadic functions (arity < 0): pack extra args into array */
            int eff_argc = (int)argc;
            if (callee_fn->arity < 0) {
                int required = (-callee_fn->arity) - 1;
                if (eff_argc < required) {
                    runtime_error(vm, frame,
                        "Not enough arguments: expected at least %d, got %d",
                        required, eff_argc);
                    VM_RETURN_ERROR(VM_RUNTIME_ERROR);
                }
                int extra = eff_argc - required;
                ObjArray* rest_arr = obj_array_new(extra < 4 ? 4 : extra);
                Value* extra_start = vm->stack_top - extra;
                for (int i = 0; i < extra; i++) {
                    rest_arr->items[i] = extra_start[i];
                }
                rest_arr->count = extra;
                vm->stack_top -= extra;
                push(vm, val_obj(rest_arr));
                eff_argc = required + 1;
            } else if (callee_fn->arity != eff_argc) {
                runtime_error(vm, frame, "Arity mismatch: expected %d but got %d",
                            callee_fn->arity, eff_argc);
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
            }

            if (callee_fn->code == NULL && callee_fn->chunk_count == -1) {
                NativeFn native_fn;
                memcpy(&native_fn, &callee_fn->constants[0], sizeof(NativeFn));
                Value result = native_fn(eff_argc, vm->stack_top - eff_argc);
                vm->stack_top -= eff_argc + 1;
                push(vm, result);
                break;
            }

            /* Close captures from the old frame before we overwrite its slots. */
            close_upvalues(vm, frame->slots);

            /* Move callee + args into the current frame's slot window. */
            Value* src = vm->stack_top - eff_argc - 1;
            Value* dst = frame->slots;
            for (int i = 0; i <= eff_argc; i++) {
                dst[i] = src[i];
            }
            vm->stack_top = dst + eff_argc + 1;

            /* Rebind current frame to callee function body. */
            frame->closure = closure;
            frame->ip = callee_fn->code;
            frame->slots = dst;
            break;
        }

        case OP_GET_FIELD: {
            uint16_t name_idx = READ_SHORT();
            ObjString* fname = (ObjString*)as_obj(frame->closure->function->constants[name_idx]);
            Value obj_val = pop(vm);
            if (!is_obj(obj_val)) {
                runtime_error(vm, frame, "Cannot access field '%s' on non-object", fname->chars);
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
            }
            Obj* obj = (Obj*)as_obj(obj_val);
            if (obj->type == OBJ_INSTANCE) {
                ObjInstance* inst = (ObjInstance*)obj;
                Value field_val;
                if (obj_map_get(inst->fields, fname, &field_val)) {
                    push(vm, field_val);
                } else {
                    runtime_error(vm, frame, "Instance has no field '%s'", fname->chars);
                    VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
                VM_RETURN_ERROR(VM_RUNTIME_ERROR);
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
            VM_RETURN_OK();

        default:
            runtime_error(vm, frame, "Unknown opcode: %d", instruction);
            VM_RETURN_ERROR(VM_RUNTIME_ERROR);
        }
    }
}

VMResult vm_run(VM* vm, ObjFunction* fn) {
    ObjClosure* closure = obj_closure_new(fn);
    return vm_run_closure(vm, closure, NULL, 0);
}
