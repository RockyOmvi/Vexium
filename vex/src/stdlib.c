#ifdef _WIN32
/* Must include winsock2 BEFORE windows.h to avoid winsock1 collision */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
/* Must rename Windows' TokenType before including our headers */
#define TokenType WindowsTokenType
#include <windows.h>
#undef TokenType
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#endif
#include "stdlib.h"
#include "interpreter.h"
#include <math.h>
#include <time.h>

typedef struct {
    const char* current;
    bool had_error;
} StdJsonParser;

typedef struct {
    char* buffer;
    size_t length;
    size_t capacity;
} StdJsonBuilder;

static void std_json_set_error(StdJsonParser* parser) {
    parser->had_error = true;
}

static void std_json_skip_whitespace(StdJsonParser* parser) {
    while (*parser->current != '\0' && isspace((unsigned char)*parser->current)) {
        parser->current++;
    }
}

static char std_json_peek(StdJsonParser* parser) {
    return *parser->current;
}

static char std_json_peek_next(StdJsonParser* parser) {
    if (*parser->current == '\0') return '\0';
    return parser->current[1];
}

static char std_json_advance(StdJsonParser* parser) {
    return *parser->current++;
}

static bool std_json_match(StdJsonParser* parser, char expected) {
    if (*parser->current != expected) return false;
    parser->current++;
    return true;
}

static bool std_json_is_delimiter(char c) {
    return c == '\0' || c == ',' || c == ']' || c == '}' || isspace((unsigned char)c);
}

static VexValue std_json_parse_value(StdJsonParser* parser);

static VexValue std_json_parse_string(StdJsonParser* parser) {
    std_json_advance(parser);

    const char* start = parser->current;
    size_t len = 0;
    while (std_json_peek(parser) != '"' && std_json_peek(parser) != '\0') {
        if (std_json_peek(parser) == '\\') {
            std_json_advance(parser);
            if (std_json_peek(parser) == '\0') {
                std_json_set_error(parser);
                return vex_nothing();
            }
            char escape = std_json_advance(parser);
            if (escape == 'u') {
                for (int i = 0; i < 4; i++) {
                    if (!isxdigit((unsigned char)std_json_peek(parser))) {
                        std_json_set_error(parser);
                        return vex_nothing();
                    }
                    std_json_advance(parser);
                }
                len += 6;
                continue;
            }
        } else {
            std_json_advance(parser);
        }
        len++;
    }

    if (std_json_peek(parser) != '"') {
        std_json_set_error(parser);
        return vex_nothing();
    }

    char* result = (char*)malloc(len + 1);
    if (!result) {
        std_json_set_error(parser);
        return vex_nothing();
    }

    parser->current = start;
    size_t out = 0;
    while (std_json_peek(parser) != '"' && std_json_peek(parser) != '\0') {
        if (std_json_peek(parser) == '\\') {
            std_json_advance(parser);
            if (std_json_peek(parser) == '\0') {
                std_json_set_error(parser);
                free(result);
                return vex_nothing();
            }
            char escape = std_json_advance(parser);
            switch (escape) {
                case '"': result[out++] = '"'; break;
                case '\\': result[out++] = '\\'; break;
                case '/': result[out++] = '/'; break;
                case 'b': result[out++] = '\b'; break;
                case 'f': result[out++] = '\f'; break;
                case 'n': result[out++] = '\n'; break;
                case 'r': result[out++] = '\r'; break;
                case 't': result[out++] = '\t'; break;
                case 'u':
                    result[out++] = '\\';
                    result[out++] = 'u';
                    for (int i = 0; i < 4; i++) {
                        if (!isxdigit((unsigned char)std_json_peek(parser))) {
                            std_json_set_error(parser);
                            free(result);
                            return vex_nothing();
                        }
                        result[out++] = std_json_advance(parser);
                    }
                    break;
                default:
                    result[out++] = escape;
                    break;
            }
        } else {
            result[out++] = std_json_advance(parser);
        }
    }

    if (std_json_peek(parser) != '"') {
        std_json_set_error(parser);
        free(result);
        return vex_nothing();
    }

    result[out] = '\0';
    std_json_advance(parser);
    VexValue value = vex_string(result, (int)out);
    free(result);
    return value;
}

static VexValue std_json_parse_number(StdJsonParser* parser) {
    const char* start = parser->current;
    if (std_json_peek(parser) == '-') std_json_advance(parser);
    if (!isdigit((unsigned char)std_json_peek(parser))) {
        std_json_set_error(parser);
        return vex_nothing();
    }
    while (isdigit((unsigned char)std_json_peek(parser))) std_json_advance(parser);
    if (std_json_peek(parser) == '.') {
        if (!isdigit((unsigned char)std_json_peek_next(parser))) {
            std_json_set_error(parser);
            return vex_nothing();
        }
        std_json_advance(parser);
        while (isdigit((unsigned char)std_json_peek(parser))) std_json_advance(parser);
    }
    if (std_json_peek(parser) == 'e' || std_json_peek(parser) == 'E') {
        std_json_advance(parser);
        if (std_json_peek(parser) == '+' || std_json_peek(parser) == '-') std_json_advance(parser);
        if (!isdigit((unsigned char)std_json_peek(parser))) {
            std_json_set_error(parser);
            return vex_nothing();
        }
        while (isdigit((unsigned char)std_json_peek(parser))) std_json_advance(parser);
    }
    if (!std_json_is_delimiter(std_json_peek(parser))) {
        std_json_set_error(parser);
        return vex_nothing();
    }

    size_t len = (size_t)(parser->current - start);
    char* num = (char*)malloc(len + 1);
    if (!num) {
        std_json_set_error(parser);
        return vex_nothing();
    }
    memcpy(num, start, len);
    num[len] = '\0';

    char* end = NULL;
    long long int_val = strtoll(num, &end, 10);
    if (*end == '\0') {
        free(num);
        return vex_int((int64_t)int_val);
    }

    double float_val = strtod(num, &end);
    if (*end != '\0') {
        free(num);
        std_json_set_error(parser);
        return vex_nothing();
    }
    free(num);
    return vex_float(float_val);
}

static void std_json_array_push(ValueArray* arr, VexValue value) {
    if (arr->count >= arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
        arr->items = (VexValue*)realloc(arr->items, sizeof(VexValue) * arr->capacity);
    }
    arr->items[arr->count++] = value;
}

static void std_json_map_set(ValueMap* map, const char* key, VexValue value) {
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->entries[i].key, key) == 0) {
            map->entries[i].value = value;
            return;
        }
    }
    if (map->count >= map->capacity) {
        map->capacity = map->capacity == 0 ? 8 : map->capacity * 2;
        map->entries = (ValueMapEntry*)realloc(map->entries, sizeof(ValueMapEntry) * map->capacity);
    }
    ValueMapEntry* entry = &map->entries[map->count++];
    entry->key = safe_strdup(key);
    entry->value = value;
}

static VexValue std_json_parse_array(StdJsonParser* parser) {
    std_json_advance(parser);
    std_json_skip_whitespace(parser);

    VexValue result;
    result.type = VAL_ARRAY;
    result.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));

    if (std_json_match(parser, ']')) {
        return result;
    }

    while (true) {
        std_json_skip_whitespace(parser);
        VexValue elem = std_json_parse_value(parser);
        if (parser->had_error) return vex_nothing();
        std_json_array_push(result.as.array_val, elem);

        std_json_skip_whitespace(parser);
        if (std_json_match(parser, ',')) {
            std_json_skip_whitespace(parser);
            if (std_json_peek(parser) == ']') {
                std_json_set_error(parser);
                return vex_nothing();
            }
            continue;
        }
        if (std_json_match(parser, ']')) break;
        std_json_set_error(parser);
        return vex_nothing();
    }

    return result;
}

