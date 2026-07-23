#ifndef VEXIUM_WASM_EMITTER_H
#define VEXIUM_WASM_EMITTER_H

#include "common.h"
#include "ast.h"
#include "chunk.h"

typedef enum {
    WASM_TYPE_I32 = 0x7F,
    WASM_TYPE_I64 = 0x7E,
    WASM_TYPE_F32 = 0x7D,
    WASM_TYPE_F64 = 0x7C,
    WASM_TYPE_FUNC = 0x60,
    WASM_TYPE_VOID = 0x40
} WASMType;

typedef struct {
    uint8_t* buffer;
    size_t size;
    size_t capacity;
    int function_count;
    int type_count;
} WASMEmitter;

void wasm_emitter_init(WASMEmitter* emitter);
void wasm_emitter_free(WASMEmitter* emitter);

void wasm_emit_u8(WASMEmitter* e, uint8_t byte);
void wasm_emit_u32_leb128(WASMEmitter* e, uint32_t val);
void wasm_emit_i32_leb128(WASMEmitter* e, int32_t val);
void wasm_emit_bytes(WASMEmitter* e, const uint8_t* bytes, size_t count);

bool wasm_emit_module(WASMEmitter* emitter, ASTNode* ast);
bool wasm_save_file(WASMEmitter* emitter, const char* filepath);

#endif
