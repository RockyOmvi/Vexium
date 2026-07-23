#ifndef VEXIUM_JIT_H
#define VEXIUM_JIT_H

#include "common.h"
#include "chunk.h"
#include "value.h"

#define JIT_MAX_CODE_SIZE 65536
#define JIT_MAX_LABEL_SLOTS 256

typedef Value64 (*JITFunction)(Value64* registers, Value64* constants);

typedef enum {
    X86_REG_RAX = 0,
    X86_REG_RCX = 1,
    X86_REG_RDX = 2,
    X86_REG_RBX = 3,
    X86_REG_RSP = 4,
    X86_REG_RBP = 5,
    X86_REG_RSI = 6,
    X86_REG_RDI = 7,
    X86_REG_R8  = 8,
    X86_REG_R9  = 9,
    X86_REG_R10 = 10,
    X86_REG_R11 = 11,
    X86_REG_R12 = 12,
    X86_REG_R13 = 13,
    X86_REG_R14 = 14,
    X86_REG_R15 = 15
} X86Register;

typedef struct {
    int opcode_offset;
    size_t code_patch_offset;
} JITLabelPatch;

typedef struct {
    uint8_t* code_buffer;
    size_t size;
    size_t capacity;
    bool is_executable;
    JITLabelPatch patches[JIT_MAX_LABEL_SLOTS];
    int patch_count;
} JITCompiler;

void jit_init(JITCompiler* jit);
void jit_free(JITCompiler* jit);

void jit_emit_u8(JITCompiler* jit, uint8_t byte);
void jit_emit_u32(JITCompiler* jit, uint32_t val);
void jit_emit_u64(JITCompiler* jit, uint64_t val);

void jit_emit_mov_reg_imm64(JITCompiler* jit, X86Register reg, uint64_t imm);
void jit_emit_mov_reg_reg(JITCompiler* jit, X86Register dst, X86Register src);
void jit_emit_add_reg_reg(JITCompiler* jit, X86Register dst, X86Register src);
void jit_emit_sub_reg_reg(JITCompiler* jit, X86Register dst, X86Register src);
void jit_emit_imul_reg_reg(JITCompiler* jit, X86Register dst, X86Register src);
void jit_emit_push_reg(JITCompiler* jit, X86Register reg);
void jit_emit_pop_reg(JITCompiler* jit, X86Register reg);
void jit_emit_ret(JITCompiler* jit);

JITFunction jit_compile_chunk(JITCompiler* jit, Chunk* chunk);
void jit_make_executable(JITCompiler* jit);

#endif
