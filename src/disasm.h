#ifndef VEXIUM_DISASM_H
#define VEXIUM_DISASM_H

#include "chunk.h"

#define MAX_BREAKPOINTS 64

typedef struct {
    int line;
    bool verified;
} Breakpoint;

typedef struct {
    Breakpoint breakpoints[MAX_BREAKPOINTS];
    int count;
    int current_frame;
    bool is_stepping;
} DAPSession;

void disassemble_chunk(Chunk* chunk, const char* name);
int disassemble_instruction(Chunk* chunk, int offset);

void dap_init(DAPSession* session);
void dap_set_breakpoint(DAPSession* session, int line);
bool dap_should_break(DAPSession* session, int current_line);
void dap_process_command(DAPSession* session, const char* json_cmd);

#endif
