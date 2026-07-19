#include "token.h"

/* ── Return human-readable name for a token type ── */
const char* token_type_name(TokenType type) {
    switch (type) {

        case TOKEN_INT:          return "INT";
        case TOKEN_FLOAT:        return "FLOAT";
        case TOKEN_STRING:       return "STRING";
        case TOKEN_IDENTIFIER:   return "IDENTIFIER";

        case TOKEN_LET:          return "LET";
        case TOKEN_BE:           return "BE";
        case TOKEN_CONST:        return "CONST";

        case TOKEN_FN:           return "FN";
        case TOKEN_GIVE_BACK:    return "GIVE_BACK";
        case TOKEN_TASK:         return "TASK";
        case TOKEN_PASS:         return "PASS";

        case TOKEN_IF:           return "IF";
        case TOKEN_ELIF:         return "ELIF";
        case TOKEN_ELSE:         return "ELSE";
        case TOKEN_WHILE:        return "WHILE";
        case TOKEN_FOR:          return "FOR";
        case TOKEN_EACH:         return "EACH";
        case TOKEN_REPEAT:       return "REPEAT";
        case TOKEN_TIMES:        return "TIMES";
        case TOKEN_IN:           return "IN";
        case TOKEN_TO:           return "TO";
        case TOKEN_BY:           return "BY";
        case TOKEN_BREAK:        return "BREAK";
        case TOKEN_SKIP:         return "SKIP";
        case TOKEN_MATCH:        return "MATCH";

        case TOKEN_AND:          return "AND";
        case TOKEN_OR:           return "OR";
        case TOKEN_NOT:          return "NOT";
        case TOKEN_IS:           return "IS";
        case TOKEN_TRUE:         return "TRUE";
        case TOKEN_FALSE:        return "FALSE";
        case TOKEN_NOTHING:      return "NOTHING";

        case TOKEN_USE:          return "USE";
        case TOKEN_FROM:         return "FROM";
        case TOKEN_STRUCT:       return "STRUCT";
        case TOKEN_HAS:          return "HAS";
        case TOKEN_CAN:          return "CAN";
        case TOKEN_SELF:         return "SELF";
        case TOKEN_ATTEMPT:      return "ATTEMPT";
        case TOKEN_OTHERWISE:    return "OTHERWISE";
        case TOKEN_UNSAFE:       return "UNSAFE";

        case TOKEN_DISPLAY:      return "DISPLAY";

        case TOKEN_IS_NOT:       return "IS_NOT";
        case TOKEN_IS_GREATER:   return "IS_GREATER";
        case TOKEN_IS_LESS:      return "IS_LESS";
        case TOKEN_IS_AT_LEAST:  return "IS_AT_LEAST";
        case TOKEN_IS_AT_MOST:   return "IS_AT_MOST";
        case TOKEN_NOT_EXISTS:   return "NOT_EXISTS";
        case TOKEN_EXISTS:       return "EXISTS";

        case TOKEN_PLUS:         return "PLUS";
        case TOKEN_MINUS:        return "MINUS";
        case TOKEN_STAR:         return "STAR";
        case TOKEN_SLASH:        return "SLASH";
        case TOKEN_PERCENT:      return "PERCENT";
        case TOKEN_POWER:        return "POWER";

        case TOKEN_EQ:           return "EQ";
        case TOKEN_NEQ:          return "NEQ";
        case TOKEN_LT:           return "LT";
        case TOKEN_GT:           return "GT";
        case TOKEN_LTE:          return "LTE";
        case TOKEN_GTE:          return "GTE";

        case TOKEN_ASSIGN:       return "ASSIGN";
        case TOKEN_PLUS_ASSIGN:  return "PLUS_ASSIGN";
        case TOKEN_MINUS_ASSIGN: return "MINUS_ASSIGN";
        case TOKEN_STAR_ASSIGN:  return "STAR_ASSIGN";
        case TOKEN_SLASH_ASSIGN: return "SLASH_ASSIGN";

        case TOKEN_LPAREN:       return "LPAREN";
        case TOKEN_RPAREN:       return "RPAREN";
        case TOKEN_LBRACKET:     return "LBRACKET";
        case TOKEN_RBRACKET:     return "RBRACKET";
        case TOKEN_LBRACE:       return "LBRACE";
        case TOKEN_RBRACE:       return "RBRACE";
        case TOKEN_COMMA:        return "COMMA";
        case TOKEN_DOT:          return "DOT";
        case TOKEN_COLON:        return "COLON";
        case TOKEN_ARROW:        return "ARROW";
        case TOKEN_FAT_ARROW:    return "FAT_ARROW";
        case TOKEN_AT:           return "AT";

        case TOKEN_NEWLINE:      return "NEWLINE";
        case TOKEN_INDENT:       return "INDENT";
        case TOKEN_DEDENT:       return "DEDENT";

        case TOKEN_EOF:          return "EOF";
        case TOKEN_ERROR:        return "ERROR";
    }
    return "UNKNOWN";
}

/* ── Print a token for debugging ── */
void token_print(Token token) {
    printf("  [%3d:%-3d] %-15s '",
        token.line, token.column, token_type_name(token.type));

    if (token.type == TOKEN_NEWLINE) {
        printf("\\n");
    } else if (token.type == TOKEN_INDENT) {
        printf("→");
    } else if (token.type == TOKEN_DEDENT) {
        printf("←");
    } else if (token.type == TOKEN_EOF) {
        printf("EOF");
    } else if (token.type == TOKEN_ERROR) {
        printf("%.*s", token.length, token.start);
    } else {
        printf("%.*s", token.length, token.start);
    }

    printf("'\n");
}
