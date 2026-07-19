#include "lexer.h"

/* ══════════════════════════════════════════════════════════════
 *  HELPER FUNCTIONS
 * ══════════════════════════════════════════════════════════════ */

static bool is_at_end(Lexer* lexer) {
    return *lexer->current == '\0';
}

static char advance(Lexer* lexer) {
    lexer->current++;
    lexer->column++;
    return lexer->current[-1];
}

static char peek(Lexer* lexer) {
    return *lexer->current;
}

static char peek_next(Lexer* lexer) {
    if (is_at_end(lexer)) return '\0';
    return lexer->current[1];
}

static bool match(Lexer* lexer, char expected) {
    if (is_at_end(lexer)) return false;
    if (*lexer->current != expected) return false;
    lexer->current++;
    lexer->column++;
    return true;
}

static Token make_token(Lexer* lexer, TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer->start;
    token.length = (int)(lexer->current - lexer->start);
    token.line = lexer->line;
    token.column = lexer->column - token.length;
    return token;
}

static Token error_token(Lexer* lexer, const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer->line;
    token.column = lexer->column;
    return token;
}

static Token synthetic_token(Lexer* lexer, TokenType type) {
    Token token;
    token.type = type;
    token.start = "";
    token.length = 0;
    token.line = lexer->line;
    token.column = lexer->column;
    return token;
}

/* ── Skip whitespace (not newlines, not leading indent) ── */
static void skip_whitespace_inline(Lexer* lexer) {
    for (;;) {
        char c = peek(lexer);
        if (c == ' ' || c == '\t' || c == '\r') {
            advance(lexer);
        } else {
            return;
        }
    }
}

/* ── Skip # comments ── */
static void skip_comment(Lexer* lexer) {
    while (peek(lexer) != '\n' && !is_at_end(lexer)) {
        advance(lexer);
    }
}

/* ══════════════════════════════════════════════════════════════
 *  KEYWORD TABLE
 * ══════════════════════════════════════════════════════════════ */

typedef struct {
    const char* name;
    int length;
    TokenType type;
} Keyword;

static Keyword keywords[] = {
    {"let",       3,  TOKEN_LET},
    {"be",        2,  TOKEN_BE},
    {"const",     5,  TOKEN_CONST},
    {"fn",        2,  TOKEN_FN},
    {"task",      4,  TOKEN_TASK},
    {"pass",      4,  TOKEN_PASS},
    {"if",        2,  TOKEN_IF},
    {"elif",      4,  TOKEN_ELIF},
    {"else",      4,  TOKEN_ELSE},
    {"while",     5,  TOKEN_WHILE},
    {"for",       3,  TOKEN_FOR},
    {"each",      4,  TOKEN_EACH},
    {"repeat",    6,  TOKEN_REPEAT},
    {"times",     5,  TOKEN_TIMES},
    {"in",        2,  TOKEN_IN},
    {"to",        2,  TOKEN_TO},
    {"by",        2,  TOKEN_BY},
    {"break",     5,  TOKEN_BREAK},
    {"skip",      4,  TOKEN_SKIP},
    {"match",     5,  TOKEN_MATCH},
    {"and",       3,  TOKEN_AND},
    {"or",        2,  TOKEN_OR},
    {"not",       3,  TOKEN_NOT},
    {"is",        2,  TOKEN_IS},
    {"true",      4,  TOKEN_TRUE},
    {"false",     5,  TOKEN_FALSE},
    {"nothing",   7,  TOKEN_NOTHING},
    {"use",       3,  TOKEN_USE},
    {"from",      4,  TOKEN_FROM},
    {"struct",    6,  TOKEN_STRUCT},
    {"has",       3,  TOKEN_HAS},
    {"can",       3,  TOKEN_CAN},
    {"self",      4,  TOKEN_SELF},
    {"attempt",   7,  TOKEN_ATTEMPT},
    {"otherwise", 9,  TOKEN_OTHERWISE},
    {"unsafe",    6,  TOKEN_UNSAFE},
    {"extends",   7,  TOKEN_EXTENDS},
    {"display",   7,  TOKEN_DISPLAY},
    {"exists",    6,  TOKEN_EXISTS},
    {NULL,        0,  TOKEN_ERROR}
};

static TokenType check_keyword(const char* start, int length) {
    for (int i = 0; keywords[i].name != NULL; i++) {
        if (keywords[i].length == length &&
            memcmp(keywords[i].name, start, length) == 0) {
            return keywords[i].type;
        }
    }
    return TOKEN_IDENTIFIER;
}

