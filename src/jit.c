#include "jit.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

void jit_init(JITCompiler* jit) {
    jit->capacity = 4096;
    jit->size = 0;
    jit->is_executable = false;

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

static void emit_byte(JITCompiler* jit, uint8_t byte) {
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
    UNUSED(chunk);
    jit->size = 0;
    jit->is_executable = false;

    /* x86_64 Machine Code Sequence:
     * push rbp (0x55)
     * mov rbp, rsp (0x48, 0x89, 0xE5)
     * mov rax, rcx (0x48, 0x89, 0xC8 - RCX is 1st arg in Win64 ABI)
     * mov rax, [rax] (0x48, 0x8B, 0x00)
     * pop rbp (0x5D)
     * ret (0xC3)
     */
    emit_byte(jit, 0x55);
    emit_byte(jit, 0x48); emit_byte(jit, 0x89); emit_byte(jit, 0xE5);
    emit_byte(jit, 0x48); emit_byte(jit, 0x89); emit_byte(jit, 0xC8);
    emit_byte(jit, 0x48); emit_byte(jit, 0x8B); emit_byte(jit, 0x00);
    emit_byte(jit, 0x5D);
    emit_byte(jit, 0xC3);

    jit_make_executable(jit);
    return (JITFunction)jit->code_buffer;
}
