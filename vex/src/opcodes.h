#ifndef VEXIUM_OPCODES_H
#define VEXIUM_OPCODES_H

#include "common.h"

/* ══════════════════════════════════════════════════════════════
 *  VM OPCODES
 * ══════════════════════════════════════════════════════════════ */

typedef enum {
    /* ── Constants & Literals ── */
    OP_CONST,           /* Push constant from pool: OP_CONST [idx_lo] [idx_hi] */
    OP_TRUE,            /* Push true */
    OP_FALSE,           /* Push false */
    OP_NOTHING,         /* Push nothing */

    /* ── Arithmetic ── */
    OP_ADD,             /* a + b */
    OP_SUB,             /* a - b */
    OP_MUL,             /* a * b */
    OP_DIV,             /* a / b */
    OP_MOD,             /* a % b */
    OP_POW,             /* a ** b */
    OP_NEG,             /* -a */

    /* ── Comparison ── */
    OP_EQ,              /* a == b */
    OP_NEQ,             /* a != b */
    OP_LT,              /* a < b */
    OP_GT,              /* a > b */
    OP_LTE,             /* a <= b */
    OP_GTE,             /* a >= b */

    /* ── Logic ── */
    OP_NOT,             /* !a */

    /* ── Variables ── */
    OP_DEFINE_GLOBAL,   /* Define global: OP_DEFINE_GLOBAL [name_idx_lo] [name_idx_hi] */
    OP_GET_GLOBAL,      /* Get global:    OP_GET_GLOBAL [name_idx_lo] [name_idx_hi] */
    OP_SET_GLOBAL,      /* Set global:    OP_SET_GLOBAL [name_idx_lo] [name_idx_hi] */
    OP_GET_LOCAL,       /* Get local:     OP_GET_LOCAL [slot] */
    OP_SET_LOCAL,       /* Set local:     OP_SET_LOCAL [slot] */
    OP_GET_UPVALUE,     /* Get upvalue:   OP_GET_UPVALUE [slot] */
    OP_SET_UPVALUE,     /* Set upvalue:   OP_SET_UPVALUE [slot] */

    /* ── Stack manipulation ── */
    OP_POP,             /* Discard TOS */
    OP_DUP,             /* Duplicate TOS */

    /* ── Control flow ── */
    OP_JMP,             /* Unconditional jump: OP_JMP [offset_lo] [offset_hi] */
    OP_JMP_IF_FALSE,    /* Jump if TOS is falsy: OP_JMP_IF_FALSE [offset_lo] [offset_hi] */
    OP_LOOP,            /* Jump backward: OP_LOOP [offset_lo] [offset_hi] */

    /* ── Functions ── */
    OP_CALL,            /* Call function: OP_CALL [argc] */
    OP_CLOSURE,         /* Create closure from function constant + upvalue descriptors */
    OP_CLOSE_UPVALUE,   /* Close captured local before popping */
    OP_RETURN,          /* Return from function */

    /* ── Strings ── */
    OP_CONCAT,          /* String concatenation */
    OP_TO_STRING,       /* Convert TOS to string (for interpolation) */

    /* ── I/O ── */
    OP_PRINT,           /* display TOS */

    /* ── Arrays ── */
    OP_ARRAY,           /* Build array: OP_ARRAY [count_lo] [count_hi] */
    OP_ARRAY_PUSH,      /* Append TOS to array at TOS-1 */
    OP_INDEX_GET,       /* array[index] */
    OP_INDEX_SET,       /* array[index] = val */

    /* ── Maps ── */
    OP_MAP,             /* Build map: OP_MAP [count_lo] [count_hi] */

    /* ── Concurrency & Control ── */
    OP_DEFER,           /* Schedule deferred execution: OP_DEFER [frame_offset] */
    OP_AWAIT,           /* Wait for async result (currently blocking stub) */
    OP_TAIL_CALL,       /* Tail call optimization: OP_TAIL_CALL [argc] */
    OP_SPAWN,           /* Spawn a new task: OP_SPAWN */
    OP_CHANNEL_CREATE,  /* Create a communication channel: OP_CHANNEL_CREATE */
    OP_CHANNEL_SEND,    /* Send value on channel: OP_CHANNEL_SEND */
    OP_CHANNEL_RECV,    /* Receive value from channel: OP_CHANNEL_RECV */
    OP_USE_MODULE,      /* Import module into current scope: OP_USE_MODULE [name_idx_lo] [name_idx_hi] */
    OP_USE_SYMBOL,      /* Import symbol from module: OP_USE_SYMBOL [module_idx_lo] [module_idx_hi] [symbol_idx_lo] [symbol_idx_hi] */

    /* ── Fields & Structs ── */
    OP_GET_FIELD,       /* Get field from instance: OP_GET_FIELD [name_idx_lo] [name_idx_hi] */
    OP_SET_FIELD,       /* Set field on instance: OP_SET_FIELD [name_idx_lo] [name_idx_hi] */
    OP_STRUCT,          /* Create struct instance: OP_STRUCT [field_count_lo] [field_count_hi] */

    /* ── Halt ── */
    OP_HALT             /* Stop execution */
} OpCode;

