#include "lsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void lsp_server_init(LSPServer* lsp) {
    lsp->initialized = false;
    lsp->current_document[0] = '\0';
    lsp->symbol_count = 0;
}

static void send_lsp_response(const char* json_body) {
    printf("Content-Length: %zu\r\n\r\n%s", strlen(json_body), json_body);
    fflush(stdout);
}

static int extract_json_id(const char* json_msg) {
    const char* id_ptr = strstr(json_msg, "\"id\":");
    if (!id_ptr) return 1;
    int id = 1;
    sscanf(id_ptr + 5, "%d", &id);
    return id;
}

static bool extract_json_string_field(const char* json, const char* field, char* out_buf, size_t max_len) {
    char search_pattern[64];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":\"", field);
    const char* start = strstr(json, search_pattern);
    if (!start) return false;
    start += strlen(search_pattern);
    const char* end = strchr(start, '"');
    if (!end) return false;
    size_t len = (size_t)(end - start);
    if (len >= max_len) len = max_len - 1;
    memcpy(out_buf, start, len);
    out_buf[len] = '\0';
    return true;
}

void lsp_index_document(LSPServer* lsp, const char* source) {
    if (!source) return;
    lsp->symbol_count = 0;

    Parser parser;
    parser_init(&parser, source);
    ASTNode* program = parser_parse(&parser);

    if (program && program->type == NODE_PROGRAM) {
        for (int i = 0; i < program->as.program.statements.count && lsp->symbol_count < LSP_MAX_SYMBOLS; i++) {
            ASTNode* stmt = program->as.program.statements.items[i];
            if (stmt->type == NODE_FN_DECL) {
                LSPSymbol* sym = &lsp->symbols[lsp->symbol_count++];
                snprintf(sym->name, sizeof(sym->name), "%s", stmt->as.fn_decl.name);
                sym->kind = LSP_SYMBOL_FUNCTION;
                sym->line = stmt->line;
                sym->column = stmt->column;
                snprintf(sym->detail, sizeof(sym->detail), "fn %s() -> %s", stmt->as.fn_decl.name,
                    stmt->as.fn_decl.return_type ? stmt->as.fn_decl.return_type : "any");
            } else if (stmt->type == NODE_LET_DECL || stmt->type == NODE_CONST_DECL) {
                LSPSymbol* sym = &lsp->symbols[lsp->symbol_count++];
                snprintf(sym->name, sizeof(sym->name), "%s", stmt->as.var_decl.name);
                sym->kind = LSP_SYMBOL_VARIABLE;
                sym->line = stmt->line;
                sym->column = stmt->column;
                snprintf(sym->detail, sizeof(sym->detail), "let %s: %s", stmt->as.var_decl.name,
                    stmt->as.var_decl.type_name ? stmt->as.var_decl.type_name : "any");
            } else if (stmt->type == NODE_STRUCT_DECL) {
                LSPSymbol* sym = &lsp->symbols[lsp->symbol_count++];
                snprintf(sym->name, sizeof(sym->name), "%s", stmt->as.struct_decl.name);
                sym->kind = LSP_SYMBOL_STRUCT;
                sym->line = stmt->line;
                sym->column = stmt->column;
                snprintf(sym->detail, sizeof(sym->detail), "struct %s", stmt->as.struct_decl.name);
            }
        }
        ast_free(program);
    }
}

const LSPSymbol* lsp_find_symbol_at(LSPServer* lsp, int line, int col) {
    UNUSED(col);
    for (int i = 0; i < lsp->symbol_count; i++) {
        if (lsp->symbols[i].line == line) {
            return &lsp->symbols[i];
        }
    }
    return NULL;
}

static void lsp_publish_diagnostics(const char* uri, const char* source) {
    if (!source) return;
    Parser parser;
    parser_init(&parser, source);
    ASTNode* ast = parser_parse(&parser);

    char resp[2048];
    if (parser.had_error) {
        snprintf(resp, sizeof(resp),
            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":\"%s\",\"diagnostics\":[{\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":10}},\"severity\":1,\"message\":\"Syntax error detected in Vexium source file\"}]}}",
            uri);
    } else {
        snprintf(resp, sizeof(resp),
            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":\"%s\",\"diagnostics\":[]}}",
            uri);
    }
    send_lsp_response(resp);
    if (ast) ast_free(ast);
}

void lsp_process_message(LSPServer* lsp, const char* json_msg) {
    int req_id = extract_json_id(json_msg);

    if (strstr(json_msg, "\"method\":\"initialize\"")) {
        lsp->initialized = true;
        char resp[512];
        snprintf(resp, sizeof(resp),
            "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{\"capabilities\":{\"textDocumentSync\":1,\"completionProvider\":{\"resolveProvider\":true},\"hoverProvider\":true,\"definitionProvider\":true,\"documentFormattingProvider\":true}}}", req_id);
        send_lsp_response(resp);
    } else if (strstr(json_msg, "\"method\":\"textDocument/completion\"")) {
        char resp[2048];
        int pos = snprintf(resp, sizeof(resp), "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":[", req_id);
        
        pos += snprintf(resp + pos, sizeof(resp) - pos, "{\"label\":\"display\",\"kind\":3,\"detail\":\"display expr\"},{\"label\":\"give back\",\"kind\":14},{\"label\":\"create_tensor\",\"kind\":3}");

        for (int i = 0; i < lsp->symbol_count; i++) {
            pos += snprintf(resp + pos, sizeof(resp) - pos,
                ",{\"label\":\"%s\",\"kind\":%d,\"detail\":\"%s\"}",
                lsp->symbols[i].name,
                (lsp->symbols[i].kind == LSP_SYMBOL_FUNCTION) ? 3 : 6,
                lsp->symbols[i].detail);
        }
        snprintf(resp + pos, sizeof(resp) - pos, "]}");
        send_lsp_response(resp);
    } else if (strstr(json_msg, "\"method\":\"textDocument/hover\"")) {
        char resp[1024];
        snprintf(resp, sizeof(resp),
            "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{\"contents\":{\"kind\":\"markdown\",\"value\":\"**Vexium Language Engine**: Active document contains %d indexed symbols.\"}}}",
            req_id, lsp->symbol_count);
        send_lsp_response(resp);
    } else if (strstr(json_msg, "\"method\":\"textDocument/definition\"")) {
        char resp[1024];
        if (lsp->symbol_count > 0 && lsp->current_document[0] != '\0') {
            snprintf(resp, sizeof(resp),
                "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}}}}",
                req_id, lsp->current_document, lsp->symbols[0].line, lsp->symbols[0].column, lsp->symbols[0].line, lsp->symbols[0].column + 10);
        } else {
            snprintf(resp, sizeof(resp), "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":null}", req_id);
        }
        send_lsp_response(resp);
    } else if (strstr(json_msg, "\"method\":\"textDocument/didOpen\"")) {
        extract_json_string_field(json_msg, "uri", lsp->current_document, sizeof(lsp->current_document));
        const char* text_start = strstr(json_msg, "\"text\":\"");
        if (text_start) {
            lsp_index_document(lsp, text_start + 8);
            lsp_publish_diagnostics(lsp->current_document, text_start + 8);
        }
    }
}

void lsp_server_run_loop(LSPServer* lsp) {
    fprintf(stderr, "[vex-lsp] Vexium Language Server initialized and listening on stdio...\n");
    char buffer[LSP_BUF_SIZE];
    while (fgets(buffer, sizeof(buffer), stdin)) {
        lsp_process_message(lsp, buffer);
    }
}
