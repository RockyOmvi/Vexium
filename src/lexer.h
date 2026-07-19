#ifndef VEXIUM_LEXER_H
#define VEXIUM_LEXER_H

#include "common.h"
#include "token.h"

/* ── Lexer State ── */
typedef struct {
    const char* source;         /* full source code */
    const char* start;          /* start of current token */
    const char* current;        /* current scan position */
    int line;                   /* current line (1-based) */
    int column;                 /* current column (1-based) */

    /* Indentation tracking (Python-style) */
    int indent_stack[VEXIUM_MAX_INDENT_DEPTH];
    int indent_depth;           /* current depth in indent_stack */
    int pending_dedents;        /* number of DEDENT tokens to emit */
    bool at_line_start;         /* are we at the beginning of a line? */
    bool emit_newline;          /* should we emit a NEWLINE next? */
} Lexer;

/* ── Public API ── */
void lexer_init(Lexer* lexer, const char* source);
Token lexer_next_token(Lexer* lexer);

#endif /* VEXIUM_LEXER_H */
