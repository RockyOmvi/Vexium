#ifndef VEXIUM_JIT_H
#define VEXIUM_JIT_H

#include "common.h"
#include "chunk.h"
#include "value.h"

typedef Value64 (*JITFunction)(Value64* registers);

typedef struct {
    uint8_t* code_buffer;
    size_t size;
    size_t capacity;
    bool is_executable;
} JITCompiler;

void jit_init(JITCompiler* jit);
void jit_free(JITCompiler* jit);

JITFunction jit_compile_chunk(JITCompiler* jit, Chunk* chunk);
void jit_make_executable(JITCompiler* jit);

#endif
