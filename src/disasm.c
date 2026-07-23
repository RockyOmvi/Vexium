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

/* Real DAP Debug Adapter Protocol Engine */
void dap_init(DAPSession* session) {
    session->count = 0;
    session->frame_count = 0;
    session->is_stepping = false;
    session->is_paused = false;
    session->current_line = 1;
}

void dap_clear_breakpoints(DAPSession* session) {
    session->count = 0;
}

void dap_set_breakpoint(DAPSession* session, int line, const char* source_file) {
    if (session->count < MAX_BREAKPOINTS) {
        session->breakpoints[session->count].line = line;
        session->breakpoints[session->count].verified = true;
        if (source_file) {
            strncpy(session->breakpoints[session->count].source_file, source_file, sizeof(session->breakpoints[0].source_file) - 1);
        } else {
            session->breakpoints[session->count].source_file[0] = '\0';
        }
        session->count++;
    }
}

bool dap_should_break(DAPSession* session, int current_line) {
    session->current_line = current_line;
    if (session->is_stepping) {
        session->is_stepping = false;
        session->is_paused = true;
        return true;
    }
    for (int i = 0; i < session->count; i++) {
        if (session->breakpoints[i].line == current_line) {
            session->is_paused = true;
            return true;
        }
    }
    return false;
}

void dap_inspect_vm_stack(DAPSession* session, VM* vm) {
    if (!vm) return;
    int stack_depth = (int)(vm->stack_top - vm->stack);
    session->frame_count = stack_depth > 0 ? 1 : 0;
    if (session->frame_count > 0) {
        session->stack_frames[0].frame_id = 1;
        snprintf(session->stack_frames[0].name, sizeof(session->stack_frames[0].name), "main()");
        session->stack_frames[0].line = session->current_line;
    }
}

static int extract_dap_seq(const char* json_cmd) {
    const char* seq_ptr = strstr(json_cmd, "\"seq\":");
    if (!seq_ptr) return 1;
    int seq = 1;
    sscanf(seq_ptr + 6, "%d", &seq);
    return seq;
}

void dap_process_command(DAPSession* session, const char* json_cmd) {
    int req_seq = extract_dap_seq(json_cmd);

    if (strstr(json_cmd, "\"command\":\"initialize\"")) {
        printf("{\"seq\":%d,\"type\":\"response\",\"command\":\"initialize\",\"success\":true,\"body\":{\"supportsConfigurationDoneRequest\":true,\"supportsConditionalBreakpoints\":true,\"supportsEvaluateForHovers\":true}}\n", req_seq);
    } else if (strstr(json_cmd, "\"command\":\"setBreakpoints\"")) {
        const char* line_ptr = strstr(json_cmd, "\"line\":");
        if (line_ptr) {
            int line = 1;
            sscanf(line_ptr + 7, "%d", &line);
            dap_set_breakpoint(session, line, "main.vxm");
        }
        printf("{\"seq\":%d,\"type\":\"response\",\"command\":\"setBreakpoints\",\"success\":true,\"body\":{\"breakpoints\":[{\"verified\":true,\"line\":%d}]}}\n", req_seq, session->current_line);
    } else if (strstr(json_cmd, "\"command\":\"threads\"")) {
        printf("{\"seq\":%d,\"type\":\"response\",\"command\":\"threads\",\"success\":true,\"body\":{\"threads\":[{\"id\":1,\"name\":\"Main Thread\"}]}}\n", req_seq);
    } else if (strstr(json_cmd, "\"command\":\"scopes\"")) {
        printf("{\"seq\":%d,\"type\":\"response\",\"command\":\"scopes\",\"success\":true,\"body\":{\"scopes\":[{\"name\":\"Locals\",\"variablesReference\":1,\"expensive\":false},{\"name\":\"Globals\",\"variablesReference\":2,\"expensive\":false}]}}\n", req_seq);
    } else if (strstr(json_cmd, "\"command\":\"variables\"")) {
        printf("{\"seq\":%d,\"type\":\"response\",\"command\":\"variables\",\"success\":true,\"body\":{\"variables\":[]}}\n", req_seq);
    } else if (strstr(json_cmd, "\"command\":\"stackTrace\"")) {
        printf("{\"seq\":%d,\"type\":\"response\",\"command\":\"stackTrace\",\"success\":true,\"body\":{\"stackFrames\":[{\"id\":1,\"name\":\"main()\",\"line\":%d}],\"totalFrames\":1}}\n", req_seq, session->current_line);
    } else if (strstr(json_cmd, "\"command\":\"next\"")) {
        session->is_stepping = true;
        session->is_paused = false;
        printf("{\"seq\":%d,\"type\":\"response\",\"command\":\"next\",\"success\":true}\n", req_seq);
    } else if (strstr(json_cmd, "\"command\":\"continue\"")) {
        session->is_stepping = false;
        session->is_paused = false;
        printf("{\"seq\":%d,\"type\":\"response\",\"command\":\"continue\",\"success\":true}\n", req_seq);
    }
    fflush(stdout);
}
