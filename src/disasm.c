#include "disasm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int simple_instruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int constant_instruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    print_value64(chunk->constants[constant]);
    printf("'\n");
    return offset + 2;
}

int disassemble_instruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);

    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:   return constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_NIL:        return simple_instruction("OP_NIL", offset);
        case OP_TRUE:       return simple_instruction("OP_TRUE", offset);
        case OP_FALSE:      return simple_instruction("OP_FALSE", offset);
        case OP_POP:        return simple_instruction("OP_POP", offset);
        case OP_EQUAL:      return simple_instruction("OP_EQUAL", offset);
        case OP_GREATER:    return simple_instruction("OP_GREATER", offset);
        case OP_LESS:       return simple_instruction("OP_LESS", offset);
        case OP_ADD:        return simple_instruction("OP_ADD", offset);
        case OP_SUBTRACT:   return simple_instruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:   return simple_instruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:     return simple_instruction("OP_DIVIDE", offset);
        case OP_NOT:        return simple_instruction("OP_NOT", offset);
        case OP_NEGATE:     return simple_instruction("OP_NEGATE", offset);
        case OP_PRINT:      return simple_instruction("OP_PRINT", offset);
        case OP_CLASS:      return constant_instruction("OP_CLASS", chunk, offset);
        case OP_METHOD:     return constant_instruction("OP_METHOD", chunk, offset);
        case OP_INVOKE:     return constant_instruction("OP_INVOKE", chunk, offset);
        case OP_CLOSURE:    return constant_instruction("OP_CLOSURE", chunk, offset);
        case OP_IMPORT:     return constant_instruction("OP_IMPORT", chunk, offset);
        case OP_RETURN:     return simple_instruction("OP_RETURN", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

void disassemble_chunk(Chunk* chunk, const char* name) {
    printf("=== BYTECODE DISASSEMBLY: %s ===\n", name);
    for (int offset = 0; offset < chunk->count;) {
        offset = disassemble_instruction(chunk, offset);
    }
    printf("===================================\n");
}

/* DAP Debugger Protocol Implementation */
void dap_init(DAPSession* session) {
    session->count = 0;
    session->current_frame = 0;
    session->is_stepping = false;
}

void dap_set_breakpoint(DAPSession* session, int line) {
    if (session->count < MAX_BREAKPOINTS) {
        session->breakpoints[session->count].line = line;
        session->breakpoints[session->count].verified = true;
        session->count++;
    }
}

bool dap_should_break(DAPSession* session, int current_line) {
    if (session->is_stepping) return true;
    for (int i = 0; i < session->count; i++) {
        if (session->breakpoints[i].line == current_line) return true;
    }
    return false;
}

void dap_process_command(DAPSession* session, const char* json_cmd) {
    if (strstr(json_cmd, "\"command\":\"setBreakpoints\"")) {
        printf("{\"jsonrpc\":\"2.0\",\"command\":\"setBreakpoints\",\"success\":true}\n");
    } else if (strstr(json_cmd, "\"command\":\"next\"")) {
        session->is_stepping = true;
    } else if (strstr(json_cmd, "\"command\":\"continue\"")) {
        session->is_stepping = false;
    }
}
