#include "lsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void lsp_server_init(LSPServer* lsp) {
    lsp->initialized = false;
    lsp->current_document[0] = '\0';
}

static void send_lsp_response(const char* json_body) {
    printf("Content-Length: %zu\r\n\r\n%s", strlen(json_body), json_body);
    fflush(stdout);
}

void lsp_process_message(LSPServer* lsp, const char* json_msg) {
    if (strstr(json_msg, "\"method\":\"initialize\"")) {
        lsp->initialized = true;
        const char* resp = "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"capabilities\":{\"textDocumentSync\":1,\"completionProvider\":{\"resolveProvider\":true},\"hoverProvider\":true,\"definitionProvider\":true}}}";
        send_lsp_response(resp);
    } else if (strstr(json_msg, "\"method\":\"textDocument/completion\"")) {
        const char* resp = "{\"jsonrpc\":\"2.0\",\"id\":2,\"result\":[{\"label\":\"display\",\"kind\":3,\"detail\":\"display expr\"},{\"label\":\"give back\",\"kind\":14},{\"label\":\"create_tensor\",\"kind\":3}]}";
        send_lsp_response(resp);
    } else if (strstr(json_msg, "\"method\":\"textDocument/hover\"")) {
        const char* resp = "{\"jsonrpc\":\"2.0\",\"id\":3,\"result\":{\"contents\":\"Vexium Language Symbol Definition\"}}";
        send_lsp_response(resp);
    } else if (strstr(json_msg, "\"method\":\"textDocument/didOpen\"")) {
        /* Run diagnostic pass with parser */
        const char* text_start = strstr(json_msg, "\"text\":\"");
        if (text_start) {
            Parser parser;
            parser_init(&parser, text_start + 8);
            ASTNode* ast = parser_parse(&parser);
            if (ast) ast_free(ast);
        }
    }
}

void lsp_server_run_loop(LSPServer* lsp) {
    fprintf(stderr, "[vex-lsp] Vexium Language Server initialized and listening on stdio...\n");
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), stdin)) {
        lsp_process_message(lsp, buffer);
    }
}