static VexValue std_json_parse_object(StdJsonParser* parser) {
    std_json_advance(parser);
    std_json_skip_whitespace(parser);

    VexValue result;
    result.type = VAL_MAP;
    result.as.map_val = (ValueMap*)calloc(1, sizeof(ValueMap));

    if (std_json_match(parser, '}')) {
        return result;
    }

    while (true) {
        std_json_skip_whitespace(parser);
        if (std_json_peek(parser) != '"') {
            std_json_set_error(parser);
            return vex_nothing();
        }

        VexValue key = std_json_parse_string(parser);
        if (parser->had_error || key.type != VAL_STRING) return vex_nothing();

        std_json_skip_whitespace(parser);
        if (!std_json_match(parser, ':')) {
            std_json_set_error(parser);
            return vex_nothing();
        }

        std_json_skip_whitespace(parser);
        VexValue value = std_json_parse_value(parser);
        if (parser->had_error) return vex_nothing();
        std_json_map_set(result.as.map_val, key.as.string_val.data, value);

        std_json_skip_whitespace(parser);
        if (std_json_match(parser, ',')) {
            std_json_skip_whitespace(parser);
            if (std_json_peek(parser) == '}') {
                std_json_set_error(parser);
                return vex_nothing();
            }
            continue;
        }
        if (std_json_match(parser, '}')) break;
        std_json_set_error(parser);
        return vex_nothing();
    }

    return result;
}

static VexValue std_json_parse_value(StdJsonParser* parser) {
    std_json_skip_whitespace(parser);
    char c = std_json_peek(parser);
    switch (c) {
        case '"': return std_json_parse_string(parser);
        case '[': return std_json_parse_array(parser);
        case '{': return std_json_parse_object(parser);
        case 't':
            if (strncmp(parser->current, "true", 4) == 0 && std_json_is_delimiter(parser->current[4])) {
                parser->current += 4;
                return vex_bool(true);
            }
            break;
        case 'f':
            if (strncmp(parser->current, "false", 5) == 0 && std_json_is_delimiter(parser->current[5])) {
                parser->current += 5;
                return vex_bool(false);
            }
            break;
        case 'n':
            if (strncmp(parser->current, "null", 4) == 0 && std_json_is_delimiter(parser->current[4])) {
                parser->current += 4;
                return vex_nothing();
            }
            break;
        default:
            if (c == '-' || isdigit((unsigned char)c)) {
                return std_json_parse_number(parser);
            }
            break;
    }
    std_json_set_error(parser);
    return vex_nothing();
}

static void std_json_builder_init(StdJsonBuilder* builder) {
    builder->capacity = 256;
    builder->length = 0;
    builder->buffer = (char*)malloc(builder->capacity);
    builder->buffer[0] = '\0';
}

static void std_json_builder_ensure(StdJsonBuilder* builder, size_t needed) {
    while (builder->length + needed >= builder->capacity) {
        builder->capacity *= 2;
    }
    builder->buffer = (char*)realloc(builder->buffer, builder->capacity);
}

static void std_json_builder_append(StdJsonBuilder* builder, const char* str) {
    size_t len = strlen(str);
    std_json_builder_ensure(builder, len + 1);
    memcpy(builder->buffer + builder->length, str, len + 1);
    builder->length += len;
}

static void std_json_builder_append_char(StdJsonBuilder* builder, char c) {
    std_json_builder_ensure(builder, 2);
    builder->buffer[builder->length++] = c;
    builder->buffer[builder->length] = '\0';
}

static void std_json_stringify_value(StdJsonBuilder* builder, VexValue value);

static void std_json_stringify_string(StdJsonBuilder* builder, const char* str, int length) {
    std_json_builder_append_char(builder, '"');
    for (int i = 0; i < length; i++) {
        unsigned char c = (unsigned char)str[i];
        switch (c) {
            case '"': std_json_builder_append(builder, "\\\""); break;
            case '\\': std_json_builder_append(builder, "\\\\"); break;
            case '\b': std_json_builder_append(builder, "\\b"); break;
            case '\f': std_json_builder_append(builder, "\\f"); break;
            case '\n': std_json_builder_append(builder, "\\n"); break;
            case '\r': std_json_builder_append(builder, "\\r"); break;
            case '\t': std_json_builder_append(builder, "\\t"); break;
            default:
                if (c >= 0x20) {
                    std_json_builder_append_char(builder, (char)c);
                } else {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    std_json_builder_append(builder, buf);
                }
        }
    }
    std_json_builder_append_char(builder, '"');
}

static void std_json_stringify_array(StdJsonBuilder* builder, ValueArray* arr) {
    std_json_builder_append_char(builder, '[');
    for (int i = 0; i < arr->count; i++) {
        if (i > 0) std_json_builder_append_char(builder, ',');
        std_json_stringify_value(builder, arr->items[i]);
    }
    std_json_builder_append_char(builder, ']');
}

static void std_json_stringify_map(StdJsonBuilder* builder, ValueMap* map) {
    std_json_builder_append_char(builder, '{');
    for (int i = 0; i < map->count; i++) {
        if (i > 0) std_json_builder_append_char(builder, ',');
        std_json_stringify_string(builder, map->entries[i].key, (int)strlen(map->entries[i].key));
        std_json_builder_append_char(builder, ':');
        std_json_stringify_value(builder, map->entries[i].value);
    }
    std_json_builder_append_char(builder, '}');
}

static void std_json_stringify_value(StdJsonBuilder* builder, VexValue value) {
    char buf[64];
    switch (value.type) {
        case VAL_NOTHING:
            std_json_builder_append(builder, "null");
            break;
        case VAL_BOOL:
            std_json_builder_append(builder, value.as.bool_val ? "true" : "false");
            break;
        case VAL_INT:
            snprintf(buf, sizeof(buf), "%lld", (long long)value.as.int_val);
            std_json_builder_append(builder, buf);
            break;
        case VAL_FLOAT:
            snprintf(buf, sizeof(buf), "%.17g", value.as.float_val);
            std_json_builder_append(builder, buf);
            break;
        case VAL_STRING:
            std_json_stringify_string(builder, value.as.string_val.data, value.as.string_val.length);
            break;
        case VAL_ARRAY:
            std_json_stringify_array(builder, value.as.array_val);
            break;
        case VAL_MAP:
            std_json_stringify_map(builder, value.as.map_val);
            break;
        default:
            std_json_builder_append(builder, "null");
            break;
    }
}

static VexValue json_parse_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) {
        fprintf(stderr, "Error: parse() expects 1 string argument\n");
        return vex_nothing();
    }

    StdJsonParser parser;
    parser.current = args[0].as.string_val.data;
    parser.had_error = false;

    VexValue value = std_json_parse_value(&parser);
    std_json_skip_whitespace(&parser);
    if (parser.had_error || *parser.current != '\0') {
        return vex_nothing();
    }
    return value;
}

static VexValue json_stringify_builtin(VexValue* args, int argc) {
    if (argc != 1) {
        fprintf(stderr, "Error: stringify() expects 1 argument\n");
        return vex_nothing();
    }

    StdJsonBuilder builder;
    std_json_builder_init(&builder);
    std_json_stringify_value(&builder, args[0]);
    VexValue result = vex_string(builder.buffer, (int)builder.length);
    free(builder.buffer);
    return result;
}