/* ══════════════════════════════════════════════════════════════
 *  VALUE REPRESENTATION (NaN-Boxing)
 *
 *  IEEE 754 double:
 *    Quiet NaN pattern: 0_11111111111_1xxxx...xxxx (51 payload bits)
 *    We steal the payload bits to store ints, bools, pointers.
 *
 *  Encoding:
 *    double     → raw IEEE 754 double
 *    int        → QNAN | SIGN_BIT | TAG_INT | (value & 0xFFFFFFFF)
 *    bool       → QNAN | TAG_BOOL | (0 or 1)
 *    nothing    → QNAN | TAG_NOTHING
 *    object ptr → QNAN | TAG_OBJ | pointer (48-bit)
 * ══════════════════════════════════════════════════════════════ */

#include <string.h>
#include <math.h>

/* A NaN-boxed value */
typedef uint64_t Value;

/* Bit patterns */
#define QNAN        ((uint64_t)0x7FFC000000000000ULL)
#define SIGN_BIT    ((uint64_t)0x8000000000000000ULL)

/* Tags (stored in bits 48-50 of the NaN payload) */
#define TAG_NOTHING 0x0001000000000000ULL   /* nothing */
#define TAG_BOOL    0x0002000000000000ULL   /* boolean */
#define TAG_INT     0x0004000000000000ULL   /* integer (32-bit in payload) */
#define TAG_OBJ     0x0008000000000000ULL   /* heap object pointer */

/* ── Constructors ── */

static inline Value val_number(double d) {
    Value v;
    memcpy(&v, &d, sizeof(double));
    return v;
}

static inline Value val_int(int32_t i) {
    return QNAN | SIGN_BIT | TAG_INT | ((uint64_t)(uint32_t)i);
}

static inline Value val_bool(bool b) {
    return QNAN | TAG_BOOL | (b ? 1ULL : 0ULL);
}

static inline Value val_nothing_v(void) {
    return QNAN | TAG_NOTHING;
}

static inline Value val_obj(void* ptr) {
    return QNAN | TAG_OBJ | (uint64_t)(uintptr_t)ptr;
}

/* ── Type checks ── */

static inline bool is_number(Value v) {
    return (v & QNAN) != QNAN;
}

static inline bool is_int(Value v) {
    return (v & (QNAN | SIGN_BIT | TAG_INT)) == (QNAN | SIGN_BIT | TAG_INT);
}

static inline bool is_bool(Value v) {
    return (v & (QNAN | TAG_BOOL)) == (QNAN | TAG_BOOL) && !is_int(v);
}

static inline bool is_nothing(Value v) {
    return v == (QNAN | TAG_NOTHING);
}

static inline bool is_obj(Value v) {
    return (v & (QNAN | TAG_OBJ)) == (QNAN | TAG_OBJ) && !is_int(v);
}

/* ── Extractors ── */

static inline double as_number(Value v) {
    double d;
    memcpy(&d, &v, sizeof(double));
    return d;
}

