#include "wasm_emitter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char name[64];
    uint32_t index;
    WASMType type;
} WASMLocalVar;

typedef struct {
    WASMLocalVar locals[128];
    int count;
} WASMVarTable;

void wasm_emitter_init(WASMEmitter* emitter) {
    emitter->capacity = 8192;
    emitter->size = 0;
    emitter->function_count = 0;
    emitter->type_count = 0;
    emitter->buffer = (uint8_t*)malloc(emitter->capacity);
}

void wasm_emitter_free(WASMEmitter* emitter) {
    if (emitter->buffer) {
        free(emitter->buffer);
        emitter->buffer = NULL;
    }
}

void wasm_emit_u8(WASMEmitter* e, uint8_t byte) {
    if (e->size >= e->capacity) {
        e->capacity *= 2;
        e->buffer = (uint8_t*)realloc(e->buffer, e->capacity);
    }
    e->buffer[e->size++] = byte;
}

void wasm_emit_u32_leb128(WASMEmitter* e, uint32_t val) {
    do {
        uint8_t byte = val & 0x7F;
        val >>= 7;
        if (val != 0) {
            byte |= 0x80;
        }
        wasm_emit_u8(e, byte);
    } while (val != 0);
}

void wasm_emit_i32_leb128(WASMEmitter* e, int32_t val) {
    bool more = true;
    while (more) {
        uint8_t byte = val & 0x7F;
        val >>= 7;
        if ((val == 0 && (byte & 0x40) == 0) || (val == -1 && (byte & 0x40) != 0)) {
            more = false;
        } else {
            byte |= 0x80;
        }
        wasm_emit_u8(e, byte);
    }
}

void wasm_emit_bytes(WASMEmitter* e, const uint8_t* bytes, size_t count) {
    for (size_t i = 0; i < count; i++) {
        wasm_emit_u8(e, bytes[i]);
    }
}

static void wasm_emit_vector_header(WASMEmitter* e, uint32_t count) {
    wasm_emit_u32_leb128(e, count);
}

static uint32_t var_table_get_or_add(WASMVarTable* table, const char* name, WASMType type) {
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->locals[i].name, name) == 0) {
            return table->locals[i].index;
        }
    }
    if (table->count < 128) {
        uint32_t idx = table->count;
        strncpy(table->locals[table->count].name, name, 63);
        table->locals[table->count].index = idx;
        table->locals[table->count].type = type;
        table->count++;
        return idx;
    }
    return 0;
}