/* ══════════════════════════════════════════════════════════════
 *  SCAN SPECIFIC TOKEN TYPES
 * ══════════════════════════════════════════════════════════════ */

/* ── Scan a string literal ("..." or '...') ── */
static Token scan_string(Lexer* lexer, char quote) {
    while (peek(lexer) != quote && !is_at_end(lexer)) {
        if (peek(lexer) == '\n') {
            lexer->line++;
            lexer->column = 0;
        }
        if (peek(lexer) == '\\') advance(lexer); /* skip escape */
        advance(lexer);
    }

    if (is_at_end(lexer)) {
        return error_token(lexer, "Unterminated string");
    }

    advance(lexer); /* closing quote */
    return make_token(lexer, TOKEN_STRING);
}

/* ── Scan a number literal ── */
static Token scan_number(Lexer* lexer) {
    bool is_float = false;

    /* Check for hex: 0x... */
    if (lexer->current[-1] == '0' && (peek(lexer) == 'x' || peek(lexer) == 'X')) {
        advance(lexer); /* skip 'x' */
        while (isxdigit(peek(lexer)) || peek(lexer) == '_') advance(lexer);
        return make_token(lexer, TOKEN_INT);
    }

    /* Check for binary: 0b... */
    if (lexer->current[-1] == '0' && (peek(lexer) == 'b' || peek(lexer) == 'B')) {
        advance(lexer); /* skip 'b' */
        while (peek(lexer) == '0' || peek(lexer) == '1' || peek(lexer) == '_') advance(lexer);
        return make_token(lexer, TOKEN_INT);
    }

    /* Decimal digits */
    while (isdigit(peek(lexer)) || peek(lexer) == '_') advance(lexer);

    /* Decimal point */
    if (peek(lexer) == '.' && isdigit(peek_next(lexer))) {
        is_float = true;
        advance(lexer); /* consume '.' */
        while (isdigit(peek(lexer)) || peek(lexer) == '_') advance(lexer);
    }

    /* Exponent */
    if (peek(lexer) == 'e' || peek(lexer) == 'E') {
        is_float = true;
        advance(lexer);
        if (peek(lexer) == '+' || peek(lexer) == '-') advance(lexer);
        while (isdigit(peek(lexer))) advance(lexer);
    }

    return make_token(lexer, is_float ? TOKEN_FLOAT : TOKEN_INT);
}

/* ── Scan identifier or keyword ── */
static Token scan_identifier(Lexer* lexer) {
    while (isalnum(peek(lexer)) || peek(lexer) == '_') {
        advance(lexer);
    }

    int length = (int)(lexer->current - lexer->start);
    TokenType type = check_keyword(lexer->start, length);

    /* ── Handle multi-word keywords ── */

    /* "give back" → TOKEN_GIVE_BACK */
    if (type == TOKEN_IDENTIFIER && length == 4 && memcmp(lexer->start, "give", 4) == 0) {
        const char* saved = lexer->current;
        int saved_col = lexer->column;
        /* skip whitespace between "give" and "back" */
        while (peek(lexer) == ' ') advance(lexer);
        if (!is_at_end(lexer) && memcmp(lexer->current, "back", 4) == 0 &&
            !isalnum(lexer->current[4]) && lexer->current[4] != '_') {
            lexer->current += 4;
            lexer->column += 4;
            return make_token(lexer, TOKEN_GIVE_BACK);
        }
        /* not "give back" — backtrack */
        lexer->current = saved;
        lexer->column = saved_col;
    }

    /* Handle "is" compound operators:
       "is not", "is greater than", "is less than",
       "is at least", "is at most" */
    if (type == TOKEN_IS) {
        const char* saved = lexer->current;
        int saved_col = lexer->column;

        while (peek(lexer) == ' ') advance(lexer);

        /* "is not" */
        if (memcmp(lexer->current, "not", 3) == 0 &&
            !isalnum(lexer->current[3]) && lexer->current[3] != '_') {
            lexer->current += 3;
            lexer->column += 3;
            return make_token(lexer, TOKEN_IS_NOT);
        }

        /* "is greater than" */
        if (memcmp(lexer->current, "greater", 7) == 0) {
            lexer->current += 7;
            lexer->column += 7;
            while (peek(lexer) == ' ') advance(lexer);
            if (memcmp(lexer->current, "than", 4) == 0 &&
                !isalnum(lexer->current[4]) && lexer->current[4] != '_') {
                lexer->current += 4;
                lexer->column += 4;
                return make_token(lexer, TOKEN_IS_GREATER);
            }
            /* backtrack */
            lexer->current = saved;
            lexer->column = saved_col;
        }

        /* "is less than" */
        if (memcmp(lexer->current, "less", 4) == 0) {
            lexer->current += 4;
            lexer->column += 4;
            while (peek(lexer) == ' ') advance(lexer);
            if (memcmp(lexer->current, "than", 4) == 0 &&
                !isalnum(lexer->current[4]) && lexer->current[4] != '_') {
                lexer->current += 4;
                lexer->column += 4;
                return make_token(lexer, TOKEN_IS_LESS);
            }
            lexer->current = saved;
            lexer->column = saved_col;
        }

        /* "is at least" */
        if (memcmp(lexer->current, "at", 2) == 0 &&
            (lexer->current[2] == ' ')) {
            const char* at_saved = lexer->current;
            lexer->current += 2;
            lexer->column += 2;
            while (peek(lexer) == ' ') advance(lexer);
            if (memcmp(lexer->current, "least", 5) == 0 &&
                !isalnum(lexer->current[5]) && lexer->current[5] != '_') {
                lexer->current += 5;
                lexer->column += 5;
                return make_token(lexer, TOKEN_IS_AT_LEAST);
            }
            if (memcmp(lexer->current, "most", 4) == 0 &&
                !isalnum(lexer->current[4]) && lexer->current[4] != '_') {
                lexer->current += 4;
                lexer->column += 4;
                return make_token(lexer, TOKEN_IS_AT_MOST);
            }
            lexer->current = saved;
            lexer->column = saved_col;
        }

        /* plain "is" — backtrack */
        lexer->current = saved;
        lexer->column = saved_col;
    }

    /* Handle "not exists" */
    if (type == TOKEN_NOT) {
        const char* saved = lexer->current;
        int saved_col = lexer->column;
        while (peek(lexer) == ' ') advance(lexer);
        if (memcmp(lexer->current, "exists", 6) == 0 &&
            !isalnum(lexer->current[6]) && lexer->current[6] != '_') {
            lexer->current += 6;
            lexer->column += 6;
            return make_token(lexer, TOKEN_NOT_EXISTS);
        }
        lexer->current = saved;
        lexer->column = saved_col;
    }

    return make_token(lexer, type);
}