/* ══════════════════════════════════════════════════════════════
 *  MATH MODULE
 *
 *  sqrt, abs, pow, sin, cos, tan, floor, ceil, round, log
 *  min, max, random, PI, E
 * ══════════════════════════════════════════════════════════════ */

static VexValue math_sqrt(VexValue* args, int argc) {
    if (argc != 1) { fprintf(stderr, "Error: sqrt() expects 1 argument\n"); return vex_nothing(); }
    return vex_float(sqrt(args[0].type == VAL_INT ? (double)args[0].as.int_val : args[0].as.float_val));
}

static VexValue math_abs(VexValue* args, int argc) {
    if (argc != 1) { fprintf(stderr, "Error: abs() expects 1 argument\n"); return vex_nothing(); }
    if (args[0].type == VAL_INT) return vex_int(args[0].as.int_val < 0 ? -args[0].as.int_val : args[0].as.int_val);
    if (args[0].type == VAL_FLOAT) return vex_float(fabs(args[0].as.float_val));
    return vex_nothing();
}

static VexValue math_pow(VexValue* args, int argc) {
    if (argc != 2) { fprintf(stderr, "Error: pow() expects 2 arguments\n"); return vex_nothing(); }
    double base = args[0].type == VAL_INT ? (double)args[0].as.int_val : args[0].as.float_val;
    double exp = args[1].type == VAL_INT ? (double)args[1].as.int_val : args[1].as.float_val;
    return vex_float(pow(base, exp));
}

static VexValue math_sin(VexValue* args, int argc) {
    if (argc != 1) { fprintf(stderr, "Error: sin() expects 1 argument\n"); return vex_nothing(); }
    double v = args[0].type == VAL_INT ? (double)args[0].as.int_val : args[0].as.float_val;
    return vex_float(sin(v));
}

static VexValue math_cos(VexValue* args, int argc) {
    if (argc != 1) { fprintf(stderr, "Error: cos() expects 1 argument\n"); return vex_nothing(); }
    double v = args[0].type == VAL_INT ? (double)args[0].as.int_val : args[0].as.float_val;
    return vex_float(cos(v));
}

static VexValue math_tan(VexValue* args, int argc) {
    if (argc != 1) { fprintf(stderr, "Error: tan() expects 1 argument\n"); return vex_nothing(); }
    double v = args[0].type == VAL_INT ? (double)args[0].as.int_val : args[0].as.float_val;
    return vex_float(tan(v));
}

static VexValue math_floor(VexValue* args, int argc) {
    if (argc != 1) { fprintf(stderr, "Error: floor() expects 1 argument\n"); return vex_nothing(); }
    double v = args[0].type == VAL_INT ? (double)args[0].as.int_val : args[0].as.float_val;
    return vex_int((int64_t)floor(v));
}

static VexValue math_ceil(VexValue* args, int argc) {
    if (argc != 1) { fprintf(stderr, "Error: ceil() expects 1 argument\n"); return vex_nothing(); }
    double v = args[0].type == VAL_INT ? (double)args[0].as.int_val : args[0].as.float_val;
    return vex_int((int64_t)ceil(v));
}

static VexValue math_round(VexValue* args, int argc) {
    if (argc != 1) { fprintf(stderr, "Error: round() expects 1 argument\n"); return vex_nothing(); }
    double v = args[0].type == VAL_INT ? (double)args[0].as.int_val : args[0].as.float_val;
    return vex_int((int64_t)round(v));
}

static VexValue math_log(VexValue* args, int argc) {
    if (argc != 1) { fprintf(stderr, "Error: log() expects 1 argument\n"); return vex_nothing(); }
    double v = args[0].type == VAL_INT ? (double)args[0].as.int_val : args[0].as.float_val;
    return vex_float(log(v));
}

static VexValue math_min(VexValue* args, int argc) {
    if (argc != 2) { fprintf(stderr, "Error: min() expects 2 arguments\n"); return vex_nothing(); }
    double a = args[0].type == VAL_INT ? (double)args[0].as.int_val : args[0].as.float_val;
    double b = args[1].type == VAL_INT ? (double)args[1].as.int_val : args[1].as.float_val;
    if (args[0].type == VAL_INT && args[1].type == VAL_INT)
        return vex_int(args[0].as.int_val < args[1].as.int_val ? args[0].as.int_val : args[1].as.int_val);
    return vex_float(a < b ? a : b);
}

static VexValue math_max(VexValue* args, int argc) {
    if (argc != 2) { fprintf(stderr, "Error: max() expects 2 arguments\n"); return vex_nothing(); }
    double a = args[0].type == VAL_INT ? (double)args[0].as.int_val : args[0].as.float_val;
    double b = args[1].type == VAL_INT ? (double)args[1].as.int_val : args[1].as.float_val;
    if (args[0].type == VAL_INT && args[1].type == VAL_INT)
        return vex_int(args[0].as.int_val > args[1].as.int_val ? args[0].as.int_val : args[1].as.int_val);
    return vex_float(a > b ? a : b);
}

static VexValue math_random(VexValue* args, int argc) {
    UNUSED(args);
    UNUSED(argc);
    return vex_float((double)rand() / (double)RAND_MAX);
}

static VexValue math_randint(VexValue* args, int argc) {
    if (argc != 2) { fprintf(stderr, "Error: randint() expects 2 arguments (min, max)\n"); return vex_nothing(); }
    int64_t lo = args[0].as.int_val;
    int64_t hi = args[1].as.int_val;
    return vex_int(lo + rand() % (hi - lo + 1));
}

/* ══════════════════════════════════════════════════════════════
 *  STRING MODULE
 *
 *  upper, lower, trim, split, join, replace, contains,
 *  starts_with, ends_with, find, slice, reverse, char_at
 * ══════════════════════════════════════════════════════════════ */

static VexValue str_upper(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) {
        fprintf(stderr, "Error: upper() expects 1 string argument\n"); return vex_nothing();
    }
    int len = args[0].as.string_val.length;
    char* buf = (char*)malloc(len + 1);
    for (int i = 0; i < len; i++) buf[i] = (char)toupper((unsigned char)args[0].as.string_val.data[i]);
    buf[len] = '\0';
    VexValue r = vex_string(buf, len);
    free(buf);
    return r;
}

static VexValue str_lower(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) {
        fprintf(stderr, "Error: lower() expects 1 string argument\n"); return vex_nothing();
    }
    int len = args[0].as.string_val.length;
    char* buf = (char*)malloc(len + 1);
    for (int i = 0; i < len; i++) buf[i] = (char)tolower((unsigned char)args[0].as.string_val.data[i]);
    buf[len] = '\0';
    VexValue r = vex_string(buf, len);
    free(buf);
    return r;
}

static VexValue str_trim(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) {
        fprintf(stderr, "Error: trim() expects 1 string argument\n"); return vex_nothing();
    }
    const char* s = args[0].as.string_val.data;
    int len = args[0].as.string_val.length;
    int start = 0, end = len - 1;
    while (start < len && isspace((unsigned char)s[start])) start++;
    while (end > start && isspace((unsigned char)s[end])) end--;
    int new_len = end - start + 1;
    return vex_string(s + start, new_len);
}

static VexValue str_contains(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        fprintf(stderr, "Error: contains() expects 2 string arguments\n"); return vex_nothing();
    }
    return vex_bool(strstr(args[0].as.string_val.data, args[1].as.string_val.data) != NULL);
}

static VexValue str_starts_with(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        fprintf(stderr, "Error: starts_with() expects 2 string arguments\n"); return vex_nothing();
    }
    int plen = args[1].as.string_val.length;
    if (args[0].as.string_val.length < plen) return vex_bool(false);
    return vex_bool(memcmp(args[0].as.string_val.data, args[1].as.string_val.data, plen) == 0);
}

