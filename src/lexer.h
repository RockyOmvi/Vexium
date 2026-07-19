#ifndef VEXIUM_LEXER_H
#define VEXIUM_LEXER_H

#include "common.h"
#include "token.h"

typedef struct {
    const char* source;
    const char* start;
    const char* current;
    int line;
    int column;

    int indent_stack[VEXIUM_MAX_INDENT_DEPTH];
    int indent_depth;
    int pending_dedents;
    bool at_line_start;
    bool emit_newline;
} Lexer;

void lexer_init(Lexer* lexer, const char* source);
Token lexer_next_token(Lexer* lexer);

#endif
