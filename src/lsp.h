#ifndef VEXIUM_LSP_H
#define VEXIUM_LSP_H

#include "common.h"
#include "lexer.h"
#include "parser.h"

#define LSP_MAX_SYMBOLS 1024
#define LSP_BUF_SIZE 65536

typedef enum {
    LSP_SYMBOL_FUNCTION,
    LSP_SYMBOL_VARIABLE,
    LSP_SYMBOL_STRUCT,
    LSP_SYMBOL_KEYWORD
} LSPSymbolKind;

typedef struct {
    char name[128];
    LSPSymbolKind kind;
    int line;
    int column;
    char detail[256];
} LSPSymbol;

typedef struct {
    bool initialized;
    char current_document[LSP_BUF_SIZE];
    LSPSymbol symbols[LSP_MAX_SYMBOLS];
    int symbol_count;
} LSPServer;

void lsp_server_init(LSPServer* lsp);
void lsp_server_run_loop(LSPServer* lsp);
void lsp_process_message(LSPServer* lsp, const char* json_msg);
void lsp_index_document(LSPServer* lsp, const char* source);
const LSPSymbol* lsp_find_symbol_at(LSPServer* lsp, int line, int col);

#endif
