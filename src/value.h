#ifndef VEXIUM_VALUE_H
#define VEXIUM_VALUE_H

#include "common.h"

/* NaN-boxing representation for 64-bit Values:
 * Standard double floats use any 64-bit pattern that is not QNaN.
 * QNaN is 0x7ff8000000000000.
 * Mask 0x7ffc000000000000:
 *   TAG_NOTHING : 0x7ffc000000000001
 *   TAG_FALSE   : 0x7ffc000000000002
 *   TAG_TRUE    : 0x7ffc000000000003
 *   TAG_INT     : 0x7ffd000000000000 | (uint32_t val)
 *   TAG_OBJ     : 0x7ffe000000000000 | (uint64_t pointer)
 */

#define QNAN             ((uint64_t)0x7ff8000000000000ULL)
#define SIGN_BIT         ((uint64_t)0x8000000000000000ULL)
#define TAG_NIL          ((uint64_t)0x7ffc000000000001ULL)
#define TAG_FALSE        ((uint64_t)0x7ffc000000000002ULL)
#define TAG_TRUE         ((uint64_t)0x7ffc000000000003ULL)
#define TAG_INT_MASK     ((uint64_t)0x7ffd000000000000ULL)
#define TAG_OBJ_MASK     ((uint64_t)0x7ffe000000000000ULL)

typedef uint64_t Value64;

typedef enum {
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_MAP,
    OBJ_TENSOR,
    OBJ_TASK,
    OBJ_INSTANCE
} ObjType;

typedef struct Obj Obj;

struct Obj {
    ObjType type;
    bool is_marked;
    Obj* next;
};

typedef struct {
    Obj obj;
    char* chars;
    int length;
} ObjString;

typedef struct {
    Obj obj;
    Value64* items;
    int count;
    int capacity;
} ObjArray;

typedef struct {
    char* key;
    Value64 value;
} MapEntry64;

typedef struct {
    Obj obj;
    MapEntry64* entries;
    int count;
    int capacity;
} ObjMap;

typedef struct {
    Obj obj;
    float* data;
    float* grad;
    int* shape;
    int ndim;
    int size;
} ObjTensor;

typedef struct {
    Obj obj;
    void* thread_handle;
    Value64 result;
    bool completed;
} ObjTask;

static inline bool is_double(Value64 v) { return (v & QNAN) != QNAN; }
static inline bool is_nothing(Value64 v) { return v == TAG_NIL; }
static inline bool is_bool(Value64 v) { return v == TAG_FALSE || v == TAG_TRUE; }
static inline bool is_int(Value64 v) { return (v & ((uint64_t)0xffff000000000000ULL)) == TAG_INT_MASK; }
static inline bool is_obj(Value64 v) { return (v & ((uint64_t)0xffff000000000000ULL)) == TAG_OBJ_MASK; }

static inline double value_to_double(Value64 v) {
    union { uint64_t u; double d; } cast;
    cast.u = v;
    return cast.d;
}

static inline Value64 double_to_value(double d) {
    union { double d; uint64_t u; } cast;
    cast.d = d;
    return cast.u;
}

static inline int64_t value_to_int(Value64 v) {
    return (int64_t)(int32_t)(v & 0xffffffffULL);
}

static inline Value64 int_to_value(int64_t i) {
    return TAG_INT_MASK | ((uint64_t)(uint32_t)(i));
}

static inline bool value_to_bool(Value64 v) { return v == TAG_TRUE; }
static inline Value64 bool_to_value(bool b) { return b ? TAG_TRUE : TAG_FALSE; }
static inline Value64 nothing_to_value(void) { return TAG_NIL; }

static inline Obj* value_to_obj(Value64 v) {
    return (Obj*)(uintptr_t)(v & 0xffffffffffffULL);
}

static inline Value64 obj_to_value(Obj* obj) {
    return TAG_OBJ_MASK | ((uint64_t)(uintptr_t)obj);
}

ObjString* allocate_string(const char* chars, int length);
ObjArray* allocate_array(void);
ObjMap* allocate_map(void);
ObjTensor* allocate_tensor(int* shape, int ndim);
ObjTask* allocate_task(void);

void free_obj(Obj* obj);
void print_value64(Value64 v);

#endif