/* ══════════════════════════════════════════════════════════════
 *  INDENTATION HANDLING
 * ══════════════════════════════════════════════════════════════ */

/* Count leading spaces/tabs at start of current line */
static int count_indent(Lexer* lexer) {
    int spaces = 0;
    const char* p = lexer->current;
    while (*p == ' ' || *p == '\t') {
        if (*p == '\t') {
            spaces += VEXIUM_TAB_WIDTH;
        } else {
            spaces++;
        }
        p++;
    }
    return spaces;
}

/* ══════════════════════════════════════════════════════════════
 *  MAIN LEXER ENTRY POINTS
 * ══════════════════════════════════════════════════════════════ */

void lexer_init(Lexer* lexer, const char* source) {
    lexer->source = source;
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
    lexer->column = 1;
    lexer->indent_stack[0] = 0;
    lexer->indent_depth = 0;
    lexer->pending_dedents = 0;
    lexer->at_line_start = true;
    lexer->emit_newline = false;
}

Token lexer_next_token(Lexer* lexer) {
    /* ── Emit pending DEDENTs first ── */
    if (lexer->pending_dedents > 0) {
        lexer->pending_dedents--;
        return synthetic_token(lexer, TOKEN_DEDENT);
    }

    /* ── Emit pending NEWLINE ── */
    if (lexer->emit_newline) {
        lexer->emit_newline = false;
        return synthetic_token(lexer, TOKEN_NEWLINE);
    }

    /* ── Handle indentation at start of line ── */
    if (lexer->at_line_start) {
        lexer->at_line_start = false;

        /* Skip blank lines and comment-only lines */
        while (!is_at_end(lexer)) {
            const char* line_start = lexer->current;
            int indent = count_indent(lexer);

            /* Move past whitespace */
            while (peek(lexer) == ' ' || peek(lexer) == '\t') {
                advance(lexer);
            }

            /* Blank line or comment-only line — skip entirely */
            if (peek(lexer) == '\n') {
                advance(lexer);
                lexer->line++;
                lexer->column = 1;
                continue;
            }
            if (peek(lexer) == '#') {
                skip_comment(lexer);
                if (peek(lexer) == '\n') {
                    advance(lexer);
                    lexer->line++;
                    lexer->column = 1;
                    continue;
                }
                if (is_at_end(lexer)) break;
                continue;
            }

            /* Real content — process indentation */
            int current_indent = lexer->indent_stack[lexer->indent_depth];

            if (indent > current_indent) {
                /* INDENT */
                lexer->indent_depth++;
                if (lexer->indent_depth >= VEXIUM_MAX_INDENT_DEPTH) {
                    return error_token(lexer, "Too many indentation levels");
                }
                lexer->indent_stack[lexer->indent_depth] = indent;
                return synthetic_token(lexer, TOKEN_INDENT);
            } else if (indent < current_indent) {
                /* One or more DEDENTs */
                int dedents = 0;
                while (lexer->indent_depth > 0 &&
                       lexer->indent_stack[lexer->indent_depth] > indent) {
                    lexer->indent_depth--;
                    dedents++;
                }
                if (lexer->indent_stack[lexer->indent_depth] != indent) {
                    return error_token(lexer, "Inconsistent indentation");
                }
                if (dedents > 1) {
                    lexer->pending_dedents = dedents - 1;
                }
                return synthetic_token(lexer, TOKEN_DEDENT);
            }
            /* indent == current_indent → no change, continue scanning */
            break;
        }
    }

    /* ── Skip inline whitespace ── */
    skip_whitespace_inline(lexer);

    /* ── Skip comments ── */
    if (peek(lexer) == '#') {
        skip_comment(lexer);
    }

    lexer->start = lexer->current;

    /* ── Check for EOF ── */
    if (is_at_end(lexer)) {
        /* Emit remaining DEDENTs at EOF */
        if (lexer->indent_depth > 0) {
            lexer->indent_depth--;
            lexer->pending_dedents = lexer->indent_depth;
            lexer->indent_depth = 0;
            return synthetic_token(lexer, TOKEN_DEDENT);
        }
        return make_token(lexer, TOKEN_EOF);
    }

    char c = advance(lexer);

    /* ── Newlines ── */
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
        lexer->at_line_start = true;
        return make_token(lexer, TOKEN_NEWLINE);
    }

    /* ── Numbers ── */
    if (isdigit(c)) {
        return scan_number(lexer);
    }

    /* ── Identifiers and Keywords ── */
    if (isalpha(c) || c == '_') {
        return scan_identifier(lexer);
    }

    /* ── Strings ── */
    if (c == '"' || c == '\'') {
        return scan_string(lexer, c);
    }

    /* ── Two-character operators ── */
    switch (c) {
        case '+':
            if (match(lexer, '=')) return make_token(lexer, TOKEN_PLUS_ASSIGN);
            return make_token(lexer, TOKEN_PLUS);
        case '-':
            if (match(lexer, '>')) return make_token(lexer, TOKEN_ARROW);
            if (match(lexer, '=')) return make_token(lexer, TOKEN_MINUS_ASSIGN);
            return make_token(lexer, TOKEN_MINUS);
        case '*':
            if (match(lexer, '*')) return make_token(lexer, TOKEN_POWER);
            if (match(lexer, '=')) return make_token(lexer, TOKEN_STAR_ASSIGN);
            return make_token(lexer, TOKEN_STAR);
        case '/':
            if (match(lexer, '=')) return make_token(lexer, TOKEN_SLASH_ASSIGN);
            return make_token(lexer, TOKEN_SLASH);
        case '%':
            return make_token(lexer, TOKEN_PERCENT);
        case '=':
            if (match(lexer, '=')) return make_token(lexer, TOKEN_EQ);
            if (match(lexer, '>')) return make_token(lexer, TOKEN_FAT_ARROW);
            return make_token(lexer, TOKEN_ASSIGN);
        case '!':
            if (match(lexer, '=')) return make_token(lexer, TOKEN_NEQ);
            return error_token(lexer, "Unexpected character '!'");
        case '<':
            if (match(lexer, '=')) return make_token(lexer, TOKEN_LTE);
            return make_token(lexer, TOKEN_LT);
        case '>':
            if (match(lexer, '=')) return make_token(lexer, TOKEN_GTE);
            return make_token(lexer, TOKEN_GT);

        /* Delimiters */
        case '(':  return make_token(lexer, TOKEN_LPAREN);
        case ')':  return make_token(lexer, TOKEN_RPAREN);
        case '[':  return make_token(lexer, TOKEN_LBRACKET);
        case ']':  return make_token(lexer, TOKEN_RBRACKET);
        case '{':  return make_token(lexer, TOKEN_LBRACE);
        case '}':  return make_token(lexer, TOKEN_RBRACE);
        case ',':  return make_token(lexer, TOKEN_COMMA);
        case '.':  return make_token(lexer, TOKEN_DOT);
        case ':':  return make_token(lexer, TOKEN_COLON);
        case '@':  return make_token(lexer, TOKEN_AT);
    }

    return error_token(lexer, "Unexpected character");
}
