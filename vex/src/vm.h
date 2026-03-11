#ifndef VEXIUM_VM_H
#define VEXIUM_VM_H

#include "common.h"
#include "opcodes.h"

/* ══════════════════════════════════════════════════════════════
 *  VM — Stack-Based Virtual Machine
 * ══════════════════════════════════════════════════════════════ */

#define STACK_MAX 4096
#define FRAMES_MAX 256
#define GLOBALS_MAX 512

/* Call frame (one per active function) */
typedef struct {
    struct ObjClosure* closure;
    uint8_t* ip;           /* instruction pointer within function's code */
    Value* slots;          /* pointer into VM stack (base of this frame) */
} CallFrame;

/* Global variable entry */
typedef struct {
    ObjString* name;
    Value value;
    bool occupied;
} GlobalEntry;

/* The VM */
typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frame_count;

    Value stack[STACK_MAX];
    Value* stack_top;

    GlobalEntry globals[GLOBALS_MAX];

    struct ObjUpvalue* open_upvalues;
    size_t bytes_allocated;
    size_t next_gc;

    /* Captured return value for top-level execution (used by task await). */
    bool has_last_result;
    Value last_result;

    Obj* objects;          /* head of all allocated objects (for GC) */
} VM;

/* Result codes */
typedef enum {
    VM_OK,
    VM_COMPILE_ERROR,
    VM_RUNTIME_ERROR
} VMResult;

/* ── Public API ── */
void vm_init(VM* vm);
VM* vm_new(void);
void vm_free(VM* vm);
VMResult vm_run(VM* vm, ObjFunction* fn);

/* Register a native/built-in function */
typedef Value (*NativeFn)(int argCount, Value* args);
void vm_define_native(VM* vm, const char* name, NativeFn fn, int arity);

ObjClosure* obj_closure_new(ObjFunction* function);

#endif /* VEXIUM_VM_H */
