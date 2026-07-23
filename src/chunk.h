#ifndef VEXIUM_CHUNK_H
#define VEXIUM_CHUNK_H

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_TASK_SPAWN,
    OP_TENSOR_DOT,
    OP_CLASS,
    OP_METHOD,
    OP_INVOKE,
    OP_CLOSURE,
    OP_IMPORT,
    OP_RETURN
} OpCode;

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    int* lines;

    int constants_count;
    int constants_capacity;
    Value64* constants;
} Chunk;

void chunk_init(Chunk* chunk);
void chunk_free(Chunk* chunk);
void chunk_write(Chunk* chunk, uint8_t byte, int line);
int chunk_add_constant(Chunk* chunk, Value64 value);

#endif