static VexValue str_ends_with(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        fprintf(stderr, "Error: ends_with() expects 2 string arguments\n"); return vex_nothing();
    }
    int slen = args[0].as.string_val.length;
    int plen = args[1].as.string_val.length;
    if (slen < plen) return vex_bool(false);
    return vex_bool(memcmp(args[0].as.string_val.data + slen - plen, args[1].as.string_val.data, plen) == 0);
}

static VexValue str_find(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        fprintf(stderr, "Error: find() expects 2 string arguments\n"); return vex_nothing();
    }
    const char* found = strstr(args[0].as.string_val.data, args[1].as.string_val.data);
    if (found) return vex_int((int64_t)(found - args[0].as.string_val.data));
    return vex_int(-1);
}

static VexValue str_replace(VexValue* args, int argc) {
    if (argc != 3 || args[0].type != VAL_STRING ||
        args[1].type != VAL_STRING || args[2].type != VAL_STRING) {
        fprintf(stderr, "Error: replace() expects 3 string arguments\n"); return vex_nothing();
    }
    const char* src = args[0].as.string_val.data;
    const char* old_s = args[1].as.string_val.data;
    const char* new_s = args[2].as.string_val.data;
    int old_len = args[1].as.string_val.length;
    int new_len = args[2].as.string_val.length;
    if (old_len == 0) return args[0];

    /* Count occurrences */
    int count = 0;
    const char* p = src;
    while ((p = strstr(p, old_s)) != NULL) { count++; p += old_len; }

    int result_len = args[0].as.string_val.length + count * (new_len - old_len);
    char* buf = (char*)malloc(result_len + 1);
    char* out = buf;
    p = src;
    while (*p) {
        if (strncmp(p, old_s, old_len) == 0) {
            memcpy(out, new_s, new_len);
            out += new_len;
            p += old_len;
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';
    VexValue r = vex_string(buf, result_len);
    free(buf);
    return r;
}

static VexValue str_split(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        fprintf(stderr, "Error: split() expects 2 string arguments\n"); return vex_nothing();
    }
    VexValue result;
    result.type = VAL_ARRAY;
    result.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));

    const char* src = args[0].as.string_val.data;
    const char* delim = args[1].as.string_val.data;
    int dlen = args[1].as.string_val.length;

    if (dlen == 0) {
        /* Split each character */
        for (int i = 0; i < args[0].as.string_val.length; i++) {
            ValueArray* arr = result.as.array_val;
            if (arr->count >= arr->capacity) {
                arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
                arr->items = (VexValue*)realloc(arr->items, sizeof(VexValue) * arr->capacity);
            }
            arr->items[arr->count++] = vex_string(&src[i], 1);
        }
        return result;
    }

    const char* start = src;
    while (*start) {
        const char* found = strstr(start, delim);
        int part_len = found ? (int)(found - start) : (int)strlen(start);

        ValueArray* arr = result.as.array_val;
        if (arr->count >= arr->capacity) {
            arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
            arr->items = (VexValue*)realloc(arr->items, sizeof(VexValue) * arr->capacity);
        }
        arr->items[arr->count++] = vex_string(start, part_len);

        if (!found) break;
        start = found + dlen;
    }
    return result;
}

static VexValue str_join(VexValue* args, int argc) {
    if (argc != 2 || args[1].type != VAL_ARRAY) {
        fprintf(stderr, "Error: join() expects (separator, array)\n"); return vex_nothing();
    }
    const char* sep = args[0].type == VAL_STRING ? args[0].as.string_val.data : "";
    int sep_len = args[0].type == VAL_STRING ? args[0].as.string_val.length : 0;
    ValueArray* arr = args[1].as.array_val;

    /* Calculate total size */
    int total = 0;
    for (int i = 0; i < arr->count; i++) {
        if (arr->items[i].type == VAL_STRING) total += arr->items[i].as.string_val.length;
        else { char* s = vex_value_to_string(arr->items[i]); total += (int)strlen(s); free(s); }
        if (i < arr->count - 1) total += sep_len;
    }

    char* buf = (char*)malloc(total + 1);
    int pos = 0;
    for (int i = 0; i < arr->count; i++) {
        if (arr->items[i].type == VAL_STRING) {
            memcpy(buf + pos, arr->items[i].as.string_val.data, arr->items[i].as.string_val.length);
            pos += arr->items[i].as.string_val.length;
        } else {
            char* s = vex_value_to_string(arr->items[i]);
            int slen = (int)strlen(s);
            memcpy(buf + pos, s, slen);
            pos += slen;
            free(s);
        }
        if (i < arr->count - 1) {
            memcpy(buf + pos, sep, sep_len);
            pos += sep_len;
        }
    }
    buf[pos] = '\0';
    VexValue r = vex_string(buf, pos);
    free(buf);
    return r;
}

static VexValue str_reverse(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) {
        fprintf(stderr, "Error: reverse() expects 1 string argument\n"); return vex_nothing();
    }
    int len = args[0].as.string_val.length;
    char* buf = (char*)malloc(len + 1);
    for (int i = 0; i < len; i++) buf[i] = args[0].as.string_val.data[len - 1 - i];
    buf[len] = '\0';
    VexValue r = vex_string(buf, len);
    free(buf);
    return r;
}

static VexValue str_char_at(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_INT) {
        fprintf(stderr, "Error: char_at() expects (string, int)\n"); return vex_nothing();
    }
    int idx = (int)args[1].as.int_val;
    int len = args[0].as.string_val.length;
    if (idx < 0) idx += len;
    if (idx < 0 || idx >= len) { fprintf(stderr, "Error: char_at() index out of bounds\n"); return vex_nothing(); }
    return vex_string(&args[0].as.string_val.data[idx], 1);
}

/* ══════════════════════════════════════════════════════════════
 *  FILE MODULE
 *
 *  read_file, write_file, append_file, file_exists
 * ══════════════════════════════════════════════════════════════ */

static VexValue file_read(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) {
        fprintf(stderr, "Error: read_file() expects 1 string argument\n"); return vex_nothing();
    }
    FILE* f = fopen(args[0].as.string_val.data, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot read file '%s'\n", args[0].as.string_val.data);
        return vex_nothing();
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char* buf = (char*)malloc(size + 1);
    size_t read = fread(buf, 1, size, f);
    buf[read] = '\0';
    fclose(f);
    VexValue r = vex_string(buf, (int)read);
    free(buf);
    return r;
}

static VexValue file_write(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        fprintf(stderr, "Error: write_file() expects (path, content)\n"); return vex_nothing();
    }
    FILE* f = fopen(args[0].as.string_val.data, "wb");
    if (!f) {
        fprintf(stderr, "Error: Cannot write to file '%s'\n", args[0].as.string_val.data);
        return vex_bool(false);
    }
    fwrite(args[1].as.string_val.data, 1, args[1].as.string_val.length, f);
    fclose(f);
    return vex_bool(true);
}

static VexValue file_append(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        fprintf(stderr, "Error: append_file() expects (path, content)\n"); return vex_nothing();
    }
    FILE* f = fopen(args[0].as.string_val.data, "ab");
    if (!f) {
        fprintf(stderr, "Error: Cannot append to file '%s'\n", args[0].as.string_val.data);
        return vex_bool(false);
    }
    fwrite(args[1].as.string_val.data, 1, args[1].as.string_val.length, f);
    fclose(f);
    return vex_bool(true);
}

