#ifndef VEXIUM_WASM_EMITTER_H
#define VEXIUM_WASM_EMITTER_H

#include "common.h"
#include "ast.h"
#include "chunk.h"

typedef struct {
    uint8_t* buffer;
    size_t size;
    size_t capacity;
} WASMEmitter;

void wasm_emitter_init(WASMEmitter* emitter);
void wasm_emitter_free(WASMEmitter* emitter);

bool wasm_emit_module(WASMEmitter* emitter, ASTNode* ast);
bool wasm_save_file(WASMEmitter* emitter, const char* filepath);

#endif
