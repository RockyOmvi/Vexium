#include "jit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

void jit_init(JITCompiler* jit) {
    jit->capacity = JIT_MAX_CODE_SIZE;
    jit->size = 0;
    jit->is_executable = false;
    jit->patch_count = 0;

#ifdef _WIN32
    jit->code_buffer = (uint8_t*)VirtualAlloc(NULL, jit->capacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    jit->code_buffer = (uint8_t*)mmap(NULL, jit->capacity, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
#endif
}

void jit_free(JITCompiler* jit) {
    if (jit->code_buffer) {
#ifdef _WIN32
        VirtualFree(jit->code_buffer, 0, MEM_RELEASE);
#else
        munmap(jit->code_buffer, jit->capacity);
#endif
        jit->code_buffer = NULL;
    }
}

void jit_emit_u8(JITCompiler* jit, uint8_t byte) {
    if (jit->size >= jit->capacity) {
        size_t new_cap = jit->capacity * 2;
#ifdef _WIN32
        uint8_t* new_buf = (uint8_t*)VirtualAlloc(NULL, new_cap, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (new_buf) {
            memcpy(new_buf, jit->code_buffer, jit->size);
            VirtualFree(jit->code_buffer, 0, MEM_RELEASE);
            jit->code_buffer = new_buf;
            jit->capacity = new_cap;
        }
#else
        uint8_t* new_buf = (uint8_t*)mremap(jit->code_buffer, jit->capacity, new_cap, MREMAP_MAYMOVE);
        if (new_buf != MAP_FAILED) {
            jit->code_buffer = new_buf;
            jit->capacity = new_cap;
        }
#endif
    }
    jit->code_buffer[jit->size++] = byte;
}

void jit_emit_u32(JITCompiler* jit, uint32_t val) {
    jit_emit_u8(jit, (uint8_t)(val & 0xFF));
    jit_emit_u8(jit, (uint8_t)((val >> 8) & 0xFF));
    jit_emit_u8(jit, (uint8_t)((val >> 16) & 0xFF));
    jit_emit_u8(jit, (uint8_t)((val >> 24) & 0xFF));
}

void jit_emit_u64(JITCompiler* jit, uint64_t val) {
    jit_emit_u32(jit, (uint32_t)(val & 0xFFFFFFFF));
    jit_emit_u32(jit, (uint32_t)(val >> 32));
}

static void jit_emit_rex(JITCompiler* jit, bool w, X86Register reg, X86Register rm) {
    uint8_t rex = 0x40;
    if (w) rex |= 0x08;
    if (reg >= 8) rex |= 0x04;
    if (rm >= 8) rex |= 0x01;
    if (rex != 0x40 || w) {
        jit_emit_u8(jit, rex);
    }
}

static void jit_emit_modrm(JITCompiler* jit, uint8_t mod, uint8_t reg, uint8_t rm) {
    uint8_t modrm = ((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7);
    jit_emit_u8(jit, modrm);
}

static void jit_emit_sib(JITCompiler* jit, uint8_t scale, uint8_t index, uint8_t base) {
    uint8_t sib = ((scale & 3) << 6) | ((index & 7) << 3) | (base & 7);
    jit_emit_u8(jit, sib);
}

void jit_emit_push_reg(JITCompiler* jit, X86Register reg) {
    if (reg >= 8) jit_emit_u8(jit, 0x41);
    jit_emit_u8(jit, 0x50 + (reg & 7));
}

void jit_emit_pop_reg(JITCompiler* jit, X86Register reg) {
    if (reg >= 8) jit_emit_u8(jit, 0x41);
    jit_emit_u8(jit, 0x58 + (reg & 7));
}

void jit_emit_mov_reg_imm64(JITCompiler* jit, X86Register reg, uint64_t imm) {
    jit_emit_rex(jit, true, 0, reg);
    jit_emit_u8(jit, 0xB8 + (reg & 7));
    jit_emit_u64(jit, imm);
}

void jit_emit_mov_reg_reg(JITCompiler* jit, X86Register dst, X86Register src) {
    jit_emit_rex(jit, true, src, dst);
    jit_emit_u8(jit, 0x89);
    jit_emit_modrm(jit, 3, src, dst);
}

void jit_emit_mov_reg_mem(JITCompiler* jit, X86Register dst, X86Register base_reg, int32_t offset) {
    jit_emit_rex(jit, true, dst, base_reg);
    jit_emit_u8(jit, 0x8B);
    if (offset == 0 && (base_reg & 7) != 5) {
        jit_emit_modrm(jit, 0, dst, base_reg);
    } else if (offset >= -128 && offset <= 127) {
        jit_emit_modrm(jit, 1, dst, base_reg);
        jit_emit_u8(jit, (uint8_t)offset);
    } else {
        jit_emit_modrm(jit, 2, dst, base_reg);
        jit_emit_u32(jit, (uint32_t)offset);
    }
}

void jit_emit_mov_mem_reg(JITCompiler* jit, X86Register base_reg, int32_t offset, X86Register src) {
    jit_emit_rex(jit, true, src, base_reg);
    jit_emit_u8(jit, 0x89);
    if (offset == 0 && (base_reg & 7) != 5) {
        jit_emit_modrm(jit, 0, src, base_reg);
    } else if (offset >= -128 && offset <= 127) {
        jit_emit_modrm(jit, 1, src, base_reg);
        jit_emit_u8(jit, (uint8_t)offset);
    } else {
        jit_emit_modrm(jit, 2, src, base_reg);
        jit_emit_u32(jit, (uint32_t)offset);
    }
}

void jit_emit_add_reg_reg(JITCompiler* jit, X86Register dst, X86Register src) {
    jit_emit_rex(jit, true, src, dst);
    jit_emit_u8(jit, 0x01);
    jit_emit_modrm(jit, 3, src, dst);
}

void jit_emit_sub_reg_reg(JITCompiler* jit, X86Register dst, X86Register src) {
    jit_emit_rex(jit, true, src, dst);
    jit_emit_u8(jit, 0x29);
    jit_emit_modrm(jit, 3, src, dst);
}

void jit_emit_imul_reg_reg(JITCompiler* jit, X86Register dst, X86Register src) {
    jit_emit_rex(jit, true, dst, src);
    jit_emit_u8(jit, 0x0F);
    jit_emit_u8(jit, 0xAF);
    jit_emit_modrm(jit, 3, dst, src);
}

void jit_emit_and_reg_reg(JITCompiler* jit, X86Register dst, X86Register src) {
    jit_emit_rex(jit, true, src, dst);
    jit_emit_u8(jit, 0x21);
    jit_emit_modrm(jit, 3, src, dst);
}

void jit_emit_or_reg_reg(JITCompiler* jit, X86Register dst, X86Register src) {
    jit_emit_rex(jit, true, src, dst);
    jit_emit_u8(jit, 0x09);
    jit_emit_modrm(jit, 3, src, dst);
}

void jit_emit_xor_reg_reg(JITCompiler* jit, X86Register dst, X86Register src) {
    jit_emit_rex(jit, true, src, dst);
    jit_emit_u8(jit, 0x31);
    jit_emit_modrm(jit, 3, src, dst);
}

void jit_emit_cmp_reg_reg(JITCompiler* jit, X86Register r1, X86Register r2) {
    jit_emit_rex(jit, true, r2, r1);
    jit_emit_u8(jit, 0x39);
    jit_emit_modrm(jit, 3, r2, r1);
}

void jit_emit_sete_reg(JITCompiler* jit, X86Register reg) {
    jit_emit_rex(jit, false, 0, reg);
    jit_emit_u8(jit, 0x0F);
    jit_emit_u8(jit, 0x94);
    jit_emit_modrm(jit, 3, 0, reg);
}

void jit_emit_setne_reg(JITCompiler* jit, X86Register reg) {
    jit_emit_rex(jit, false, 0, reg);
    jit_emit_u8(jit, 0x0F);
    jit_emit_u8(jit, 0x95);
    jit_emit_modrm(jit, 3, 0, reg);
}

void jit_emit_setg_reg(JITCompiler* jit, X86Register reg) {
    jit_emit_rex(jit, false, 0, reg);
    jit_emit_u8(jit, 0x0F);
    jit_emit_u8(jit, 0x97);
    jit_emit_modrm(jit, 3, 0, reg);
}

void jit_emit_setl_reg(JITCompiler* jit, X86Register reg) {
    jit_emit_rex(jit, false, 0, reg);
    jit_emit_u8(jit, 0x0F);
    jit_emit_u8(jit, 0x9C);
    jit_emit_modrm(jit, 3, 0, reg);
}

void jit_emit_jmp_rel32(JITCompiler* jit, int32_t offset) {
    jit_emit_u8(jit, 0xE9);
    jit_emit_u32(jit, (uint32_t)offset);
}

void jit_emit_jz_rel32(JITCompiler* jit, int32_t offset) {
    jit_emit_u8(jit, 0x0F);
    jit_emit_u8(jit, 0x84);
    jit_emit_u32(jit, (uint32_t)offset);
}

void jit_emit_jnz_rel32(JITCompiler* jit, int32_t offset) {
    jit_emit_u8(jit, 0x0F);
    jit_emit_u8(jit, 0x85);
    jit_emit_u32(jit, (uint32_t)offset);
}

void jit_emit_call_reg(JITCompiler* jit, X86Register reg) {
    if (reg >= 8) jit_emit_u8(jit, 0x41);
    jit_emit_u8(jit, 0xFF);
    jit_emit_modrm(jit, 3, 2, reg);
}

void jit_emit_ret(JITCompiler* jit) {
    jit_emit_u8(jit, 0xC3);
}

void jit_make_executable(JITCompiler* jit) {
    if (jit->is_executable) return;
#ifdef _WIN32
    DWORD old_protect;
    VirtualProtect(jit->code_buffer, jit->size, PAGE_EXECUTE_READ, &old_protect);
#else
    mprotect(jit->code_buffer, jit->size, PROT_READ | PROT_EXEC);
#endif
    jit->is_executable = true;
}

JITFunction jit_compile_chunk(JITCompiler* jit, Chunk* chunk) {
    if (!chunk || chunk->count == 0) return NULL;
    jit->size = 0;
    jit->is_executable = false;

    /* x86_64 Calling Convention Prologue (Win64 ABI: RCX=registers, RDX=constants) */
    jit_emit_push_reg(jit, X86_REG_RBP);
    jit_emit_mov_reg_reg(jit, X86_REG_RBP, X86_REG_RSP);
    jit_emit_push_reg(jit, X86_REG_RBX);
    jit_emit_push_reg(jit, X86_REG_R12);
    jit_emit_push_reg(jit, X86_REG_R13);
    jit_emit_push_reg(jit, X86_REG_R14);
    jit_emit_push_reg(jit, X86_REG_R15);

    /* Move base register array into R14, constants array into R15 */
    jit_emit_mov_reg_reg(jit, X86_REG_R14, X86_REG_RCX);
    jit_emit_mov_reg_reg(jit, X86_REG_R15, X86_REG_RDX);

    X86Register reg_accum = X86_REG_RAX;
    X86Register reg_temp  = X86_REG_RBX;
    X86Register reg_aux   = X86_REG_RCX;

    int offset = 0;
    while (offset < chunk->count) {
        uint8_t opcode = chunk->code[offset];
        switch (opcode) {
            case OP_CONSTANT: {
                uint8_t const_idx = chunk->code[offset + 1];
                Value64 val = chunk->constants[const_idx];
                jit_emit_mov_reg_imm64(jit, reg_accum, val);
                offset += 2;
                break;
            }
            case OP_GET_GLOBAL: {
                uint8_t const_idx = chunk->code[offset + 1];
                int32_t mem_offset = (int32_t)const_idx * (int32_t)sizeof(Value64);
                jit_emit_mov_reg_mem(jit, reg_accum, X86_REG_R15, mem_offset);
                offset += 2;
                break;
            }
            case OP_SET_GLOBAL: {
                uint8_t const_idx = chunk->code[offset + 1];
                int32_t mem_offset = (int32_t)const_idx * (int32_t)sizeof(Value64);
                jit_emit_mov_mem_reg(jit, X86_REG_R15, mem_offset, reg_accum);
                offset += 2;
                break;
            }
            case OP_NIL: {
                jit_emit_mov_reg_imm64(jit, reg_accum, TAG_NIL);
                offset += 1;
                break;
            }
            case OP_TRUE: {
                jit_emit_mov_reg_imm64(jit, reg_accum, TAG_TRUE);
                offset += 1;
                break;
            }
            case OP_FALSE: {
                jit_emit_mov_reg_imm64(jit, reg_accum, TAG_FALSE);
                offset += 1;
                break;
            }
            case OP_POP: {
                jit_emit_mov_reg_imm64(jit, reg_accum, 0);
                offset += 1;
                break;
            }
            case OP_EQUAL: {
                jit_emit_cmp_reg_reg(jit, reg_accum, reg_temp);
                jit_emit_mov_reg_imm64(jit, reg_accum, TAG_FALSE);
                jit_emit_sete_reg(jit, reg_accum);
                offset += 1;
                break;
            }
            case OP_GREATER: {
                jit_emit_cmp_reg_reg(jit, reg_accum, reg_temp);
                jit_emit_mov_reg_imm64(jit, reg_accum, TAG_FALSE);
                jit_emit_setg_reg(jit, reg_accum);
                offset += 1;
                break;
            }
            case OP_LESS: {
                jit_emit_cmp_reg_reg(jit, reg_accum, reg_temp);
                jit_emit_mov_reg_imm64(jit, reg_accum, TAG_FALSE);
                jit_emit_setl_reg(jit, reg_accum);
                offset += 1;
                break;
            }
            case OP_ADD: {
                jit_emit_add_reg_reg(jit, reg_accum, reg_temp);
                offset += 1;
                break;
            }
            case OP_SUBTRACT: {
                jit_emit_sub_reg_reg(jit, reg_accum, reg_temp);
                offset += 1;
                break;
            }
            case OP_MULTIPLY: {
                jit_emit_imul_reg_reg(jit, reg_accum, reg_temp);
                offset += 1;
                break;
            }
            case OP_NOT: {
                jit_emit_xor_reg_reg(jit, reg_accum, reg_accum);
                offset += 1;
                break;
            }
            case OP_NEGATE: {
                jit_emit_mov_reg_imm64(jit, reg_aux, 0);
                jit_emit_sub_reg_reg(jit, reg_aux, reg_accum);
                jit_emit_mov_reg_reg(jit, reg_accum, reg_aux);
                offset += 1;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t jump_dist = (uint16_t)((chunk->code[offset + 1] << 8) | chunk->code[offset + 2]);
                jit_emit_mov_reg_imm64(jit, reg_temp, TAG_FALSE);
                jit_emit_cmp_reg_reg(jit, reg_accum, reg_temp);
                jit_emit_jz_rel32(jit, (int32_t)jump_dist);
                offset += 3;
                break;
            }
            case OP_LOOP: {
                uint16_t loop_dist = (uint16_t)((chunk->code[offset + 1] << 8) | chunk->code[offset + 2]);
                jit_emit_jmp_rel32(jit, -((int32_t)loop_dist));
                offset += 3;
                break;
            }
            case OP_RETURN: {
                offset += 1;
                break;
            }
            default: {
                offset += 1;
                break;
            }
        }
    }

    /* Epilogue */
    jit_emit_pop_reg(jit, X86_REG_R15);
    jit_emit_pop_reg(jit, X86_REG_R14);
    jit_emit_pop_reg(jit, X86_REG_R13);
    jit_emit_pop_reg(jit, X86_REG_R12);
    jit_emit_pop_reg(jit, X86_REG_RBX);
    jit_emit_pop_reg(jit, X86_REG_RBP);
    jit_emit_ret(jit);

    jit_make_executable(jit);
    return (JITFunction)jit->code_buffer;
}
