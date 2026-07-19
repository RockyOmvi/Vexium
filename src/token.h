#ifndef VEXIUM_TOKEN_H
#define VEXIUM_TOKEN_H

#include "common.h"

typedef enum {

    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_STRING,

    TOKEN_IDENTIFIER,

    TOKEN_LET,
    TOKEN_BE,
    TOKEN_CONST,

    TOKEN_FN,
    TOKEN_GIVE_BACK,
    TOKEN_TASK,
    TOKEN_PASS,

    TOKEN_IF,
    TOKEN_ELIF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_EACH,
    TOKEN_REPEAT,
    TOKEN_TIMES,
    TOKEN_IN,
    TOKEN_TO,
    TOKEN_BY,
    TOKEN_BREAK,
    TOKEN_SKIP,
    TOKEN_MATCH,

    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_IS,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NOTHING,

    TOKEN_USE,
    TOKEN_FROM,
    TOKEN_STRUCT,
    TOKEN_HAS,
    TOKEN_CAN,
    TOKEN_SELF,
    TOKEN_ATTEMPT,
    TOKEN_OTHERWISE,
    TOKEN_UNSAFE,
    TOKEN_EXTENDS,

    TOKEN_DISPLAY,

    TOKEN_IS_NOT,
    TOKEN_IS_GREATER,
    TOKEN_IS_LESS,
    TOKEN_IS_AT_LEAST,
    TOKEN_IS_AT_MOST,
    TOKEN_NOT_EXISTS,
    TOKEN_EXISTS,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_POWER,

    TOKEN_EQ,
    TOKEN_NEQ,
    TOKEN_LT,
    TOKEN_GT,
    TOKEN_LTE,
    TOKEN_GTE,

    TOKEN_ASSIGN,
    TOKEN_PLUS_ASSIGN,
    TOKEN_MINUS_ASSIGN,
    TOKEN_STAR_ASSIGN,
    TOKEN_SLASH_ASSIGN,

    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_COLON,
    TOKEN_ARROW,
    TOKEN_FAT_ARROW,
    TOKEN_AT,               /* @ */

    TOKEN_NEWLINE,
    TOKEN_INDENT,
    TOKEN_DEDENT,

    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
    int column;
} Token;

const char* token_type_name(TokenType type);
void token_print(Token token);

#endif
