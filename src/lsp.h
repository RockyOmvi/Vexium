#ifndef VEXIUM_LSP_H
#define VEXIUM_LSP_H

#include "common.h"
#include "lexer.h"
#include "parser.h"

typedef struct {
    bool initialized;
    char current_document[4096];
} LSPServer;

void lsp_server_init(LSPServer* lsp);
void lsp_server_run_loop(LSPServer* lsp);
void lsp_process_message(LSPServer* lsp, const char* json_msg);

#endif