static VexValue file_exists(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) {
        fprintf(stderr, "Error: file_exists() expects 1 string argument\n"); return vex_nothing();
    }
    FILE* f = fopen(args[0].as.string_val.data, "r");
    if (f) { fclose(f); return vex_bool(true); }
    return vex_bool(false);
}

/* ══════════════════════════════════════════════════════════════
 *  SYS MODULE
 *
 *  exit, clock, sleep, args, time
 * ══════════════════════════════════════════════════════════════ */

static VexValue sys_exit(VexValue* args, int argc) {
    int code = (argc >= 1 && args[0].type == VAL_INT) ? (int)args[0].as.int_val : 0;
    exit(code);
    return vex_nothing();
}

static VexValue sys_clock(VexValue* args, int argc) {
    UNUSED(args); UNUSED(argc);
    return vex_float((double)clock() / CLOCKS_PER_SEC);
}

static VexValue sys_time(VexValue* args, int argc) {
    UNUSED(args); UNUSED(argc);
    return vex_int((int64_t)time(NULL));
}

static VexValue sys_sleep(VexValue* args, int argc) {
    if (argc != 1) { fprintf(stderr, "Error: sleep() expects 1 argument (ms)\n"); return vex_nothing(); }
    int64_t ms = args[0].as.int_val;
    #ifdef _WIN32
        Sleep((DWORD)ms);
    #else
        struct timespec ts;
        ts.tv_sec = ms / 1000;
        ts.tv_nsec = (ms % 1000) * 1000000;
        nanosleep(&ts, NULL);
    #endif
    return vex_nothing();
}

static VexValue sys_platform(VexValue* args, int argc) {
    UNUSED(args); UNUSED(argc);
    #ifdef _WIN32
        return vex_string("windows", 7);
    #elif __APPLE__
        return vex_string("macos", 5);
    #elif __linux__
        return vex_string("linux", 5);
    #else
        return vex_string("unknown", 7);
    #endif
}

/* ══════════════════════════════════════════════════════════════
 *  TIME MODULE
 *
 *  now, format, sleep, clock
 * ══════════════════════════════════════════════════════════════ */

static VexValue time_now(VexValue* args, int argc) {
    UNUSED(args); UNUSED(argc);
    return vex_float((double)time(NULL));
}

static VexValue time_format(VexValue* args, int argc) {
    if (argc < 1 || argc > 2) {
        fprintf(stderr, "Error: format() expects 1 or 2 arguments\n");
        return vex_nothing();
    }

    time_t timestamp;
    if (args[0].type == VAL_INT) {
        timestamp = (time_t)args[0].as.int_val;
    } else if (args[0].type == VAL_FLOAT) {
        timestamp = (time_t)args[0].as.float_val;
    } else {
        fprintf(stderr, "Error: format() expects numeric timestamp\n");
        return vex_nothing();
    }

    const char* format = "%Y-%m-%d %H:%M:%S";
    if (argc == 2) {
        if (args[1].type != VAL_STRING) {
            fprintf(stderr, "Error: format() expects string format specifier\n");
            return vex_nothing();
        }
        format = args[1].as.string_val.data;
    }

    struct tm* tm_info = localtime(&timestamp);
    if (!tm_info) return vex_nothing();

    char buffer[256];
    size_t len = strftime(buffer, sizeof(buffer), format, tm_info);
    if (len == 0) return vex_nothing();
    return vex_string(buffer, (int)len);
}

/* ══════════════════════════════════════════════════════════════
 *  HTTP MODULE
 *
 *  get(url) -> string or nothing
 *  post(url, body) -> string or nothing
 * ══════════════════════════════════════════════════════════════ */

/* Parse http://host[:port]/path — returns false on failure */
static bool stdlib_parse_url(const char* url, char* host, int* port, char* path) {
    if (strncmp(url, "http://", 7) != 0) return false;
    const char* p = url + 7;
    int i = 0;
    while (*p && *p != ':' && *p != '/') host[i++] = *p++;
    host[i] = '\0';
    *port = 80;
    if (*p == ':') {
        p++;
        *port = 0;
        while (*p >= '0' && *p <= '9') *port = *port * 10 + (*p++ - '0');
    }
    if (*p == '/') {
        i = 0;
        while (*p) path[i++] = *p++;
        path[i] = '\0';
    } else {
        strcpy(path, "/");
    }
    return true;
}

/* Perform HTTP request; caller frees returned string (or NULL on error) */
static char* stdlib_http_request(const char* method, const char* url, const char* body) {
    char host[256]; int port; char path[512];
    if (!stdlib_parse_url(url, host, &port, path)) return NULL;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) return NULL;
#endif

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
#ifdef _WIN32
        WSACleanup();
#endif
        return NULL;
    }

    struct hostent* server = gethostbyname(host);
    if (!server) {
#ifdef _WIN32
        closesocket(sock); WSACleanup();
#else
        close((int)sock);
#endif
        return NULL;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((unsigned short)port);
    memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
#ifdef _WIN32
        closesocket(sock); WSACleanup();
#else
        close((int)sock);
#endif
        return NULL;
    }

    /* Build request */
    char request[4096];
    int blen = body ? (int)strlen(body) : 0;
    snprintf(request, sizeof(request),
        "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Vexium/1.0\r\nConnection: close\r\n",
        method, path, host);
    if (body && blen > 0) {
        char ch[128];
        snprintf(ch, sizeof(ch), "Content-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\n", blen);
        strncat(request, ch, sizeof(request) - strlen(request) - 1);
    }
    strncat(request, "\r\n", sizeof(request) - strlen(request) - 1);

    send(sock, request, (int)strlen(request), 0);
    if (body && blen > 0) send(sock, body, blen, 0);

    /* Receive */
    char* resp = (char*)malloc(65536); int total = 0, cap = 65536;
    while (1) {
        if (total + 4096 > cap) { cap *= 2; resp = (char*)realloc(resp, cap); }
        int n = (int)recv(sock, resp + total, 4096, 0);
        if (n <= 0) break;
        total += n;
    }
    resp[total] = '\0';

#ifdef _WIN32
    closesocket(sock); WSACleanup();
#else
    close((int)sock);
#endif

    /* Strip headers */
    char* body_start = strstr(resp, "\r\n\r\n");
    if (body_start) {
        size_t bl = strlen(body_start + 4);
        char* result = (char*)malloc(bl + 1);
        strcpy(result, body_start + 4);
        free(resp);
        return result;
    }
    return resp;
}

static VexValue http_get_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) {
        fprintf(stderr, "Error: http.get() expects 1 string argument\n");
        return vex_nothing();
    }
    char* resp = stdlib_http_request("GET", args[0].as.string_val.data, NULL);
    if (!resp) return vex_nothing();
    VexValue result = vex_string(resp, (int)strlen(resp));
    free(resp);
    return result;
}

static VexValue http_post_builtin(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        fprintf(stderr, "Error: http.post() expects 2 string arguments\n");
        return vex_nothing();
    }
    char* resp = stdlib_http_request("POST", args[0].as.string_val.data, args[1].as.string_val.data);
    if (!resp) return vex_nothing();
    VexValue result = vex_string(resp, (int)strlen(resp));
    free(resp);
    return result;
}

/* ══════════════════════════════════════════════════════════════
 *  COLLECTIONS MODULE — Enhanced Dynamic Arrays & Data Structures
 *
 *  Overcomes: Python's lack of built-in sorted-insert, no native
 *  slice-and-dice, verbose list comprehensions, no built-in
 *  flatten/zip. Rust/C++ require iterators boilerplate.
 *
 *  sort, slice, insert, remove, index_of, count, flatten, zip,
 *  enumerate, map_array, filter_array, reduce, any, all,
 *  reversed_array, unique
 * ══════════════════════════════════════════════════════════════ */

