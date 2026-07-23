#include "chunk.h"

void chunk_init(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    chunk->constants_count = 0;
    chunk->constants_capacity = 0;
    chunk->constants = NULL;
}

void chunk_free(Chunk* chunk) {
    free(chunk->code);
    free(chunk->lines);
    free(chunk->constants);
    chunk_init(chunk);
}

void chunk_write(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int old_capacity = chunk->capacity;
        chunk->capacity = old_capacity < 8 ? 8 : old_capacity * 2;
        chunk->code = (uint8_t*)realloc(chunk->code, sizeof(uint8_t) * chunk->capacity);
        chunk->lines = (int*)realloc(chunk->lines, sizeof(int) * chunk->capacity);
    }
    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int chunk_add_constant(Chunk* chunk, Value64 value) {
    if (chunk->constants_capacity < chunk->constants_count + 1) {
        int old_capacity = chunk->constants_capacity;
        chunk->constants_capacity = old_capacity < 8 ? 8 : old_capacity * 2;
        chunk->constants = (Value64*)realloc(chunk->constants, sizeof(Value64) * chunk->constants_capacity);
    }
    chunk->constants[chunk->constants_count] = value;
    return chunk->constants_count++;
}
