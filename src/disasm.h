#ifndef VEXIUM_DISASM_H
#define VEXIUM_DISASM_H

#include "chunk.h"
#include "vm.h"

#define MAX_BREAKPOINTS 128
#define MAX_DAP_STACK_FRAMES 64

typedef struct {
    int line;
    bool verified;
    char source_file[256];
} Breakpoint;

typedef struct {
    int frame_id;
    char name[128];
    int line;
    char source_path[256];
} DAPStackFrame;

typedef struct {
    Breakpoint breakpoints[MAX_BREAKPOINTS];
    int count;
    DAPStackFrame stack_frames[MAX_DAP_STACK_FRAMES];
    int frame_count;
    bool is_stepping;
    bool is_paused;
    int current_line;
} DAPSession;

void disassemble_chunk(Chunk* chunk, const char* name);
int disassemble_instruction(Chunk* chunk, int offset);

void dap_init(DAPSession* session);
void dap_set_breakpoint(DAPSession* session, int line, const char* source_file);
void dap_clear_breakpoints(DAPSession* session);
bool dap_should_break(DAPSession* session, int current_line);
void dap_process_command(DAPSession* session, const char* json_cmd);
void dap_inspect_vm_stack(DAPSession* session, VM* vm);

#endif