/* Helper: compare two VexValues for sorting */
static int vex_compare(const void* a, const void* b) {
    const VexValue* va = (const VexValue*)a;
    const VexValue* vb = (const VexValue*)b;
    if (va->type == VAL_INT && vb->type == VAL_INT)
        return (va->as.int_val > vb->as.int_val) - (va->as.int_val < vb->as.int_val);
    if (va->type == VAL_FLOAT || vb->type == VAL_FLOAT) {
        double da = va->type == VAL_INT ? (double)va->as.int_val : va->as.float_val;
        double db = vb->type == VAL_INT ? (double)vb->as.int_val : vb->as.float_val;
        return (da > db) - (da < db);
    }
    if (va->type == VAL_STRING && vb->type == VAL_STRING)
        return strcmp(va->as.string_val.data, vb->as.string_val.data);
    return 0;
}

/* Helper: push value to dynamic array */
static void arr_push(ValueArray* arr, VexValue val) {
    if (arr->count >= arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
        arr->items = (VexValue*)realloc(arr->items, sizeof(VexValue) * arr->capacity);
    }
    arr->items[arr->count++] = val;
}

/* sort(array) — returns new sorted array */
static VexValue coll_sort(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_ARRAY) {
        fprintf(stderr, "Error: sort() expects 1 array argument\n"); return vex_nothing();
    }
    ValueArray* src = args[0].as.array_val;
    VexValue result;
    result.type = VAL_ARRAY;
    result.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
    result.as.array_val->count = src->count;
    result.as.array_val->capacity = src->count;
    result.as.array_val->items = (VexValue*)malloc(sizeof(VexValue) * src->count);
    memcpy(result.as.array_val->items, src->items, sizeof(VexValue) * src->count);
    qsort(result.as.array_val->items, result.as.array_val->count, sizeof(VexValue), vex_compare);
    return result;
}

/* slice(array, start, end) — returns sub-array [start:end) */
static VexValue coll_slice(VexValue* args, int argc) {
    if (argc < 2 || args[0].type != VAL_ARRAY) {
        fprintf(stderr, "Error: slice() expects (array, start[, end])\n"); return vex_nothing();
    }
    ValueArray* src = args[0].as.array_val;
    int start = (int)args[1].as.int_val;
    int end = (argc >= 3) ? (int)args[2].as.int_val : src->count;
    if (start < 0) start += src->count;
    if (end < 0) end += src->count;
    if (start < 0) start = 0;
    if (end > src->count) end = src->count;

    VexValue result;
    result.type = VAL_ARRAY;
    result.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
    for (int i = start; i < end; i++) arr_push(result.as.array_val, src->items[i]);
    return result;
}

/* insert(array, index, value) — inserts at position (mutates) */
static VexValue coll_insert(VexValue* args, int argc) {
    if (argc != 3 || args[0].type != VAL_ARRAY || args[1].type != VAL_INT) {
        fprintf(stderr, "Error: insert() expects (array, index, value)\n"); return vex_nothing();
    }
    ValueArray* arr = args[0].as.array_val;
    int idx = (int)args[1].as.int_val;
    if (idx < 0) idx += arr->count;
    if (idx < 0 || idx > arr->count) { fprintf(stderr, "Error: insert() index out of bounds\n"); return vex_nothing(); }

    arr_push(arr, vex_nothing()); /* grow */
    /* Shift elements right */
    for (int i = arr->count - 1; i > idx; i--) arr->items[i] = arr->items[i-1];
    arr->items[idx] = args[2];
    return vex_nothing();
}

/* remove(array, index) — removes at index, returns removed value */
static VexValue coll_remove(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_INT) {
        fprintf(stderr, "Error: remove() expects (array, index)\n"); return vex_nothing();
    }
    ValueArray* arr = args[0].as.array_val;
    int idx = (int)args[1].as.int_val;
    if (idx < 0) idx += arr->count;
    if (idx < 0 || idx >= arr->count) { fprintf(stderr, "Error: remove() index out of bounds\n"); return vex_nothing(); }
    VexValue removed = arr->items[idx];
    for (int i = idx; i < arr->count - 1; i++) arr->items[i] = arr->items[i+1];
    arr->count--;
    return removed;
}

/* index_of(array, value) — returns first index or -1 */
static VexValue coll_index_of(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_ARRAY) {
        fprintf(stderr, "Error: index_of() expects (array, value)\n"); return vex_nothing();
    }
    ValueArray* arr = args[0].as.array_val;
    for (int i = 0; i < arr->count; i++) {
        if (arr->items[i].type == args[1].type) {
            if (args[1].type == VAL_INT && arr->items[i].as.int_val == args[1].as.int_val) return vex_int(i);
            if (args[1].type == VAL_STRING && strcmp(arr->items[i].as.string_val.data, args[1].as.string_val.data) == 0) return vex_int(i);
            if (args[1].type == VAL_FLOAT && arr->items[i].as.float_val == args[1].as.float_val) return vex_int(i);
            if (args[1].type == VAL_BOOL && arr->items[i].as.bool_val == args[1].as.bool_val) return vex_int(i);
        }
    }
    return vex_int(-1);
}

/* count(array, value) — count occurrences */
static VexValue coll_count(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_ARRAY) {
        fprintf(stderr, "Error: count() expects (array, value)\n"); return vex_nothing();
    }
    ValueArray* arr = args[0].as.array_val;
    int64_t c = 0;
    for (int i = 0; i < arr->count; i++) {
        if (arr->items[i].type == args[1].type) {
            if (args[1].type == VAL_INT && arr->items[i].as.int_val == args[1].as.int_val) c++;
            if (args[1].type == VAL_STRING && strcmp(arr->items[i].as.string_val.data, args[1].as.string_val.data) == 0) c++;
        }
    }
    return vex_int(c);
}

/* flatten(array) — flatten nested arrays one level */
static VexValue coll_flatten(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_ARRAY) {
        fprintf(stderr, "Error: flatten() expects 1 array argument\n"); return vex_nothing();
    }
    VexValue result;
    result.type = VAL_ARRAY;
    result.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
    ValueArray* src = args[0].as.array_val;
    for (int i = 0; i < src->count; i++) {
        if (src->items[i].type == VAL_ARRAY) {
            ValueArray* inner = src->items[i].as.array_val;
            for (int j = 0; j < inner->count; j++) arr_push(result.as.array_val, inner->items[j]);
        } else {
            arr_push(result.as.array_val, src->items[i]);
        }
    }
    return result;
}

/* zip(arr1, arr2) — returns array of [a, b] pairs */
static VexValue coll_zip(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_ARRAY) {
        fprintf(stderr, "Error: zip() expects 2 array arguments\n"); return vex_nothing();
    }
    VexValue result;
    result.type = VAL_ARRAY;
    result.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
    ValueArray* a = args[0].as.array_val;
    ValueArray* b = args[1].as.array_val;
    int min_len = a->count < b->count ? a->count : b->count;
    for (int i = 0; i < min_len; i++) {
        VexValue pair;
        pair.type = VAL_ARRAY;
        pair.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
        arr_push(pair.as.array_val, a->items[i]);
        arr_push(pair.as.array_val, b->items[i]);
        arr_push(result.as.array_val, pair);
    }
    return result;
}

