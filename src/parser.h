#ifndef VEXIUM_PARSER_H
#define VEXIUM_PARSER_H

#include "common.h"
#include "lexer.h"
#include "ast.h"

typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
} Parser;

void parser_init(Parser* parser, const char* source);
ASTNode* parser_parse(Parser* parser);

#endif