static inline int32_t as_int(Value v) {
    return (int32_t)(v & 0xFFFFFFFFULL);
}

static inline bool as_bool(Value v) {
    return (v & 1ULL) != 0;
}

static inline void* as_obj(Value v) {
    return (void*)(uintptr_t)(v & 0x0000FFFFFFFFFFFFULL);
}

/* ══════════════════════════════════════════════════════════════
 *  HEAP OBJECTS (referenced by NaN-boxed pointers)
 * ══════════════════════════════════════════════════════════════ */

typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_ARRAY,
    OBJ_MAP,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_STRUCT_DEF,
    OBJ_INSTANCE,
    OBJ_ERROR,
    OBJ_TENSOR,
    OBJ_CHANNEL,
    OBJ_TASK_HANDLE
} ObjType;

/* Common object header */
typedef struct Obj {
    ObjType type;
    bool is_marked;
    struct Obj* next;  /* GC intrusive linked list */
} Obj;

/* String object */
typedef struct {
    Obj obj;
    int length;
    uint32_t hash;
    char chars[];      /* flexible array member */
} ObjString;

/* Function object */
typedef struct {
    Obj obj;
    int arity;
    int upvalue_count;
    char* name;        /* function name (for debugging) */
    int chunk_count;   /* bytecode count */
    int chunk_capacity;
    uint8_t* code;     /* bytecodes */
    int* lines;        /* line info per bytecode */
    int const_count;
    int const_capacity;
    Value* constants;  /* constant pool */
} ObjFunction;

/* Array object */
typedef struct {
    Obj obj;
    int count;
    int capacity;
    Value* items;
} ObjArray;

/* Map entry for VM */
typedef struct {
    ObjString* key;
    Value value;
    bool occupied;
} VMMapEntry;

/* Map object */
typedef struct {
    Obj obj;
    int count;
    int capacity;
    VMMapEntry* entries;
} ObjMap;

/* Upvalue object */
typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

/* Closure object */
typedef struct ObjClosure {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalue_count;
} ObjClosure;

/* Struct Definition object */
typedef struct {
    Obj obj;
    ObjString* name;
    int field_count;
    ObjString** field_names;
    ObjMap* methods;
} ObjStructDef;

/* Instance object */
typedef struct {
    Obj obj;
    ObjStructDef* struct_def;
    ObjMap* fields;
} ObjInstance;

/* Error object */
typedef struct {
    Obj obj;
    ObjString* message;
    ObjString* error_type;
    ObjString* file;
    int line;
} ObjError;

/* Forward declaration to avoid including task.h in this header. */
typedef struct Task Task;

/* Spawn/await handle object. Owns a Task* until awaited or VM teardown. */
typedef struct {
    Obj obj;
    Task* task;
    bool completed;
    Value result;
} ObjTaskHandle;

/* ══════════════════════════════════════════════════════════════
 *  CHUNK (bytecode container)
 * ══════════════════════════════════════════════════════════════ */

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    int* lines;          /* source line for each byte */
    int const_count;
    int const_capacity;
    Value* constants;
} Chunk;

/* Chunk API */
void chunk_init(Chunk* chunk);
void chunk_write(Chunk* chunk, uint8_t byte, int line);
int chunk_add_constant(Chunk* chunk, Value value);
void chunk_free(Chunk* chunk);

/* Object helpers */
ObjString* obj_string_new(const char* chars, int length);
ObjFunction* obj_function_new(const char* name);
ObjArray* obj_array_new(int capacity);
ObjMap* obj_map_new(void);
void obj_map_set(ObjMap* map, ObjString* key, Value value);
bool obj_map_get(ObjMap* map, ObjString* key, Value* out);
ObjStructDef* obj_struct_def_new(ObjString* name, int field_count);
ObjInstance* obj_instance_new(ObjStructDef* def);
uint32_t hash_string(const char* key, int length);

/* Value printing */
void value_print(Value val);
bool value_is_truthy(Value val);
bool values_equal(Value a, Value b);

/* Opcode name for debugging */
const char* opcode_name(OpCode op);

#endif /* VEXIUM_OPCODES_H */