/* enumerate(array) — returns [[0, item0], [1, item1], ...] */
static VexValue coll_enumerate(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_ARRAY) {
        fprintf(stderr, "Error: enumerate() expects 1 array argument\n"); return vex_nothing();
    }
    VexValue result;
    result.type = VAL_ARRAY;
    result.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
    ValueArray* src = args[0].as.array_val;
    for (int i = 0; i < src->count; i++) {
        VexValue pair;
        pair.type = VAL_ARRAY;
        pair.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
        arr_push(pair.as.array_val, vex_int(i));
        arr_push(pair.as.array_val, src->items[i]);
        arr_push(result.as.array_val, pair);
    }
    return result;
}

/* reversed(array) — returns new reversed array */
static VexValue coll_reversed(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_ARRAY) {
        fprintf(stderr, "Error: reversed() expects 1 array argument\n"); return vex_nothing();
    }
    VexValue result;
    result.type = VAL_ARRAY;
    result.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
    ValueArray* src = args[0].as.array_val;
    for (int i = src->count - 1; i >= 0; i--) arr_push(result.as.array_val, src->items[i]);
    return result;
}

/* unique(array) — returns array with duplicates removed */
static VexValue coll_unique(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_ARRAY) {
        fprintf(stderr, "Error: unique() expects 1 array argument\n"); return vex_nothing();
    }
    VexValue result;
    result.type = VAL_ARRAY;
    result.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
    ValueArray* src = args[0].as.array_val;
    for (int i = 0; i < src->count; i++) {
        bool found = false;
        for (int j = 0; j < result.as.array_val->count; j++) {
            if (src->items[i].type == result.as.array_val->items[j].type) {
                if (src->items[i].type == VAL_INT && src->items[i].as.int_val == result.as.array_val->items[j].as.int_val) { found = true; break; }
                if (src->items[i].type == VAL_STRING && strcmp(src->items[i].as.string_val.data, result.as.array_val->items[j].as.string_val.data) == 0) { found = true; break; }
                if (src->items[i].type == VAL_FLOAT && src->items[i].as.float_val == result.as.array_val->items[j].as.float_val) { found = true; break; }
            }
        }
        if (!found) arr_push(result.as.array_val, src->items[i]);
    }
    return result;
}

/* sum(array) — sum all numeric elements */
static VexValue coll_sum(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_ARRAY) {
        fprintf(stderr, "Error: sum() expects 1 array argument\n"); return vex_nothing();
    }
    ValueArray* arr = args[0].as.array_val;
    bool has_float = false;
    double total_f = 0.0;
    int64_t total_i = 0;
    for (int i = 0; i < arr->count; i++) {
        if (arr->items[i].type == VAL_FLOAT) { has_float = true; total_f += arr->items[i].as.float_val; }
        else if (arr->items[i].type == VAL_INT) { total_i += arr->items[i].as.int_val; total_f += (double)arr->items[i].as.int_val; }
    }
    return has_float ? vex_float(total_f) : vex_int(total_i);
}

/* any(array) — returns true if any element is truthy */
static VexValue coll_any(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_ARRAY) {
        fprintf(stderr, "Error: any() expects 1 array argument\n"); return vex_nothing();
    }
    ValueArray* arr = args[0].as.array_val;
    for (int i = 0; i < arr->count; i++) {
        if (vex_is_truthy(arr->items[i])) return vex_bool(true);
    }
    return vex_bool(false);
}

/* all(array) — returns true if all elements are truthy */
static VexValue coll_all(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_ARRAY) {
        fprintf(stderr, "Error: all() expects 1 array argument\n"); return vex_nothing();
    }
    ValueArray* arr = args[0].as.array_val;
    for (int i = 0; i < arr->count; i++) {
        if (!vex_is_truthy(arr->items[i])) return vex_bool(false);
    }
    return vex_bool(true);
}

/* ══════════════════════════════════════════════════════════════
 *  ALGO MODULE — Algorithms
 *
 *  Overcomes: Python has no built-in binary search (needs bisect),
 *  no built-in GCD before 3.5, no is_prime. JavaScript has
 *  nothing. Most languages require importing or writing these.
 *
 *  binary_search, shuffle, gcd, lcm, is_prime, fibonacci,
 *  factorial, clamp, lerp
 * ══════════════════════════════════════════════════════════════ */

/* binary_search(sorted_array, target) — returns index or -1 */
static VexValue algo_binary_search(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_ARRAY) {
        fprintf(stderr, "Error: binary_search() expects (sorted_array, target)\n"); return vex_nothing();
    }
    ValueArray* arr = args[0].as.array_val;
    int lo = 0, hi = arr->count - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int cmp = vex_compare(&arr->items[mid], &args[1]);
        if (cmp == 0) return vex_int(mid);
        if (cmp < 0) lo = mid + 1;
        else hi = mid - 1;
    }
    return vex_int(-1);
}

/* shuffle(array) — returns a new shuffled array */
static VexValue algo_shuffle(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_ARRAY) {
        fprintf(stderr, "Error: shuffle() expects 1 array argument\n"); return vex_nothing();
    }
    ValueArray* src = args[0].as.array_val;
    VexValue result;
    result.type = VAL_ARRAY;
    result.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
    result.as.array_val->count = src->count;
    result.as.array_val->capacity = src->count;
    result.as.array_val->items = (VexValue*)malloc(sizeof(VexValue) * src->count);
    memcpy(result.as.array_val->items, src->items, sizeof(VexValue) * src->count);
    /* Fisher-Yates shuffle */
    for (int i = src->count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        VexValue tmp = result.as.array_val->items[i];
        result.as.array_val->items[i] = result.as.array_val->items[j];
        result.as.array_val->items[j] = tmp;
    }
    return result;
}

/* gcd(a, b) — greatest common divisor */
static VexValue algo_gcd(VexValue* args, int argc) {
    if (argc != 2) { fprintf(stderr, "Error: gcd() expects 2 arguments\n"); return vex_nothing(); }
    int64_t a = args[0].as.int_val, b = args[1].as.int_val;
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) { int64_t t = b; b = a % b; a = t; }
    return vex_int(a);
}

/* lcm(a, b) — least common multiple */
static VexValue algo_lcm(VexValue* args, int argc) {
    if (argc != 2) { fprintf(stderr, "Error: lcm() expects 2 arguments\n"); return vex_nothing(); }
    int64_t a = args[0].as.int_val, b = args[1].as.int_val;
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    if (a == 0 || b == 0) return vex_int(0);
    /* Use GCD to compute LCM */
    int64_t ga = a, gb = b;
    while (gb != 0) { int64_t t = gb; gb = ga % gb; ga = t; }
    return vex_int(a / ga * b);
}

/* is_prime(n) — primality test */
static VexValue algo_is_prime(VexValue* args, int argc) {
    if (argc != 1) { fprintf(stderr, "Error: is_prime() expects 1 argument\n"); return vex_nothing(); }
    int64_t n = args[0].as.int_val;
    if (n < 2) return vex_bool(false);
    if (n < 4) return vex_bool(true);
    if (n % 2 == 0 || n % 3 == 0) return vex_bool(false);
    for (int64_t i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return vex_bool(false);
    }
    return vex_bool(true);
}

/* fibonacci(n) — returns nth fibonacci number */
static VexValue algo_fibonacci(VexValue* args, int argc) {
    if (argc != 1) { fprintf(stderr, "Error: fibonacci() expects 1 argument\n"); return vex_nothing(); }
    int64_t n = args[0].as.int_val;
    if (n <= 0) return vex_int(0);
    if (n == 1) return vex_int(1);
    int64_t a = 0, b = 1;
    for (int64_t i = 2; i <= n; i++) { int64_t t = a + b; a = b; b = t; }
    return vex_int(b);
}