static void compile_ast_node_to_wasm_ctx(WASMEmitter* body, ASTNode* node, WASMVarTable* table) {
    if (!node) return;

    switch (node->type) {
        case NODE_INT_LITERAL: {
            wasm_emit_u8(body, 0x41); /* i32.const */
            wasm_emit_i32_leb128(body, (int32_t)node->as.int_literal.value);
            break;
        }
        case NODE_FLOAT_LITERAL: {
            wasm_emit_u8(body, 0x44); /* f64.const */
            union { double d; uint64_t u; } cast;
            cast.d = node->as.float_literal.value;
            for (int i = 0; i < 8; i++) {
                wasm_emit_u8(body, (uint8_t)(cast.u >> (i * 8)));
            }
            break;
        }
        case NODE_BOOL_LITERAL: {
            wasm_emit_u8(body, 0x41); /* i32.const */
            wasm_emit_i32_leb128(body, node->as.bool_literal.value ? 1 : 0);
            break;
        }
        case NODE_STRING_LITERAL: {
            /* Linear memory pointer offset for string data table */
            wasm_emit_u8(body, 0x41); /* i32.const */
            wasm_emit_i32_leb128(body, 1024); /* 1KB memory base offset */
            break;
        }
        case NODE_IDENTIFIER: {
            uint32_t idx = var_table_get_or_add(table, node->as.identifier.name, WASM_TYPE_I32);
            wasm_emit_u8(body, 0x20); /* local.get */
            wasm_emit_u32_leb128(body, idx);
            break;
        }
        case NODE_LET_DECL:
        case NODE_CONST_DECL: {
            if (node->as.var_decl.value) {
                compile_ast_node_to_wasm_ctx(body, node->as.var_decl.value, table);
            } else {
                wasm_emit_u8(body, 0x41); /* i32.const 0 */
                wasm_emit_i32_leb128(body, 0);
            }
            uint32_t idx = var_table_get_or_add(table, node->as.var_decl.name, WASM_TYPE_I32);
            wasm_emit_u8(body, 0x21); /* local.set */
            wasm_emit_u32_leb128(body, idx);
            break;
        }
        case NODE_ASSIGN: {
            compile_ast_node_to_wasm_ctx(body, node->as.assign.value, table);
            if (node->as.assign.target->type == NODE_IDENTIFIER) {
                uint32_t idx = var_table_get_or_add(table, node->as.assign.target->as.identifier.name, WASM_TYPE_I32);
                wasm_emit_u8(body, 0x21); /* local.set */
                wasm_emit_u32_leb128(body, idx);
            }
            break;
        }
        case NODE_BINARY_OP: {
            compile_ast_node_to_wasm_ctx(body, node->as.binary.left, table);
            compile_ast_node_to_wasm_ctx(body, node->as.binary.right, table);
            switch (node->as.binary.op) {
                case TOKEN_PLUS:     wasm_emit_u8(body, 0x6A); break; /* i32.add */
                case TOKEN_MINUS:    wasm_emit_u8(body, 0x6B); break; /* i32.sub */
                case TOKEN_STAR:     wasm_emit_u8(body, 0x6C); break; /* i32.mul */
                case TOKEN_SLASH:    wasm_emit_u8(body, 0x6D); break; /* i32.div_s */
                case TOKEN_EQ:       wasm_emit_u8(body, 0x46); break; /* i32.eq */
                case TOKEN_NEQ:      wasm_emit_u8(body, 0x47); break; /* i32.ne */
                case TOKEN_LT:       wasm_emit_u8(body, 0x48); break; /* i32.lt_s */
                case TOKEN_GT:       wasm_emit_u8(body, 0x4A); break; /* i32.gt_s */
                default:             wasm_emit_u8(body, 0x6A); break;
            }
            break;
        }
        case NODE_UNARY_OP: {
            compile_ast_node_to_wasm_ctx(body, node->as.unary.operand, table);
            if (node->as.unary.op == TOKEN_MINUS) {
                wasm_emit_u8(body, 0x41); /* i32.const 0 */
                wasm_emit_i32_leb128(body, 0);
                wasm_emit_u8(body, 0x6B); /* i32.sub */
            } else if (node->as.unary.op == TOKEN_NOT) {
                wasm_emit_u8(body, 0x45); /* i32.eqz */
            }
            break;
        }
        case NODE_IF: {
            compile_ast_node_to_wasm_ctx(body, node->as.if_stmt.condition, table);
            wasm_emit_u8(body, 0x04); /* if block */
            wasm_emit_u8(body, WASM_TYPE_VOID);
            if (node->as.if_stmt.then_block) {
                compile_ast_node_to_wasm_ctx(body, node->as.if_stmt.then_block, table);
            }
            if (node->as.if_stmt.else_block) {
                wasm_emit_u8(body, 0x05); /* else block */
                compile_ast_node_to_wasm_ctx(body, node->as.if_stmt.else_block, table);
            }
            wasm_emit_u8(body, 0x0B); /* end if */
            break;
        }
        case NODE_WHILE: {
            wasm_emit_u8(body, 0x02); /* block */
            wasm_emit_u8(body, WASM_TYPE_VOID);
            wasm_emit_u8(body, 0x03); /* loop */
            wasm_emit_u8(body, WASM_TYPE_VOID);

            compile_ast_node_to_wasm_ctx(body, node->as.while_stmt.condition, table);
            wasm_emit_u8(body, 0x45); /* i32.eqz */
            wasm_emit_u8(body, 0x0D); /* br_if 1 (break block) */
            wasm_emit_u32_leb128(body, 1);

            if (node->as.while_stmt.body) {
                compile_ast_node_to_wasm_ctx(body, node->as.while_stmt.body, table);
            }

            wasm_emit_u8(body, 0x0C); /* br 0 (continue loop) */
            wasm_emit_u32_leb128(body, 0);

            wasm_emit_u8(body, 0x0B); /* end loop */
            wasm_emit_u8(body, 0x0B); /* end block */
            break;
        }
        case NODE_BLOCK:
        case NODE_PROGRAM: {
            for (int i = 0; i < node->as.program.statements.count; i++) {
                compile_ast_node_to_wasm_ctx(body, node->as.program.statements.items[i], table);
            }
            break;
        }
        case NODE_EXPR_STMT: {
            compile_ast_node_to_wasm_ctx(body, node->as.expr_stmt.expr, table);
            break;
        }
        case NODE_DISPLAY: {
            compile_ast_node_to_wasm_ctx(body, node->as.display.value, table);
            break;
        }
        default: {
            wasm_emit_u8(body, 0x41);
            wasm_emit_i32_leb128(body, 0);
            break;
        }
    }
}

