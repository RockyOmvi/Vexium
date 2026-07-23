#ifndef VEXIUM_VM_H
#define VEXIUM_VM_H

#include "chunk.h"
#include "value.h"

#define STACK_MAX 1024
#define GLOBALS_MAX 256

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct {
    char* name;
    Value64 value;
} VMGlobal;

typedef struct {
    Chunk* chunk;
    uint8_t* ip;
    Value64 stack[STACK_MAX];
    Value64* stack_top;
    Obj* objects;
    VMGlobal globals[GLOBALS_MAX];
    int globals_count;
} VM;

void vm_init(VM* vm);
void vm_free(VM* vm);
InterpretResult vm_run(VM* vm, Chunk* chunk);
void vm_push(VM* vm, Value64 value);
Value64 vm_pop(VM* vm);

#endif