/* clamp(value, min, max) — constrain value between bounds */
static VexValue algo_clamp(VexValue* args, int argc) {
    if (argc != 3) { fprintf(stderr, "Error: clamp() expects 3 arguments\n"); return vex_nothing(); }
    double v = args[0].type == VAL_INT ? (double)args[0].as.int_val : args[0].as.float_val;
    double lo = args[1].type == VAL_INT ? (double)args[1].as.int_val : args[1].as.float_val;
    double hi = args[2].type == VAL_INT ? (double)args[2].as.int_val : args[2].as.float_val;
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    if (args[0].type == VAL_INT && args[1].type == VAL_INT && args[2].type == VAL_INT)
        return vex_int((int64_t)v);
    return vex_float(v);
}

/* lerp(a, b, t) — linear interpolation: a + (b - a) * t */
static VexValue algo_lerp(VexValue* args, int argc) {
    if (argc != 3) { fprintf(stderr, "Error: lerp() expects 3 arguments (a, b, t)\n"); return vex_nothing(); }
    double a = args[0].type == VAL_INT ? (double)args[0].as.int_val : args[0].as.float_val;
    double b = args[1].type == VAL_INT ? (double)args[1].as.int_val : args[1].as.float_val;
    double t = args[2].type == VAL_INT ? (double)args[2].as.int_val : args[2].as.float_val;
    return vex_float(a + (b - a) * t);
}

/* ══════════════════════════════════════════════════════════════
 *  MODULE REGISTRATION TABLE
 * ══════════════════════════════════════════════════════════════ */

typedef struct {
    const char* name;
    BuiltinFn func;
} StdlibEntry;

static StdlibEntry math_entries[] = {
    {"sqrt",    math_sqrt},
    {"abs",     math_abs},
    {"pow",     math_pow},
    {"sin",     math_sin},
    {"cos",     math_cos},
    {"tan",     math_tan},
    {"floor",   math_floor},
    {"ceil",    math_ceil},
    {"round",   math_round},
    {"log",     math_log},
    {"min",     math_min},
    {"max",     math_max},
    {"random",  math_random},
    {"randint", math_randint},
    {NULL, NULL}
};

static StdlibEntry string_entries[] = {
    {"upper",       str_upper},
    {"lower",       str_lower},
    {"trim",        str_trim},
    {"contains",    str_contains},
    {"starts_with", str_starts_with},
    {"ends_with",   str_ends_with},
    {"find",        str_find},
    {"replace",     str_replace},
    {"split",       str_split},
    {"join",        str_join},
    {"reverse",     str_reverse},
    {"char_at",     str_char_at},
    {NULL, NULL}
};

static StdlibEntry file_entries[] = {
    {"read_file",    file_read},
    {"write_file",   file_write},
    {"append_file",  file_append},
    {"file_exists",  file_exists},
    {NULL, NULL}
};

static StdlibEntry sys_entries[] = {
    {"exit",     sys_exit},
    {"clock",    sys_clock},
    {"time",     sys_time},
    {"sleep",    sys_sleep},
    {"platform", sys_platform},
    {NULL, NULL}
};

static StdlibEntry time_entries[] = {
    {"now",    time_now},
    {"format", time_format},
    {"sleep",  sys_sleep},
    {"clock",  sys_clock},
    {NULL, NULL}
};

static StdlibEntry collections_entries[] = {
    {"sort",       coll_sort},
    {"slice",      coll_slice},
    {"insert",     coll_insert},
    {"remove",     coll_remove},
    {"index_of",   coll_index_of},
    {"count",      coll_count},
    {"flatten",    coll_flatten},
    {"zip",        coll_zip},
    {"enumerate",  coll_enumerate},
    {"reversed",   coll_reversed},
    {"unique",     coll_unique},
    {"sum",        coll_sum},
    {"any",        coll_any},
    {"all",        coll_all},
    {NULL, NULL}
};

static StdlibEntry algo_entries[] = {
    {"binary_search", algo_binary_search},
    {"shuffle",       algo_shuffle},
    {"gcd",           algo_gcd},
    {"lcm",           algo_lcm},
    {"is_prime",      algo_is_prime},
    {"fibonacci",     algo_fibonacci},
    {"clamp",         algo_clamp},
    {"lerp",          algo_lerp},
    {NULL, NULL}
};

static StdlibEntry json_entries[] = {
    {"parse",     json_parse_builtin},
    {"stringify", json_stringify_builtin},
    {NULL, NULL}
};

static StdlibEntry http_entries[] = {
    {"get",  http_get_builtin},
    {"post", http_post_builtin},
    {NULL, NULL}
};

/* Module table */
typedef struct {
    const char* module_name;
    StdlibEntry* entries;
} ModuleTable;

static ModuleTable modules[] = {
    {"math",        math_entries},
    {"string",      string_entries},
    {"file",        file_entries},
    {"sys",         sys_entries},
    {"time",        time_entries},
    {"collections", collections_entries},
    {"algo",        algo_entries},
    {"json",        json_entries},
    {"http",        http_entries},
    {NULL, NULL}
};

/* ══════════════════════════════════════════════════════════════
 *  REGISTRATION API
 * ══════════════════════════════════════════════════════════════ */

static void register_builtin(Environment* env, const char* name, BuiltinFn func) {
    VexValue val;
    val.type = VAL_BUILTIN_FN;
    val.as.builtin_fn.func = func;
    val.as.builtin_fn.name = name;
    env_define(env, name, val, true);
}

bool stdlib_load_module(const char* module_name, Environment* env) {
    /* Seed random on first math load */
    static bool rand_seeded = false;
    if (!rand_seeded && strcmp(module_name, "math") == 0) {
        srand((unsigned int)time(NULL));
        rand_seeded = true;
    }

    /* Register math constants */
    if (strcmp(module_name, "math") == 0) {
        env_define(env, "PI", vex_float(3.14159265358979323846), true);
        env_define(env, "E",  vex_float(2.71828182845904523536), true);
        env_define(env, "TAU", vex_float(6.28318530717958647692), true);
        env_define(env, "INF", vex_float(1.0/0.0), true);
    }

    for (int m = 0; modules[m].module_name != NULL; m++) {
        if (strcmp(modules[m].module_name, module_name) == 0) {
            StdlibEntry* entries = modules[m].entries;
            for (int i = 0; entries[i].name != NULL; i++) {
                register_builtin(env, entries[i].name, entries[i].func);
            }
            return true;
        }
    }
    return false;
}

bool stdlib_load_symbol(const char* module_name, const char* symbol_name, Environment* env) {
    for (int m = 0; modules[m].module_name != NULL; m++) {
        if (strcmp(modules[m].module_name, module_name) == 0) {
            StdlibEntry* entries = modules[m].entries;
            for (int i = 0; entries[i].name != NULL; i++) {
                if (strcmp(entries[i].name, symbol_name) == 0) {
                    register_builtin(env, entries[i].name, entries[i].func);
                    return true;
                }
            }
            /* Check for math constants */
            if (strcmp(module_name, "math") == 0) {
                if (strcmp(symbol_name, "PI") == 0) { env_define(env, "PI", vex_float(3.14159265358979323846), true); return true; }
                if (strcmp(symbol_name, "E") == 0) { env_define(env, "E", vex_float(2.71828182845904523536), true); return true; }
                if (strcmp(symbol_name, "TAU") == 0) { env_define(env, "TAU", vex_float(6.28318530717958647692), true); return true; }
            }
            fprintf(stderr, "Error: '%s' not found in module '%s'\n", symbol_name, module_name);
            return false;
        }
    }
    fprintf(stderr, "Error: Unknown module '%s'\n", module_name);
    return false;
}
