#include "value.h"

ObjString* allocate_string(const char* chars, int length) {
    ObjString* string = (ObjString*)malloc(sizeof(ObjString));
    string->obj.type = OBJ_STRING;
    string->obj.is_marked = false;
    string->obj.next = NULL;
    string->chars = vex_strdup(chars, length);
    string->length = length;
    return string;
}

ObjArray* allocate_array(void) {
    ObjArray* array = (ObjArray*)malloc(sizeof(ObjArray));
    array->obj.type = OBJ_ARRAY;
    array->obj.is_marked = false;
    array->obj.next = NULL;
    array->items = NULL;
    array->count = 0;
    array->capacity = 0;
    return array;
}

ObjMap* allocate_map(void) {
    ObjMap* map = (ObjMap*)malloc(sizeof(ObjMap));
    map->obj.type = OBJ_MAP;
    map->obj.is_marked = false;
    map->obj.next = NULL;
    map->entries = NULL;
    map->count = 0;
    map->capacity = 0;
    return map;
}

ObjTensor* allocate_tensor(int* shape, int ndim) {
    ObjTensor* tensor = (ObjTensor*)malloc(sizeof(ObjTensor));
    tensor->obj.type = OBJ_TENSOR;
    tensor->obj.is_marked = false;
    tensor->obj.next = NULL;
    tensor->ndim = ndim;
    tensor->shape = (int*)malloc(sizeof(int) * ndim);
    int total_size = 1;
    for (int i = 0; i < ndim; i++) {
        tensor->shape[i] = shape[i];
        total_size *= shape[i];
    }
    tensor->size = total_size;
    tensor->data = (float*)calloc(total_size, sizeof(float));
    return tensor;
}

ObjTask* allocate_task(void) {
    ObjTask* task = (ObjTask*)malloc(sizeof(ObjTask));
    task->obj.type = OBJ_TASK;
    task->obj.is_marked = false;
    task->obj.next = NULL;
    task->thread_handle = NULL;
    task->result = TAG_NIL;
    task->completed = false;
    return task;
}

void free_obj(Obj* obj) {
    if (!obj) return;
    switch (obj->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*)obj;
            free(string->chars);
            free(string);
            break;
        }
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)obj;
            free(array->items);
            free(array);
            break;
        }
        case OBJ_MAP: {
            ObjMap* map = (ObjMap*)obj;
            for (int i = 0; i < map->count; i++) {
                free(map->entries[i].key);
            }
            free(map->entries);
            free(map);
            break;
        }
        case OBJ_TENSOR: {
            ObjTensor* tensor = (ObjTensor*)obj;
            free(tensor->shape);
            free(tensor->data);
            free(tensor);
            break;
        }
        case OBJ_TASK: {
            ObjTask* task = (ObjTask*)obj;
            free(task);
            break;
        }
        default:
            free(obj);
            break;
    }
}

void print_value64(Value64 v) {
    if (is_double(v)) {
        printf("%g", value_to_double(v));
    } else if (is_int(v)) {
        printf("%lld", (long long)value_to_int(v));
    } else if (is_bool(v)) {
        printf("%s", value_to_bool(v) ? "true" : "false");
    } else if (is_nothing(v)) {
        printf("nothing");
    } else if (is_obj(v)) {
        Obj* obj = value_to_obj(v);
        switch (obj->type) {
            case OBJ_STRING:
                printf("%s", ((ObjString*)obj)->chars);
                break;
            case OBJ_ARRAY:
                printf("[Array count=%d]", ((ObjArray*)obj)->count);
                break;
            case OBJ_MAP:
                printf("{Map count=%d}", ((ObjMap*)obj)->count);
                break;
            case OBJ_TENSOR:
                printf("<tensor size=%d>", ((ObjTensor*)obj)->size);
                break;
            case OBJ_TASK:
                printf("<task status=%s>", ((ObjTask*)obj)->completed ? "done" : "running");
                break;
            default:
                printf("<object>");
                break;
        }
    } else {
        printf("<unknown>");
    }
}
