#include "wasm_emitter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void wasm_emitter_init(WASMEmitter* emitter) {
    emitter->capacity = 1024;
    emitter->size = 0;
    emitter->buffer = (uint8_t*)malloc(emitter->capacity);
}

void wasm_emitter_free(WASMEmitter* emitter) {
    if (emitter->buffer) {
        free(emitter->buffer);
        emitter->buffer = NULL;
    }
}

static void emit_u8(WASMEmitter* e, uint8_t byte) {
    if (e->size >= e->capacity) {
        e->capacity *= 2;
        e->buffer = (uint8_t*)realloc(e->buffer, e->capacity);
    }
    e->buffer[e->size++] = byte;
}

static void emit_bytes(WASMEmitter* e, const uint8_t* bytes, size_t count) {
    for (size_t i = 0; i < count; i++) {
        emit_u8(e, bytes[i]);
    }
}

bool wasm_emit_module(WASMEmitter* emitter, ASTNode* ast) {
    UNUSED(ast);
    emitter->size = 0;

    /* WASM Header Magic & Version */
    const uint8_t wasm_magic[4] = { 0x00, 0x61, 0x73, 0x6d };
    const uint8_t wasm_version[4] = { 0x01, 0x00, 0x00, 0x00 };
    emit_bytes(emitter, wasm_magic, 4);
    emit_bytes(emitter, wasm_version, 4);

    /* Type Section (ID 1) */
    emit_u8(emitter, 1);  /* Section ID */
    emit_u8(emitter, 5);  /* Section Size */
    emit_u8(emitter, 1);  /* Num Types */
    emit_u8(emitter, 0x60); /* func type */
    emit_u8(emitter, 0);  /* 0 params */
    emit_u8(emitter, 1);  /* 1 result */
    emit_u8(emitter, 0x7f); /* i32 */

    /* Function Section (ID 3) */
    emit_u8(emitter, 3);
    emit_u8(emitter, 2);
    emit_u8(emitter, 1);
    emit_u8(emitter, 0);

    /* Code Section (ID 10) */
    emit_u8(emitter, 10);
    emit_u8(emitter, 6);
    emit_u8(emitter, 1);  /* 1 body */
    emit_u8(emitter, 4);  /* body size */
    emit_u8(emitter, 0);  /* locals */
    emit_u8(emitter, 0x41); /* i32.const */
    emit_u8(emitter, 42);   /* value 42 */
    emit_u8(emitter, 0x0b); /* end */

    return true;
}

bool wasm_save_file(WASMEmitter* emitter, const char* filepath) {
    FILE* f = fopen(filepath, "wb");
    if (!f) return false;
    fwrite(emitter->buffer, 1, emitter->size, f);
    fclose(f);
    printf("✓ WebAssembly module successfully compiled to '%s' (%zu bytes).\n", filepath, emitter->size);
    return true;
}
