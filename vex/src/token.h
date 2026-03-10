#ifndef VEXIUM_TOKEN_H
#define VEXIUM_TOKEN_H

#include "common.h"

/* ── Token Types ── */
typedef enum VexiumTokenType {
    /* Literals */
    TOKEN_INT,              /* 42, 0xFF, 0b1010 */
    TOKEN_FLOAT,            /* 3.14, 1.0e10 */
    TOKEN_STRING,           /* "hello", 'world' */

    /* Identifiers */
    TOKEN_IDENTIFIER,       /* variable/function names */

    /* Keywords: Variables */
    TOKEN_LET,              /* let */
    TOKEN_BE,               /* be */
    TOKEN_CONST,            /* const */

    /* Keywords: Functions */
    TOKEN_FN,               /* fn */
    TOKEN_GIVE_BACK,        /* give back (return) */
    TOKEN_TASK,             /* task (async fn) */
    TOKEN_SPAWN,            /* spawn (create concurrent task) */
    TOKEN_DEFER,            /* defer */
    TOKEN_AWAIT,            /* await */
    TOKEN_YIELD,            /* yield (generator) */
    TOKEN_CHANNEL,          /* channel (for concurrency) */
    TOKEN_PASS,             /* pass */

    /* Keywords: Control Flow */
    TOKEN_IF,               /* if */
    TOKEN_ELIF,             /* elif */
    TOKEN_ELSE,             /* else */
    TOKEN_WHILE,            /* while */
    TOKEN_FOR,              /* for */
    TOKEN_EACH,             /* each */
    TOKEN_REPEAT,           /* repeat */
    TOKEN_TIMES,            /* times */
    TOKEN_IN,               /* in */
    TOKEN_TO,               /* to */
    TOKEN_BY,               /* by */
    TOKEN_BREAK,            /* break */
    TOKEN_SKIP,             /* skip (continue) */
    TOKEN_MATCH,            /* match */
    TOKEN_WHEN,             /* when */
    TOKEN_WHERE,            /* where */

    /* Keywords: Logic & Values */
    TOKEN_AND,              /* and */
    TOKEN_OR,               /* or */
    TOKEN_NOT,              /* not */
    TOKEN_IS,               /* is */
    TOKEN_TRUE,             /* true */
    TOKEN_FALSE,            /* false */
    TOKEN_NOTHING,          /* nothing */

    /* Keywords: Structure */
    TOKEN_USE,              /* use */
    TOKEN_FROM,             /* from */
    TOKEN_STRUCT,           /* struct */
    TOKEN_HAS,              /* has */
    TOKEN_CAN,              /* can */
    TOKEN_SELF,             /* self */
    TOKEN_TRAIT,            /* trait */
    TOKEN_IMPL,             /* impl */
    TOKEN_OPERATOR,         /* operator */
    TOKEN_ATTEMPT,          /* attempt */
    TOKEN_OTHERWISE,        /* otherwise */
    TOKEN_UNSAFE,           /* unsafe */
    TOKEN_EXTENDS,          /* extends */

    /* Keywords: I/O */
    TOKEN_DISPLAY,          /* display */

    /* English compound operators (multi-word, resolved by lexer) */
    TOKEN_IS_NOT,           /* is not */
    TOKEN_IS_GREATER,       /* is greater than */
    TOKEN_IS_LESS,          /* is less than */
    TOKEN_IS_AT_LEAST,      /* is at least */
    TOKEN_IS_AT_MOST,       /* is at most */
    TOKEN_NOT_EXISTS,       /* not exists in */
    TOKEN_EXISTS,           /* exists in */

    /* Arithmetic Operators */
    TOKEN_PLUS,             /* + */
    TOKEN_MINUS,            /* - */
    TOKEN_STAR,             /* * */
    TOKEN_SLASH,            /* / */
    TOKEN_PERCENT,          /* % */
    TOKEN_POWER,            /* ** */

    /* Comparison Operators */
    TOKEN_EQ,               /* == */
    TOKEN_NEQ,              /* != */
    TOKEN_LT,               /* < */
    TOKEN_GT,               /* > */
    TOKEN_LTE,              /* <= */
    TOKEN_GTE,              /* >= */

    /* Assignment */
    TOKEN_ASSIGN,           /* = */
    TOKEN_PLUS_ASSIGN,      /* += */
    TOKEN_MINUS_ASSIGN,     /* -= */
    TOKEN_STAR_ASSIGN,      /* *= */
    TOKEN_SLASH_ASSIGN,     /* /= */

    /* Delimiters */
    TOKEN_LPAREN,           /* ( */
    TOKEN_RPAREN,           /* ) */
    TOKEN_LBRACKET,         /* [ */
    TOKEN_RBRACKET,         /* ] */
    TOKEN_LBRACE,           /* { */
    TOKEN_RBRACE,           /* } */
    TOKEN_COMMA,            /* , */
    TOKEN_DOT,              /* . */
    TOKEN_COLON,            /* : */
    TOKEN_ARROW,            /* -> */
    TOKEN_FAT_ARROW,        /* => */
    TOKEN_AT,               /* @ */

    /* Indentation (Python-style) */
    TOKEN_NEWLINE,          /* end of logical line */
    TOKEN_INDENT,           /* increase in indentation */
    TOKEN_DEDENT,           /* decrease in indentation */

    /* Special */
    TOKEN_EOF,              /* end of file */
    TOKEN_ERROR             /* lexer error */
} VexiumTokenType;

/* ── Token Structure ── */
typedef struct {
    VexiumTokenType type;
    const char* start;     /* pointer into source string */
    int length;            /* length of the token text */
    int line;              /* 1-based line number */
    int column;            /* 1-based column number */
} Token;

/* ── Token Utilities ── */
const char* token_type_name(VexiumTokenType type);
void token_print(Token token);

#endif /* VEXIUM_TOKEN_H */