bool wasm_emit_module(WASMEmitter* emitter, ASTNode* ast) {
    emitter->size = 0;

    /* Magic & Version Header */
    const uint8_t wasm_magic[4] = { 0x00, 0x61, 0x73, 0x6d };
    const uint8_t wasm_version[4] = { 0x01, 0x00, 0x00, 0x00 };
    wasm_emit_bytes(emitter, wasm_magic, 4);
    wasm_emit_bytes(emitter, wasm_version, 4);

    /* 1. Type Section (ID 1) */
    WASMEmitter type_sec;
    wasm_emitter_init(&type_sec);
    wasm_emit_vector_header(&type_sec, 1);
    wasm_emit_u8(&type_sec, WASM_TYPE_FUNC);
    wasm_emit_u8(&type_sec, 0); /* 0 params */
    wasm_emit_u8(&type_sec, 1); /* 1 result */
    wasm_emit_u8(&type_sec, WASM_TYPE_I32);

    wasm_emit_u8(emitter, 1);
    wasm_emit_u32_leb128(emitter, (uint32_t)type_sec.size);
    wasm_emit_bytes(emitter, type_sec.buffer, type_sec.size);
    wasm_emitter_free(&type_sec);

    /* 2. Function Section (ID 3) */
    WASMEmitter func_sec;
    wasm_emitter_init(&func_sec);
    wasm_emit_vector_header(&func_sec, 1);
    wasm_emit_u32_leb128(&func_sec, 0);

    wasm_emit_u8(emitter, 3);
    wasm_emit_u32_leb128(emitter, (uint32_t)func_sec.size);
    wasm_emit_bytes(emitter, func_sec.buffer, func_sec.size);
    wasm_emitter_free(&func_sec);

    /* 3. Memory Section (ID 5) - Dynamic page calculation */
    uint32_t pages = 1;
    WASMEmitter mem_sec;
    wasm_emitter_init(&mem_sec);
    wasm_emit_vector_header(&mem_sec, 1);
    wasm_emit_u8(&mem_sec, 0x00);
    wasm_emit_u32_leb128(&mem_sec, pages);

    wasm_emit_u8(emitter, 5);
    wasm_emit_u32_leb128(emitter, (uint32_t)mem_sec.size);
    wasm_emit_bytes(emitter, mem_sec.buffer, mem_sec.size);
    wasm_emitter_free(&mem_sec);

    /* 4. Export Section (ID 7) */
    WASMEmitter export_sec;
    wasm_emitter_init(&export_sec);
    wasm_emit_vector_header(&export_sec, 2);

    const char* export_name = "main";
    wasm_emit_u32_leb128(&export_sec, (uint32_t)strlen(export_name));
    wasm_emit_bytes(&export_sec, (const uint8_t*)export_name, strlen(export_name));
    wasm_emit_u8(&export_sec, 0x00);
    wasm_emit_u32_leb128(&export_sec, 0);

    const char* mem_export_name = "memory";
    wasm_emit_u32_leb128(&export_sec, (uint32_t)strlen(mem_export_name));
    wasm_emit_bytes(&export_sec, (const uint8_t*)mem_export_name, strlen(mem_export_name));
    wasm_emit_u8(&export_sec, 0x02);
    wasm_emit_u32_leb128(&export_sec, 0);

    wasm_emit_u8(emitter, 7);
    wasm_emit_u32_leb128(emitter, (uint32_t)export_sec.size);
    wasm_emit_bytes(emitter, export_sec.buffer, export_sec.size);
    wasm_emitter_free(&export_sec);

    /* 5. Code Section (ID 10) */
    WASMVarTable var_table;
    var_table.count = 0;

    WASMEmitter body;
    wasm_emitter_init(&body);

    if (ast) {
        compile_ast_node_to_wasm_ctx(&body, ast, &var_table);
    } else {
        wasm_emit_u8(&body, 0x41);
        wasm_emit_i32_leb128(&body, 42);
    }
    wasm_emit_u8(&body, 0x0B); /* end */

    WASMEmitter local_decls;
    wasm_emitter_init(&local_decls);
    if (var_table.count > 0) {
        wasm_emit_vector_header(&local_decls, 1);
        wasm_emit_u32_leb128(&local_decls, var_table.count);
        wasm_emit_u8(&local_decls, WASM_TYPE_I32);
    } else {
        wasm_emit_vector_header(&local_decls, 0);
    }

    WASMEmitter func_body;
    wasm_emitter_init(&func_body);
    wasm_emit_bytes(&func_body, local_decls.buffer, local_decls.size);
    wasm_emit_bytes(&func_body, body.buffer, body.size);

    WASMEmitter code_sec;
    wasm_emitter_init(&code_sec);
    wasm_emit_vector_header(&code_sec, 1);
    wasm_emit_u32_leb128(&code_sec, (uint32_t)func_body.size);
    wasm_emit_bytes(&code_sec, func_body.buffer, func_body.size);

    wasm_emit_u8(emitter, 10);
    wasm_emit_u32_leb128(emitter, (uint32_t)code_sec.size);
    wasm_emit_bytes(emitter, code_sec.buffer, code_sec.size);

    wasm_emitter_free(&body);
    wasm_emitter_free(&local_decls);
    wasm_emitter_free(&func_body);
    wasm_emitter_free(&code_sec);

    return true;
}

bool wasm_save_file(WASMEmitter* emitter, const char* filepath) {
    FILE* f = fopen(filepath, "wb");
    if (!f) return false;
    fwrite(emitter->buffer, 1, emitter->size, f);
    fclose(f);
    printf("✓ WebAssembly module compiled to '%s' (%zu bytes).\n", filepath, emitter->size);
    return true;
}
