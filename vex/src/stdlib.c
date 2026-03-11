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
#include "tensor.h"
#include <math.h>
#include <time.h>
#ifdef USE_CUDA
#include <cuda_runtime_api.h>
#include <cuda.h>
#endif
#ifdef USE_NVRTC
#include <nvrtc.h>
#endif

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

/* slice(value, start, end, step) — Python-like slicing for arrays and strings */
static VexValue coll_slice(VexValue* args, int argc) {
    if (argc < 1 || (args[0].type != VAL_ARRAY && args[0].type != VAL_STRING)) {
        fprintf(stderr, "Error: slice() expects (array_or_string[, start[, end[, step]]])\n"); return vex_nothing();
    }

    int length = (args[0].type == VAL_ARRAY) ? args[0].as.array_val->count : args[0].as.string_val.length;
    int start = 0;
    int end = length;
    int step = 1;

    if (argc >= 2 && args[1].type == VAL_INT) start = (int)args[1].as.int_val;
    if (argc >= 3 && args[2].type == VAL_INT) end = (int)args[2].as.int_val;
    if (argc >= 4 && args[3].type == VAL_INT) step = (int)args[3].as.int_val;
    if (step == 0) step = 1;

    if (start < 0) start += length;
    if (end < 0) end += length;

    if (step > 0) {
        if (start < 0) start = 0;
        if (start > length) start = length;
        if (end < 0) end = 0;
        if (end > length) end = length;
    } else {
        if (start < -1) start = -1;
        if (start >= length) start = length - 1;
        if (end < -1) end = -1;
        if (end >= length) end = length - 1;
    }

    if (args[0].type == VAL_ARRAY) {
        VexValue result;
        result.type = VAL_ARRAY;
        result.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
        if (step > 0) {
            for (int i = start; i < end; i += step) arr_push(result.as.array_val, args[0].as.array_val->items[i]);
        } else {
            for (int i = start; i > end; i += step) arr_push(result.as.array_val, args[0].as.array_val->items[i]);
        }
        return result;
    }

    int capacity = (length > 0) ? length + 1 : 1;
    char* buf = (char*)malloc((size_t)capacity);
    int out = 0;
    if (step > 0) {
        for (int i = start; i < end; i += step) buf[out++] = args[0].as.string_val.data[i];
    } else {
        for (int i = start; i > end; i += step) buf[out++] = args[0].as.string_val.data[i];
    }
    buf[out] = '\0';
    VexValue result = vex_string(buf, out);
    free(buf);
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
 *  AI MODULE (tensor core + tokenizer)
 * ══════════════════════════════════════════════════════════════ */

typedef struct {
    int64_t handle;
    Tensor* tensor;
} AiTensorEntry;

static AiTensorEntry* g_ai_tensors = NULL;
static int g_ai_tensor_count = 0;
static int g_ai_tensor_capacity = 0;
static int64_t g_ai_next_tensor_handle = 1;

static int ai_tensor_find(int64_t handle) {
    for (int i = 0; i < g_ai_tensor_count; i++) {
        if (g_ai_tensors[i].handle == handle) return i;
    }
    return -1;
}

static int64_t ai_tensor_store(Tensor* t) {
    if (!t) return 0;
    if (g_ai_tensor_count >= g_ai_tensor_capacity) {
        int new_cap = g_ai_tensor_capacity == 0 ? 8 : g_ai_tensor_capacity * 2;
        AiTensorEntry* grown = (AiTensorEntry*)realloc(g_ai_tensors, sizeof(AiTensorEntry) * new_cap);
        if (!grown) {
            tensor_free(t);
            return 0;
        }
        g_ai_tensors = grown;
        g_ai_tensor_capacity = new_cap;
    }

    int64_t handle = g_ai_next_tensor_handle++;
    g_ai_tensors[g_ai_tensor_count].handle = handle;
    g_ai_tensors[g_ai_tensor_count].tensor = t;
    g_ai_tensor_count++;
    return handle;
}

static bool ai_shape_from_value_array(ValueArray* arr, int** shape_out, int* ndim_out) {
    if (!arr || arr->count <= 0) return false;
    int* shape = (int*)malloc(sizeof(int) * arr->count);
    if (!shape) return false;

    for (int i = 0; i < arr->count; i++) {
        if (arr->items[i].type != VAL_INT || arr->items[i].as.int_val <= 0) {
            free(shape);
            return false;
        }
        shape[i] = (int)arr->items[i].as.int_val;
    }

    *shape_out = shape;
    *ndim_out = arr->count;
    return true;
}

static bool ai_value_to_double(VexValue v, double* out) {
    if (out == NULL) return false;
    if (v.type == VAL_FLOAT) {
        *out = v.as.float_val;
        return true;
    }
    if (v.type == VAL_INT) {
        *out = (double)v.as.int_val;
        return true;
    }
    return false;
}

static Tensor* ai_tensor_from_value_array(ValueArray* arr) {
    if (arr == NULL || arr->count <= 0) return NULL;

    if (arr->items[0].type == VAL_ARRAY && arr->items[0].as.array_val != NULL) {
        int rows = arr->count;
        int cols = arr->items[0].as.array_val->count;
        int shape[2] = {rows, cols};
        Tensor* t = NULL;
        if (cols <= 0) return NULL;
        for (int r = 0; r < rows; r++) {
            if (arr->items[r].type != VAL_ARRAY || arr->items[r].as.array_val == NULL || arr->items[r].as.array_val->count != cols) {
                return NULL;
            }
        }
        t = tensor_new(TENSOR_FLOAT32, 2, shape);
        if (t == NULL) return NULL;
        for (int r = 0; r < rows; r++) {
            ValueArray* row = arr->items[r].as.array_val;
            for (int c = 0; c < cols; c++) {
                double v = 0.0;
                if (!ai_value_to_double(row->items[c], &v)) {
                    tensor_free(t);
                    return NULL;
                }
                t->data.f32_data[r * cols + c] = (float)v;
            }
        }
        return t;
    }

    {
        int n = arr->count;
        int shape[1] = {n};
        Tensor* t = tensor_new(TENSOR_FLOAT32, 1, shape);
        if (t == NULL) return NULL;
        for (int i = 0; i < n; i++) {
            double v = 0.0;
            if (!ai_value_to_double(arr->items[i], &v)) {
                tensor_free(t);
                return NULL;
            }
            t->data.f32_data[i] = (float)v;
        }
        return t;
    }
}

static bool ai_tensor_resolve_arg(VexValue arg, Tensor** tensor_out, bool* owns_tensor) {
    if (tensor_out == NULL || owns_tensor == NULL) return false;
    *tensor_out = NULL;
    *owns_tensor = false;

    if (arg.type == VAL_INT) {
        int idx = ai_tensor_find(arg.as.int_val);
        if (idx < 0) return false;
        *tensor_out = g_ai_tensors[idx].tensor;
        return true;
    }

    if (arg.type == VAL_ARRAY && arg.as.array_val != NULL) {
        Tensor* tmp = ai_tensor_from_value_array(arg.as.array_val);
        if (tmp == NULL) return false;
        *tensor_out = tmp;
        *owns_tensor = true;
        return true;
    }

    return false;
}

static VexValue ai_tensor_from_array_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_ARRAY || args[0].as.array_val == NULL) return vex_int(0);
    return vex_int(ai_tensor_store(ai_tensor_from_value_array(args[0].as.array_val)));
}

static VexValue ai_tensor_to_array_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_INT) return vex_nothing();
    int idx = ai_tensor_find(args[0].as.int_val);
    if (idx < 0) return vex_nothing();

    {
        Tensor* t = g_ai_tensors[idx].tensor;
        VexValue out;
        out.type = VAL_ARRAY;
        out.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
        if (out.as.array_val == NULL) return vex_nothing();

        if (t->ndim == 1) {
            int n = t->shape[0];
            out.as.array_val->capacity = n > 0 ? n : 1;
            out.as.array_val->items = (VexValue*)calloc(out.as.array_val->capacity, sizeof(VexValue));
            if (out.as.array_val->items == NULL) return vex_nothing();
            for (int i = 0; i < n; i++) {
                out.as.array_val->items[out.as.array_val->count++] = vex_float(t->data.f32_data[i]);
            }
            return out;
        }

        if (t->ndim == 2) {
            int rows = t->shape[0];
            int cols = t->shape[1];
            out.as.array_val->capacity = rows > 0 ? rows : 1;
            out.as.array_val->items = (VexValue*)calloc(out.as.array_val->capacity, sizeof(VexValue));
            if (out.as.array_val->items == NULL) return vex_nothing();

            for (int r = 0; r < rows; r++) {
                VexValue row_val;
                row_val.type = VAL_ARRAY;
                row_val.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
                if (row_val.as.array_val == NULL) return vex_nothing();
                row_val.as.array_val->capacity = cols > 0 ? cols : 1;
                row_val.as.array_val->items = (VexValue*)calloc(row_val.as.array_val->capacity, sizeof(VexValue));
                if (row_val.as.array_val->items == NULL) return vex_nothing();

                for (int c = 0; c < cols; c++) {
                    row_val.as.array_val->items[row_val.as.array_val->count++] = vex_float(t->data.f32_data[r * cols + c]);
                }

                out.as.array_val->items[out.as.array_val->count++] = row_val;
            }
            return out;
        }

        return vex_nothing();
    }
}

static VexValue ai_tensor_zeros_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_ARRAY || !args[0].as.array_val) return vex_int(0);
    int* shape = NULL;
    int ndim = 0;
    if (!ai_shape_from_value_array(args[0].as.array_val, &shape, &ndim)) return vex_int(0);
    Tensor* t = tensor_zeros(TENSOR_FLOAT32, ndim, shape);
    free(shape);
    return vex_int(ai_tensor_store(t));
}

static VexValue ai_tensor_rand_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_ARRAY || !args[0].as.array_val) return vex_int(0);
    int* shape = NULL;
    int ndim = 0;
    if (!ai_shape_from_value_array(args[0].as.array_val, &shape, &ndim)) return vex_int(0);
    Tensor* t = tensor_rand(ndim, shape);
    free(shape);
    return vex_int(ai_tensor_store(t));
}

static VexValue ai_tensor_matmul_builtin(VexValue* args, int argc) {
    Tensor *a = NULL, *b = NULL, *out = NULL;
    bool own_a = false, own_b = false;
    int64_t handle;
    if (argc != 2) return vex_int(0);
    if (!ai_tensor_resolve_arg(args[0], &a, &own_a) || !ai_tensor_resolve_arg(args[1], &b, &own_b)) {
        if (own_a && a) tensor_free(a);
        if (own_b && b) tensor_free(b);
        return vex_int(0);
    }
    out = tensor_matmul(a, b);
    if (own_a && a) tensor_free(a);
    if (own_b && b) tensor_free(b);
    handle = ai_tensor_store(out);
    return vex_int(handle);
}

static VexValue ai_tensor_sum_builtin(VexValue* args, int argc) {
    Tensor* t = NULL;
    bool own_t = false;
    double sum;
    if (argc != 1) return vex_nothing();
    if (!ai_tensor_resolve_arg(args[0], &t, &own_t)) return vex_nothing();
    sum = tensor_sum(t);
    if (own_t) tensor_free(t);
    return vex_float(sum);
}

static VexValue ai_tensor_shape_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_INT) return vex_nothing();
    int idx = ai_tensor_find(args[0].as.int_val);
    if (idx < 0) return vex_nothing();

    Tensor* t = g_ai_tensors[idx].tensor;
    VexValue out;
    out.type = VAL_ARRAY;
    out.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
    if (!out.as.array_val) return vex_nothing();

    for (int i = 0; i < t->ndim; i++) {
        ValueArray* arr = out.as.array_val;
        if (arr->count >= arr->capacity) {
            arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
            arr->items = (VexValue*)realloc(arr->items, sizeof(VexValue) * arr->capacity);
            if (!arr->items) return vex_nothing();
        }
        arr->items[arr->count++] = vex_int(t->shape[i]);
    }
    return out;
}

static VexValue ai_tensor_free_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_INT) return vex_bool(false);
    int idx = ai_tensor_find(args[0].as.int_val);
    if (idx < 0) return vex_bool(false);

    tensor_free(g_ai_tensors[idx].tensor);
    g_ai_tensor_count--;
    if (idx != g_ai_tensor_count) {
        g_ai_tensors[idx] = g_ai_tensors[g_ai_tensor_count];
    }
    return vex_bool(true);
}

static VexValue ai_tokenize_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) return vex_nothing();
    const char* s = args[0].as.string_val.data;
    int len = args[0].as.string_val.length;

    VexValue out;
    out.type = VAL_ARRAY;
    out.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
    if (!out.as.array_val) return vex_nothing();

    int i = 0;
    while (i < len) {
        while (i < len && isspace((unsigned char)s[i])) i++;
        if (i >= len) break;
        int start = i;
        while (i < len && !isspace((unsigned char)s[i])) i++;

        ValueArray* arr = out.as.array_val;
        if (arr->count >= arr->capacity) {
            arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
            arr->items = (VexValue*)realloc(arr->items, sizeof(VexValue) * arr->capacity);
            if (!arr->items) return vex_nothing();
        }
        arr->items[arr->count++] = vex_string(s + start, i - start);
    }
    return out;
}

/* ── Extended AI: tensor creation ── */
static VexValue ai_tensor_ones_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_ARRAY || !args[0].as.array_val) return vex_int(0);
    int* shape = NULL; int ndim = 0;
    if (!ai_shape_from_value_array(args[0].as.array_val, &shape, &ndim)) return vex_int(0);
    Tensor* t = tensor_ones(TENSOR_FLOAT32, ndim, shape);
    free(shape);
    return vex_int(ai_tensor_store(t));
}

static VexValue ai_tensor_randn_builtin(VexValue* args, int argc) {
    if (argc < 1 || args[0].type != VAL_ARRAY || !args[0].as.array_val) return vex_int(0);
    int* shape = NULL; int ndim = 0;
    if (!ai_shape_from_value_array(args[0].as.array_val, &shape, &ndim)) return vex_int(0);
    double mean = 0.0, std_val = 1.0;
    if (argc >= 2) {
        if (args[1].type == VAL_FLOAT) mean = args[1].as.float_val;
        else if (args[1].type == VAL_INT) mean = (double)args[1].as.int_val;
    }
    if (argc >= 3) {
        if (args[2].type == VAL_FLOAT) std_val = args[2].as.float_val;
        else if (args[2].type == VAL_INT) std_val = (double)args[2].as.int_val;
    }
    Tensor* t = tensor_randn(ndim, shape, mean, std_val);
    free(shape);
    return vex_int(ai_tensor_store(t));
}

static VexValue ai_tensor_fill_builtin(VexValue* args, int argc) {
    if (argc < 2 || args[0].type != VAL_ARRAY || !args[0].as.array_val) return vex_int(0);
    double val = (args[1].type == VAL_FLOAT) ? args[1].as.float_val :
                 (args[1].type == VAL_INT)   ? (double)args[1].as.int_val : 0.0;
    int* shape = NULL; int ndim = 0;
    if (!ai_shape_from_value_array(args[0].as.array_val, &shape, &ndim)) return vex_int(0);
    Tensor* t = tensor_full(TENSOR_FLOAT32, val, ndim, shape);
    free(shape);
    return vex_int(ai_tensor_store(t));
}

static VexValue ai_tensor_clone_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_INT) return vex_int(0);
    int idx = ai_tensor_find(args[0].as.int_val);
    if (idx < 0) return vex_int(0);
    return vex_int(ai_tensor_store(tensor_clone(g_ai_tensors[idx].tensor)));
}

/* ── Extended AI: binary element-wise ops ── */
static VexValue ai_tensor_add_builtin(VexValue* args, int argc) {
    Tensor *a = NULL, *b = NULL;
    bool own_a = false, own_b = false;
    Tensor* out;
    if (argc != 2) return vex_int(0);
    if (!ai_tensor_resolve_arg(args[0], &a, &own_a) || !ai_tensor_resolve_arg(args[1], &b, &own_b)) {
        if (own_a && a) tensor_free(a);
        if (own_b && b) tensor_free(b);
        return vex_int(0);
    }
    out = tensor_clone(a);
    if (!out) {
        if (own_a && a) tensor_free(a);
        if (own_b && b) tensor_free(b);
        return vex_int(0);
    }
    tensor_iadd(out, b);
    if (own_a && a) tensor_free(a);
    if (own_b && b) tensor_free(b);
    return vex_int(ai_tensor_store(out));
}

static VexValue ai_tensor_sub_builtin(VexValue* args, int argc) {
    Tensor *a = NULL, *b = NULL;
    bool own_a = false, own_b = false;
    Tensor* out;
    if (argc != 2) return vex_int(0);
    if (!ai_tensor_resolve_arg(args[0], &a, &own_a) || !ai_tensor_resolve_arg(args[1], &b, &own_b)) {
        if (own_a && a) tensor_free(a);
        if (own_b && b) tensor_free(b);
        return vex_int(0);
    }
    out = tensor_clone(a);
    if (!out) {
        if (own_a && a) tensor_free(a);
        if (own_b && b) tensor_free(b);
        return vex_int(0);
    }
    tensor_isub(out, b);
    if (own_a && a) tensor_free(a);
    if (own_b && b) tensor_free(b);
    return vex_int(ai_tensor_store(out));
}

static VexValue ai_tensor_mul_builtin(VexValue* args, int argc) {
    Tensor *a = NULL, *b = NULL;
    bool own_a = false, own_b = false;
    Tensor* out;
    if (argc != 2) return vex_int(0);
    if (!ai_tensor_resolve_arg(args[0], &a, &own_a) || !ai_tensor_resolve_arg(args[1], &b, &own_b)) {
        if (own_a && a) tensor_free(a);
        if (own_b && b) tensor_free(b);
        return vex_int(0);
    }
    out = tensor_clone(a);
    if (!out) {
        if (own_a && a) tensor_free(a);
        if (own_b && b) tensor_free(b);
        return vex_int(0);
    }
    tensor_imul(out, b);
    if (own_a && a) tensor_free(a);
    if (own_b && b) tensor_free(b);
    return vex_int(ai_tensor_store(out));
}

static VexValue ai_tensor_div_builtin(VexValue* args, int argc) {
    Tensor *a = NULL, *b = NULL;
    bool own_a = false, own_b = false;
    Tensor* out;
    if (argc != 2) return vex_int(0);
    if (!ai_tensor_resolve_arg(args[0], &a, &own_a) || !ai_tensor_resolve_arg(args[1], &b, &own_b)) {
        if (own_a && a) tensor_free(a);
        if (own_b && b) tensor_free(b);
        return vex_int(0);
    }
    out = tensor_clone(a);
    if (!out) {
        if (own_a && a) tensor_free(a);
        if (own_b && b) tensor_free(b);
        return vex_int(0);
    }
    tensor_idiv(out, b);
    if (own_a && a) tensor_free(a);
    if (own_b && b) tensor_free(b);
    return vex_int(ai_tensor_store(out));
}

static VexValue ai_tensor_scale_builtin(VexValue* args, int argc) {
    Tensor* t = NULL;
    bool own_t = false;
    if (argc != 2) return vex_int(0);
    double s = (args[1].type == VAL_FLOAT) ? args[1].as.float_val :
               (args[1].type == VAL_INT)   ? (double)args[1].as.int_val : 1.0;
    Tensor* out;
    if (!ai_tensor_resolve_arg(args[0], &t, &own_t)) return vex_int(0);
    out = tensor_clone(t);
    if (!out) {
        if (own_t) tensor_free(t);
        return vex_int(0);
    }
    tensor_imul_scalar(out, s);
    if (own_t) tensor_free(t);
    return vex_int(ai_tensor_store(out));
}

/* ── Extended AI: activations ── */
static VexValue ai_tensor_relu_builtin(VexValue* args, int argc) {
    Tensor* t = NULL;
    bool own_t = false;
    Tensor* out;
    if (argc != 1) return vex_int(0);
    if (!ai_tensor_resolve_arg(args[0], &t, &own_t)) return vex_int(0);
    out = tensor_relu(t);
    if (own_t) tensor_free(t);
    return vex_int(ai_tensor_store(out));
}

static VexValue ai_tensor_sigmoid_builtin(VexValue* args, int argc) {
    Tensor* t = NULL;
    bool own_t = false;
    Tensor* out;
    if (argc != 1) return vex_int(0);
    if (!ai_tensor_resolve_arg(args[0], &t, &own_t)) return vex_int(0);
    out = tensor_sigmoid(t);
    if (own_t) tensor_free(t);
    return vex_int(ai_tensor_store(out));
}

static VexValue ai_tensor_tanh_builtin(VexValue* args, int argc) {
    Tensor* t = NULL;
    bool own_t = false;
    Tensor* out;
    if (argc != 1) return vex_int(0);
    if (!ai_tensor_resolve_arg(args[0], &t, &own_t)) return vex_int(0);
    out = tensor_tanh(t);
    if (own_t) tensor_free(t);
    return vex_int(ai_tensor_store(out));
}

static VexValue ai_tensor_softmax_builtin(VexValue* args, int argc) {
    Tensor* t = NULL;
    bool own_t = false;
    Tensor* out;
    if (argc != 1) return vex_int(0);
    if (!ai_tensor_resolve_arg(args[0], &t, &own_t)) return vex_int(0);
    out = tensor_softmax(t);
    if (own_t) tensor_free(t);
    return vex_int(ai_tensor_store(out));
}

/* ── Extended AI: shape ops ── */
static VexValue ai_tensor_reshape_builtin(VexValue* args, int argc) {
    Tensor* t = NULL;
    bool own_t = false;
    if (argc != 2 || args[1].type != VAL_ARRAY || !args[1].as.array_val) return vex_int(0);
    if (!ai_tensor_resolve_arg(args[0], &t, &own_t)) return vex_int(0);
    int* shape = NULL; int ndim = 0;
    if (!ai_shape_from_value_array(args[1].as.array_val, &shape, &ndim)) return vex_int(0);
    Tensor* out = tensor_reshape(t, ndim, shape);
    free(shape);
    if (own_t) tensor_free(t);
    return vex_int(ai_tensor_store(out));
}

static VexValue ai_tensor_transpose_builtin(VexValue* args, int argc) {
    Tensor* t = NULL;
    bool own_t = false;
    Tensor* out;
    if (argc != 1) return vex_int(0);
    if (!ai_tensor_resolve_arg(args[0], &t, &own_t)) return vex_int(0);
    out = tensor_transpose(t);
    if (own_t) tensor_free(t);
    return vex_int(ai_tensor_store(out));
}

/* ── Extended AI: reductions ── */
static VexValue ai_tensor_mean_builtin(VexValue* args, int argc) {
    Tensor* t = NULL;
    bool own_t = false;
    double val;
    if (argc != 1) return vex_nothing();
    if (!ai_tensor_resolve_arg(args[0], &t, &own_t)) return vex_nothing();
    val = tensor_mean(t);
    if (own_t) tensor_free(t);
    return vex_float(val);
}

static VexValue ai_tensor_std_builtin(VexValue* args, int argc) {
    Tensor* t = NULL;
    bool own_t = false;
    double val;
    if (argc != 1) return vex_nothing();
    if (!ai_tensor_resolve_arg(args[0], &t, &own_t)) return vex_nothing();
    val = tensor_std(t);
    if (own_t) tensor_free(t);
    return vex_float(val);
}

static VexValue ai_tensor_max_builtin(VexValue* args, int argc) {
    Tensor* t = NULL;
    bool own_t = false;
    double val;
    if (argc != 1) return vex_nothing();
    if (!ai_tensor_resolve_arg(args[0], &t, &own_t)) return vex_nothing();
    val = tensor_max(t);
    if (own_t) tensor_free(t);
    return vex_float(val);
}

static VexValue ai_tensor_min_builtin(VexValue* args, int argc) {
    Tensor* t = NULL;
    bool own_t = false;
    double val;
    if (argc != 1) return vex_nothing();
    if (!ai_tensor_resolve_arg(args[0], &t, &own_t)) return vex_nothing();
    val = tensor_min(t);
    if (own_t) tensor_free(t);
    return vex_float(val);
}

/* ── Extended AI: optimizer ── */
typedef struct {
    int64_t handle;
    void* opt;
    OptimizerType opt_type;
} AiOptimizerEntry;

static AiOptimizerEntry* g_ai_optimizers = NULL;
static int g_ai_opt_count = 0;
static int g_ai_opt_capacity = 0;
static int64_t g_ai_next_opt_handle = 1;

static int ai_opt_find(int64_t handle) {
    for (int i = 0; i < g_ai_opt_count; i++) {
        if (g_ai_optimizers[i].handle == handle) return i;
    }
    return -1;
}

static int64_t ai_opt_store(void* opt, OptimizerType type) {
    if (!opt) return 0;
    if (g_ai_opt_count >= g_ai_opt_capacity) {
        int new_cap = g_ai_opt_capacity == 0 ? 8 : g_ai_opt_capacity * 2;
        AiOptimizerEntry* grown = (AiOptimizerEntry*)realloc(g_ai_optimizers, sizeof(AiOptimizerEntry) * new_cap);
        if (!grown) { optimizer_free(opt, type); return 0; }
        g_ai_optimizers = grown;
        g_ai_opt_capacity = new_cap;
    }
    int64_t handle = g_ai_next_opt_handle++;
    g_ai_optimizers[g_ai_opt_count].handle = handle;
    g_ai_optimizers[g_ai_opt_count].opt = opt;
    g_ai_optimizers[g_ai_opt_count].opt_type = type;
    g_ai_opt_count++;
    return handle;
}

static VexValue ai_optimizer_create_builtin(VexValue* args, int argc) {
    if (argc < 2 || args[0].type != VAL_STRING) return vex_int(0);
    const char* type_str = args[0].as.string_val.data;
    double lr = (args[1].type == VAL_FLOAT) ? args[1].as.float_val :
                (args[1].type == VAL_INT)   ? (double)args[1].as.int_val : 0.001;
    OptimizerType type = OPTIMIZER_SGD;
    if (strcmp(type_str, "adam") == 0)         type = OPTIMIZER_ADAM;
    else if (strcmp(type_str, "rmsprop") == 0) type = OPTIMIZER_RMSPROP;
    void* opt = optimizer_create(type, lr);
    return vex_int(ai_opt_store(opt, type));
}

static VexValue ai_optimizer_step_builtin(VexValue* args, int argc) {
    if (argc != 3 || args[0].type != VAL_INT || args[1].type != VAL_INT || args[2].type != VAL_INT) return vex_bool(false);
    int oi = ai_opt_find(args[0].as.int_val);
    int pi = ai_tensor_find(args[1].as.int_val);
    int gi = ai_tensor_find(args[2].as.int_val);
    if (oi < 0 || pi < 0 || gi < 0) return vex_bool(false);
    optimizer_update(g_ai_optimizers[oi].opt, g_ai_optimizers[oi].opt_type,
                     g_ai_tensors[pi].tensor, g_ai_tensors[gi].tensor);
    return vex_bool(true);
}

static VexValue ai_optimizer_free_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_INT) return vex_bool(false);
    int idx = ai_opt_find(args[0].as.int_val);
    if (idx < 0) return vex_bool(false);
    optimizer_free(g_ai_optimizers[idx].opt, g_ai_optimizers[idx].opt_type);
    g_ai_opt_count--;
    if (idx != g_ai_opt_count) g_ai_optimizers[idx] = g_ai_optimizers[g_ai_opt_count];
    return vex_bool(true);
}

/* ── Extended AI: neural network model ── */
typedef struct {
    int64_t handle;
    NNModel* model;
} AiModelEntry;

static AiModelEntry* g_ai_models = NULL;
static int g_ai_model_count = 0;
static int g_ai_model_capacity = 0;
static int64_t g_ai_next_model_handle = 1;

static int ai_model_find(int64_t handle) {
    for (int i = 0; i < g_ai_model_count; i++) {
        if (g_ai_models[i].handle == handle) return i;
    }
    return -1;
}

static int64_t ai_model_store(NNModel* m) {
    if (!m) return 0;
    if (g_ai_model_count >= g_ai_model_capacity) {
        int new_cap = g_ai_model_capacity == 0 ? 8 : g_ai_model_capacity * 2;
        AiModelEntry* grown = (AiModelEntry*)realloc(g_ai_models, sizeof(AiModelEntry) * new_cap);
        if (!grown) { nn_model_free(m); return 0; }
        g_ai_models = grown;
        g_ai_model_capacity = new_cap;
    }
    int64_t handle = g_ai_next_model_handle++;
    g_ai_models[g_ai_model_count].handle = handle;
    g_ai_models[g_ai_model_count].model = m;
    g_ai_model_count++;
    return handle;
}

static VexValue ai_nn_new_builtin(VexValue* args, int argc) {
    const char* name = (argc >= 1 && args[0].type == VAL_STRING) ? args[0].as.string_val.data : "model";
    return vex_int(ai_model_store(nn_model_new(name)));
}

static VexValue ai_nn_add_dense_builtin(VexValue* args, int argc) {
    if (argc < 2 || args[0].type != VAL_INT || args[1].type != VAL_INT) return vex_bool(false);
    int idx = ai_model_find(args[0].as.int_val);
    if (idx < 0) return vex_bool(false);
    int units = (int)args[1].as.int_val;
    ActivationType act = ACTIVATION_NONE;
    if (argc >= 3 && args[2].type == VAL_STRING) {
        const char* s = args[2].as.string_val.data;
        if (strcmp(s, "relu") == 0)         act = ACTIVATION_RELU;
        else if (strcmp(s, "sigmoid") == 0) act = ACTIVATION_SIGMOID;
        else if (strcmp(s, "tanh") == 0)    act = ACTIVATION_TANH;
        else if (strcmp(s, "softmax") == 0) act = ACTIVATION_SOFTMAX;
    }
    int in_size = 0;
    if (argc >= 4 && args[3].type == VAL_INT) in_size = (int)args[3].as.int_val;
    nn_model_add_dense(g_ai_models[idx].model, units, act, in_size);
    return vex_bool(true);
}

static VexValue ai_nn_add_conv2d_builtin(VexValue* args, int argc) {
    if (argc < 3 || args[0].type != VAL_INT || args[1].type != VAL_INT || args[2].type != VAL_INT) return vex_bool(false);
    int idx = ai_model_find(args[0].as.int_val);
    if (idx < 0) return vex_bool(false);
    int filters = (int)args[1].as.int_val;
    int kernel = (int)args[2].as.int_val;
    int stride = (argc >= 4 && args[3].type == VAL_INT) ? (int)args[3].as.int_val : 1;
    int padding = (argc >= 5 && args[4].type == VAL_INT) ? (int)args[4].as.int_val : 0;
    ActivationType act = ACTIVATION_NONE;
    if (argc >= 6 && args[5].type == VAL_STRING) {
        const char* s = args[5].as.string_val.data;
        if (strcmp(s, "relu") == 0)         act = ACTIVATION_RELU;
        else if (strcmp(s, "sigmoid") == 0) act = ACTIVATION_SIGMOID;
        else if (strcmp(s, "tanh") == 0)    act = ACTIVATION_TANH;
        else if (strcmp(s, "softmax") == 0) act = ACTIVATION_SOFTMAX;
    }
    nn_model_add_conv2d(g_ai_models[idx].model, filters, kernel, stride, padding, act);
    return vex_bool(true);
}

static VexValue ai_nn_add_dropout_builtin(VexValue* args, int argc) {
    double rate = 0.0;
    int idx;
    if (argc != 2 || args[0].type != VAL_INT) return vex_bool(false);
    idx = ai_model_find(args[0].as.int_val);
    if (idx < 0) return vex_bool(false);
    if (args[1].type == VAL_FLOAT) rate = args[1].as.float_val;
    else if (args[1].type == VAL_INT) rate = (double)args[1].as.int_val;
    else return vex_bool(false);
    nn_model_add_dropout(g_ai_models[idx].model, rate);
    return vex_bool(true);
}

static VexValue ai_nn_add_maxpool2d_builtin(VexValue* args, int argc) {
    if (argc < 2 || args[0].type != VAL_INT || args[1].type != VAL_INT) return vex_bool(false);
    int idx = ai_model_find(args[0].as.int_val);
    if (idx < 0) return vex_bool(false);
    int pool = (int)args[1].as.int_val;
    int stride = (argc >= 3 && args[2].type == VAL_INT) ? (int)args[2].as.int_val : pool;
    nn_model_add_maxpool2d(g_ai_models[idx].model, pool, stride);
    return vex_bool(true);
}

static VexValue ai_nn_forward_builtin(VexValue* args, int argc) {
    Tensor* x = NULL;
    bool own_x = false;
    if (argc != 2 || args[0].type != VAL_INT) return vex_int(0);
    int mi = ai_model_find(args[0].as.int_val);
    Tensor* out;
    if (mi < 0) return vex_int(0);
    if (!ai_tensor_resolve_arg(args[1], &x, &own_x)) return vex_int(0);
    out = nn_model_forward(g_ai_models[mi].model, x);
    if (own_x) tensor_free(x);
    return vex_int(ai_tensor_store(out));
}

static VexValue ai_nn_predict_builtin(VexValue* args, int argc) {
    Tensor* x = NULL;
    bool own_x = false;
    if (argc != 2 || args[0].type != VAL_INT) return vex_int(0);
    int mi = ai_model_find(args[0].as.int_val);
    Tensor* out;
    if (mi < 0) return vex_int(0);
    if (!ai_tensor_resolve_arg(args[1], &x, &own_x)) return vex_int(0);
    out = nn_model_predict(g_ai_models[mi].model, x);
    if (own_x) tensor_free(x);
    return vex_int(ai_tensor_store(out));
}

static VexValue ai_nn_train_step_builtin(VexValue* args, int argc) {
    Tensor *x = NULL, *y = NULL;
    bool own_x = false, own_y = false;
    double loss;
    if (argc < 3 || args[0].type != VAL_INT) return vex_float(0.0);
    int mi = ai_model_find(args[0].as.int_val);
    if (mi < 0) return vex_float(0.0);
    if (!ai_tensor_resolve_arg(args[1], &x, &own_x) || !ai_tensor_resolve_arg(args[2], &y, &own_y)) {
        if (own_x && x) tensor_free(x);
        if (own_y && y) tensor_free(y);
        return vex_float(0.0);
    }
    int batch = (argc >= 4 && args[3].type == VAL_INT) ? (int)args[3].as.int_val : 32;
    LossType loss_type = LOSS_MSE;
    if (argc >= 5 && args[4].type == VAL_STRING) {
        const char* loss_name = args[4].as.string_val.data;
        if (strcmp(loss_name, "cross_entropy") == 0 || strcmp(loss_name, "ce") == 0) {
            loss_type = LOSS_CROSS_ENTROPY;
        }
    }
    loss = nn_model_train_with_loss(g_ai_models[mi].model, x, y, batch, loss_type);
    if (own_x && x) tensor_free(x);
    if (own_y && y) tensor_free(y);
    return vex_float(loss);
}

static VexValue ai_nn_set_lr_builtin(VexValue* args, int argc) {
    double learning_rate = 0.0;
    int idx;
    if (argc != 2 || args[0].type != VAL_INT) return vex_bool(false);
    idx = ai_model_find(args[0].as.int_val);
    if (idx < 0) return vex_bool(false);
    if (args[1].type == VAL_FLOAT) learning_rate = args[1].as.float_val;
    else if (args[1].type == VAL_INT) learning_rate = (double)args[1].as.int_val;
    else return vex_bool(false);
    nn_model_set_learning_rate(g_ai_models[idx].model, learning_rate);
    return vex_bool(true);
}

static VexValue ai_nn_get_lr_builtin(VexValue* args, int argc) {
    int idx;
    if (argc != 1 || args[0].type != VAL_INT) return vex_float(0.0);
    idx = ai_model_find(args[0].as.int_val);
    if (idx < 0) return vex_float(0.0);
    return vex_float(nn_model_get_learning_rate(g_ai_models[idx].model));
}

static VexValue ai_nn_save_builtin(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_INT || args[1].type != VAL_STRING) return vex_bool(false);
    int idx = ai_model_find(args[0].as.int_val);
    if (idx < 0) return vex_bool(false);
    return vex_bool(nn_model_save(g_ai_models[idx].model, args[1].as.string_val.data));
}

static VexValue ai_nn_load_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) return vex_int(0);
    return vex_int(ai_model_store(nn_model_load(args[0].as.string_val.data)));
}

static VexValue ai_nn_free_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_INT) return vex_bool(false);
    int idx = ai_model_find(args[0].as.int_val);
    if (idx < 0) return vex_bool(false);
    nn_model_free(g_ai_models[idx].model);
    g_ai_model_count--;
    if (idx != g_ai_model_count) g_ai_models[idx] = g_ai_models[g_ai_model_count];
    return vex_bool(true);
}

static VexValue ai_reset_builtin(VexValue* args, int argc) {
    int i;
    UNUSED(args);
    if (argc != 0) return vex_bool(false);

    for (i = 0; i < g_ai_tensor_count; i++) {
        tensor_free(g_ai_tensors[i].tensor);
        g_ai_tensors[i].tensor = NULL;
    }
    g_ai_tensor_count = 0;

    for (i = 0; i < g_ai_opt_count; i++) {
        optimizer_free(g_ai_optimizers[i].opt, g_ai_optimizers[i].opt_type);
        g_ai_optimizers[i].opt = NULL;
    }
    g_ai_opt_count = 0;

    for (i = 0; i < g_ai_model_count; i++) {
        nn_model_free(g_ai_models[i].model);
        g_ai_models[i].model = NULL;
    }
    g_ai_model_count = 0;

    return vex_bool(true);
}

static VexValue ai_nn_onnx_export_builtin(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_INT || args[1].type != VAL_STRING) return vex_bool(false);
    int idx = ai_model_find(args[0].as.int_val);
    if (idx < 0) return vex_bool(false);
    return vex_bool(nn_model_export_onnx(g_ai_models[idx].model, args[1].as.string_val.data));
}

static VexValue ai_nn_export_vxnn_builtin(VexValue* args, int argc) {
    return ai_nn_onnx_export_builtin(args, argc);
}

static VexValue ai_nn_onnx_import_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) return vex_int(0);
    NNModel* m = nn_model_import_onnx(args[0].as.string_val.data);
    if (!m) return vex_int(0);
    return vex_int(ai_model_store(m));
}

static VexValue ai_nn_import_vxnn_builtin(VexValue* args, int argc) {
    return ai_nn_onnx_import_builtin(args, argc);
}

/* ══════════════════════════════════════════════════════════════
 *  GPU MODULE (CUDA-aware, CPU fallback)
 *
 *  available, backend, device_count, dot, matmul
 * ══════════════════════════════════════════════════════════════ */

static bool gpu_is_numeric(VexValue v) {
    return v.type == VAL_INT || v.type == VAL_FLOAT;
}

typedef struct {
    char* name;
    char* entry_name;
    char* source;
    char* ptx;
} GpuKernelEntry;

typedef struct {
    int64_t handle;
    double* data;
    int64_t capacity;
    int64_t used;
    void* device_ptr;
} GpuBufferEntry;

static GpuKernelEntry* g_gpu_kernels = NULL;
static int g_gpu_kernel_count = 0;
static int g_gpu_kernel_capacity = 0;
static GpuBufferEntry* g_gpu_buffers = NULL;
static int g_gpu_buffer_count = 0;
static int g_gpu_buffer_capacity = 0;
static int64_t g_gpu_next_buffer_handle = 1;

static int gpu_buffer_find(int64_t handle) {
    for (int i = 0; i < g_gpu_buffer_count; i++) {
        if (g_gpu_buffers[i].handle == handle) return i;
    }
    return -1;
}

static int64_t gpu_buffer_alloc(int64_t capacity) {
    if (capacity <= 0) return 0;

    if (g_gpu_buffer_count >= g_gpu_buffer_capacity) {
        int new_cap = g_gpu_buffer_capacity == 0 ? 8 : g_gpu_buffer_capacity * 2;
        GpuBufferEntry* grown = (GpuBufferEntry*)realloc(g_gpu_buffers, sizeof(GpuBufferEntry) * new_cap);
        if (!grown) return 0;
        g_gpu_buffers = grown;
        g_gpu_buffer_capacity = new_cap;
    }

    GpuBufferEntry* entry = &g_gpu_buffers[g_gpu_buffer_count];
    entry->handle = g_gpu_next_buffer_handle++;
    entry->data = (double*)calloc((size_t)capacity, sizeof(double));
    if (!entry->data) return 0;
    entry->capacity = capacity;
    entry->used = 0;
    entry->device_ptr = NULL;
#ifdef USE_CUDA
    cudaError_t alloc_err = cudaMalloc(&entry->device_ptr, (size_t)capacity * sizeof(double));
    if (alloc_err != cudaSuccess) {
        entry->device_ptr = NULL;
    }
#endif
    g_gpu_buffer_count++;
    return entry->handle;
}

static bool gpu_buffer_free(int64_t handle) {
    int idx = gpu_buffer_find(handle);
    if (idx < 0) return false;

#ifdef USE_CUDA
    if (g_gpu_buffers[idx].device_ptr) {
        cudaFree(g_gpu_buffers[idx].device_ptr);
    }
#endif
    free(g_gpu_buffers[idx].data);
    g_gpu_buffer_count--;
    if (idx != g_gpu_buffer_count) {
        g_gpu_buffers[idx] = g_gpu_buffers[g_gpu_buffer_count];
    }
    return true;
}

static int gpu_kernel_find(const char* name) {
    for (int i = 0; i < g_gpu_kernel_count; i++) {
        if (strcmp(g_gpu_kernels[i].name, name) == 0) return i;
    }
    return -1;
}

static bool gpu_kernel_store(const char* name, const char* source) {
    int idx = gpu_kernel_find(name);
    if (idx >= 0) {
        free(g_gpu_kernels[idx].entry_name);
        g_gpu_kernels[idx].entry_name = safe_strdup(name);
        free(g_gpu_kernels[idx].source);
        g_gpu_kernels[idx].source = safe_strdup(source);
        free(g_gpu_kernels[idx].ptx);
        g_gpu_kernels[idx].ptx = NULL;
        return g_gpu_kernels[idx].entry_name != NULL && g_gpu_kernels[idx].source != NULL;
    }

    if (g_gpu_kernel_count >= g_gpu_kernel_capacity) {
        int new_cap = g_gpu_kernel_capacity == 0 ? 8 : g_gpu_kernel_capacity * 2;
        GpuKernelEntry* grown = (GpuKernelEntry*)realloc(g_gpu_kernels, sizeof(GpuKernelEntry) * new_cap);
        if (!grown) return false;
        g_gpu_kernels = grown;
        g_gpu_kernel_capacity = new_cap;
    }

    g_gpu_kernels[g_gpu_kernel_count].name = safe_strdup(name);
    g_gpu_kernels[g_gpu_kernel_count].entry_name = safe_strdup(name);
    g_gpu_kernels[g_gpu_kernel_count].source = safe_strdup(source);
    g_gpu_kernels[g_gpu_kernel_count].ptx = NULL;
    if (!g_gpu_kernels[g_gpu_kernel_count].name || !g_gpu_kernels[g_gpu_kernel_count].entry_name ||
        !g_gpu_kernels[g_gpu_kernel_count].source) return false;
    g_gpu_kernel_count++;
    return true;
}

static bool gpu_kernel_set_entry_name(const char* name, const char* entry_name) {
    int idx = gpu_kernel_find(name);
    if (idx < 0) return false;
    free(g_gpu_kernels[idx].entry_name);
    g_gpu_kernels[idx].entry_name = safe_strdup(entry_name);
    return g_gpu_kernels[idx].entry_name != NULL;
}

static bool gpu_kernel_set_ptx(const char* name, const char* ptx) {
    int idx = gpu_kernel_find(name);
    if (idx < 0) return false;
    free(g_gpu_kernels[idx].ptx);
    g_gpu_kernels[idx].ptx = safe_strdup(ptx);
    return g_gpu_kernels[idx].ptx != NULL;
}

#ifdef USE_NVRTC
static bool gpu_nvrtc_compile(const char* source, char** ptx_out, char** log_out) {
    nvrtcProgram prog;
    nvrtcResult r = nvrtcCreateProgram(&prog, source, "vexium_kernel.cu", 0, NULL, NULL);
    if (r != NVRTC_SUCCESS) return false;

    const char* opts[] = {"--gpu-architecture=compute_52"};
    r = nvrtcCompileProgram(prog, 1, opts);

    size_t log_size = 0;
    nvrtcGetProgramLogSize(prog, &log_size);
    if (log_size > 1 && log_out) {
        *log_out = (char*)malloc(log_size);
        if (*log_out) nvrtcGetProgramLog(prog, *log_out);
    }

    if (r == NVRTC_SUCCESS && ptx_out) {
        size_t ptx_size = 0;
        if (nvrtcGetPTXSize(prog, &ptx_size) == NVRTC_SUCCESS && ptx_size > 1) {
            *ptx_out = (char*)malloc(ptx_size);
            if (*ptx_out) {
                if (nvrtcGetPTX(prog, *ptx_out) != NVRTC_SUCCESS) {
                    free(*ptx_out);
                    *ptx_out = NULL;
                }
            }
        }
    }

    nvrtcDestroyProgram(&prog);
    return r == NVRTC_SUCCESS;
}
#endif

#if defined(USE_CUDA) && defined(USE_NVRTC)
static bool gpu_launch_ptx_kernel(const char* ptx, const char* kernel_name, int blocks, int threads,
                                  void** kernel_params) {
    CUdevice dev;
    CUcontext ctx = NULL;
    CUcontext prev_ctx = NULL;
    CUmodule mod = NULL;
    CUfunction fn;
    bool ok = false;
    bool retained_primary = false;

    if (cuInit(0) != CUDA_SUCCESS) return false;
    if (cuCtxGetCurrent(&prev_ctx) != CUDA_SUCCESS) return false;

    if (prev_ctx) {
        ctx = prev_ctx;
    } else {
        if (cuDeviceGet(&dev, 0) != CUDA_SUCCESS) return false;
        if (cuDevicePrimaryCtxRetain(&ctx, dev) != CUDA_SUCCESS) return false;
        retained_primary = true;
        if (cuCtxSetCurrent(ctx) != CUDA_SUCCESS) goto cleanup;
    }

    if (cuModuleLoadDataEx(&mod, ptx, 0, NULL, NULL) != CUDA_SUCCESS) goto cleanup;
    if (cuModuleGetFunction(&fn, mod, kernel_name) != CUDA_SUCCESS) goto cleanup;
    if (cuLaunchKernel(fn, (unsigned int)blocks, 1, 1, (unsigned int)threads, 1, 1,
                       0, NULL, kernel_params, NULL) != CUDA_SUCCESS) {
        goto cleanup;
    }
    if (cuCtxSynchronize() != CUDA_SUCCESS) goto cleanup;
    ok = true;

cleanup:
    if (mod) cuModuleUnload(mod);
    if (retained_primary && prev_ctx) {
        cuCtxSetCurrent(prev_ctx);
    }
    if (retained_primary && ctx) {
        cuDevicePrimaryCtxRelease(dev);
    }
    return ok;
}

static bool gpu_prepare_kernel_params(ValueArray* launch_args,
                                      void*** kernel_params_out,
                                      CUdeviceptr** ptr_storage_out,
                                      int64_t** int_storage_out,
                                      double** float_storage_out,
                                      int* out_count) {
    int count = launch_args ? launch_args->count : 0;
    *kernel_params_out = NULL;
    *ptr_storage_out = NULL;
    *int_storage_out = NULL;
    *float_storage_out = NULL;
    *out_count = count;
    if (count == 0) return true;

    void** kernel_params = (void**)calloc((size_t)count, sizeof(void*));
    CUdeviceptr* ptr_storage = (CUdeviceptr*)calloc((size_t)count, sizeof(CUdeviceptr));
    int64_t* int_storage = (int64_t*)calloc((size_t)count, sizeof(int64_t));
    double* float_storage = (double*)calloc((size_t)count, sizeof(double));
    if (!kernel_params || !ptr_storage || !int_storage || !float_storage) {
        free(kernel_params);
        free(ptr_storage);
        free(int_storage);
        free(float_storage);
        return false;
    }

    for (int i = 0; i < count; i++) {
        VexValue v = launch_args->items[i];
        if (v.type == VAL_INT) {
            int buf_idx = gpu_buffer_find(v.as.int_val);
            if (buf_idx >= 0 && g_gpu_buffers[buf_idx].device_ptr) {
                ptr_storage[i] = (CUdeviceptr)(uintptr_t)g_gpu_buffers[buf_idx].device_ptr;
                kernel_params[i] = &ptr_storage[i];
            } else {
                int_storage[i] = v.as.int_val;
                kernel_params[i] = &int_storage[i];
            }
            continue;
        }
        if (v.type == VAL_FLOAT) {
            float_storage[i] = v.as.float_val;
            kernel_params[i] = &float_storage[i];
            continue;
        }

        free(kernel_params);
        free(ptr_storage);
        free(int_storage);
        free(float_storage);
        return false;
    }

    *kernel_params_out = kernel_params;
    *ptr_storage_out = ptr_storage;
    *int_storage_out = int_storage;
    *float_storage_out = float_storage;
    return true;
}
#endif

static double gpu_to_double(VexValue v) {
    if (v.type == VAL_INT) return (double)v.as.int_val;
    if (v.type == VAL_FLOAT) return v.as.float_val;
    return 0.0;
}

static VexValue gpu_available_builtin(VexValue* args, int argc) {
    UNUSED(args);
    UNUSED(argc);
#ifdef USE_CUDA
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    return vex_bool(err == cudaSuccess && count > 0);
#else
    return vex_bool(false);
#endif
}

static VexValue gpu_backend_builtin(VexValue* args, int argc) {
    UNUSED(args);
    UNUSED(argc);
#ifdef USE_CUDA
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    if (err == cudaSuccess && count > 0) return vex_string("cuda", 4);
#endif
    return vex_string("cpu", 3);
}

static VexValue gpu_device_count_builtin(VexValue* args, int argc) {
    UNUSED(args);
    UNUSED(argc);
#ifdef USE_CUDA
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    if (err != cudaSuccess) return vex_int(0);
    return vex_int((int64_t)count);
#else
    return vex_int(0);
#endif
}

static VexValue gpu_dot_builtin(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_ARRAY ||
        !args[0].as.array_val || !args[1].as.array_val) {
        fprintf(stderr, "Error: dot() expects 2 arrays\n");
        return vex_nothing();
    }

    ValueArray* a = args[0].as.array_val;
    ValueArray* b = args[1].as.array_val;
    if (a->count != b->count) {
        fprintf(stderr, "Error: dot() requires arrays of equal length\n");
        return vex_nothing();
    }

    double sum = 0.0;
    bool all_int = true;
    for (int i = 0; i < a->count; i++) {
        if (!gpu_is_numeric(a->items[i]) || !gpu_is_numeric(b->items[i])) {
            fprintf(stderr, "Error: dot() requires numeric array elements\n");
            return vex_nothing();
        }
        if (a->items[i].type != VAL_INT || b->items[i].type != VAL_INT) all_int = false;
        sum += gpu_to_double(a->items[i]) * gpu_to_double(b->items[i]);
    }
    if (all_int) return vex_int((int64_t)sum);
    return vex_float(sum);
}

static VexValue gpu_matmul_builtin(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_ARRAY ||
        !args[0].as.array_val || !args[1].as.array_val) {
        fprintf(stderr, "Error: matmul() expects 2 matrix arrays\n");
        return vex_nothing();
    }

    ValueArray* a_rows = args[0].as.array_val;
    ValueArray* b_rows = args[1].as.array_val;
    if (a_rows->count == 0 || b_rows->count == 0) {
        fprintf(stderr, "Error: matmul() requires non-empty matrices\n");
        return vex_nothing();
    }

    if (a_rows->items[0].type != VAL_ARRAY || b_rows->items[0].type != VAL_ARRAY ||
        !a_rows->items[0].as.array_val || !b_rows->items[0].as.array_val) {
        fprintf(stderr, "Error: matmul() expects 2D array matrices\n");
        return vex_nothing();
    }

    int m = a_rows->count;
    int k = a_rows->items[0].as.array_val->count;
    int k2 = b_rows->count;
    int n = b_rows->items[0].as.array_val->count;
    if (k != k2) {
        fprintf(stderr, "Error: matmul() dimension mismatch\n");
        return vex_nothing();
    }

    VexValue out;
    out.type = VAL_ARRAY;
    out.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));

    for (int i = 0; i < m; i++) {
        if (a_rows->items[i].type != VAL_ARRAY || !a_rows->items[i].as.array_val ||
            a_rows->items[i].as.array_val->count != k) {
            fprintf(stderr, "Error: matmul() left matrix row shape mismatch\n");
            return vex_nothing();
        }

        VexValue row_val;
        row_val.type = VAL_ARRAY;
        row_val.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));

        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            bool all_int = true;
            for (int t = 0; t < k; t++) {
                if (b_rows->items[t].type != VAL_ARRAY || !b_rows->items[t].as.array_val ||
                    b_rows->items[t].as.array_val->count != n) {
                    fprintf(stderr, "Error: matmul() right matrix row shape mismatch\n");
                    return vex_nothing();
                }

                VexValue av = a_rows->items[i].as.array_val->items[t];
                VexValue bv = b_rows->items[t].as.array_val->items[j];
                if (!gpu_is_numeric(av) || !gpu_is_numeric(bv)) {
                    fprintf(stderr, "Error: matmul() requires numeric matrix elements\n");
                    return vex_nothing();
                }
                if (av.type != VAL_INT || bv.type != VAL_INT) all_int = false;
                sum += gpu_to_double(av) * gpu_to_double(bv);
            }

            ValueArray* row = row_val.as.array_val;
            if (row->count >= row->capacity) {
                row->capacity = row->capacity == 0 ? 8 : row->capacity * 2;
                row->items = (VexValue*)realloc(row->items, sizeof(VexValue) * row->capacity);
            }
            row->items[row->count++] = all_int ? vex_int((int64_t)sum) : vex_float(sum);
        }

        ValueArray* out_rows = out.as.array_val;
        if (out_rows->count >= out_rows->capacity) {
            out_rows->capacity = out_rows->capacity == 0 ? 8 : out_rows->capacity * 2;
            out_rows->items = (VexValue*)realloc(out_rows->items, sizeof(VexValue) * out_rows->capacity);
        }
        out_rows->items[out_rows->count++] = row_val;
    }

    return out;
}

static VexValue gpu_kernel_compile_builtin(VexValue* args, int argc) {
    const char* name = NULL;
    const char* entry_name = NULL;
    const char* source = NULL;
    if (argc == 1 && args[0].type == VAL_STRING) {
        name = "default_kernel";
        entry_name = "default_kernel";
        source = args[0].as.string_val.data;
    } else if (argc == 2 && args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
        name = args[0].as.string_val.data;
        entry_name = name;
        source = args[1].as.string_val.data;
    } else if (argc == 3 && args[0].type == VAL_STRING && args[1].type == VAL_STRING && args[2].type == VAL_STRING) {
        name = args[0].as.string_val.data;
        entry_name = args[1].as.string_val.data;
        source = args[2].as.string_val.data;
    } else {
        fprintf(stderr, "Error: kernel_compile() expects (source), (name, source), or (name, entry, source)\n");
        return vex_bool(false);
    }

    if (source[0] == '\0' || name[0] == '\0' || entry_name[0] == '\0') return vex_bool(false);
    if (!gpu_kernel_store(name, source)) return vex_bool(false);
    if (!gpu_kernel_set_entry_name(name, entry_name)) return vex_bool(false);

#if defined(USE_CUDA) && defined(USE_NVRTC)
    /* Compile real CUDA kernels when source looks like kernel code. */
    if (strstr(source, "__global__") != NULL || strstr(source, "extern") != NULL) {
        char* ptx = NULL;
        char* log = NULL;
        bool ok = gpu_nvrtc_compile(source, &ptx, &log);
        if (!ok && log) {
            fprintf(stderr, "[gpu] NVRTC compile failed for '%s':\n%s\n", name, log);
        }
        if (ok && ptx) {
            if (!gpu_kernel_set_ptx(name, ptx)) ok = false;
        }
        free(ptx);
        free(log);
        return vex_bool(ok);
    }
    return vex_bool(true);
#else
    /* CPU fallback still supports development-time compile/launch flow. */
    return vex_bool(true);
#endif
}

static VexValue gpu_kernel_launch_builtin(VexValue* args, int argc) {
    if ((argc != 3 && argc != 4) || args[0].type != VAL_STRING || args[1].type != VAL_INT || args[2].type != VAL_INT) {
        fprintf(stderr, "Error: kernel_launch() expects (name, blocks, threads[, args])\n");
        return vex_bool(false);
    }

    const char* name = args[0].as.string_val.data;
    int64_t blocks = args[1].as.int_val;
    int64_t threads = args[2].as.int_val;
    if (blocks <= 0 || threads <= 0) return vex_bool(false);
    int kernel_idx = gpu_kernel_find(name);
    if (kernel_idx < 0) return vex_bool(false);

    ValueArray* launch_args = NULL;
    if (argc == 4) {
        if (args[3].type != VAL_ARRAY || !args[3].as.array_val) return vex_bool(false);
        launch_args = args[3].as.array_val;
    }

#if defined(USE_CUDA) && defined(USE_NVRTC)
    if (g_gpu_kernels[kernel_idx].ptx) {
        const char* entry_name = g_gpu_kernels[kernel_idx].entry_name ? g_gpu_kernels[kernel_idx].entry_name : name;
        void** kernel_params = NULL;
        CUdeviceptr* ptr_storage = NULL;
        int64_t* int_storage = NULL;
        double* float_storage = NULL;
        int param_count = 0;
        if (!gpu_prepare_kernel_params(launch_args, &kernel_params, &ptr_storage, &int_storage, &float_storage, &param_count)) {
            return vex_bool(false);
        }
        bool ok = gpu_launch_ptx_kernel(g_gpu_kernels[kernel_idx].ptx, entry_name, (int)blocks, (int)threads,
                                        param_count > 0 ? kernel_params : NULL);
        free(kernel_params);
        free(ptr_storage);
        free(int_storage);
        free(float_storage);
        return vex_bool(ok);
    }
    return vex_bool(true);
#elif defined(USE_CUDA)
    return vex_bool(true);
#else
    return vex_bool(true);
#endif
}

static VexValue gpu_alloc_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_INT) {
        fprintf(stderr, "Error: alloc() expects (size)\n");
        return vex_int(0);
    }
    return vex_int(gpu_buffer_alloc(args[0].as.int_val));
}

static VexValue gpu_free_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_INT) {
        fprintf(stderr, "Error: free() expects (handle)\n");
        return vex_bool(false);
    }
    return vex_bool(gpu_buffer_free(args[0].as.int_val));
}

static VexValue gpu_write_builtin(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_INT || args[1].type != VAL_ARRAY || !args[1].as.array_val) {
        fprintf(stderr, "Error: write() expects (handle, array)\n");
        return vex_bool(false);
    }

    int idx = gpu_buffer_find(args[0].as.int_val);
    if (idx < 0) return vex_bool(false);

    GpuBufferEntry* entry = &g_gpu_buffers[idx];
    ValueArray* arr = args[1].as.array_val;
    if ((int64_t)arr->count > entry->capacity) return vex_bool(false);

    for (int i = 0; i < arr->count; i++) {
        if (!gpu_is_numeric(arr->items[i])) return vex_bool(false);
        entry->data[i] = gpu_to_double(arr->items[i]);
    }
    entry->used = arr->count;
#ifdef USE_CUDA
    if (entry->device_ptr && entry->used > 0) {
        cudaError_t err = cudaMemcpy(entry->device_ptr, entry->data,
                                     (size_t)entry->used * sizeof(double), cudaMemcpyHostToDevice);
        if (err != cudaSuccess) return vex_bool(false);
    }
#endif
    return vex_bool(true);
}

static VexValue gpu_read_builtin(VexValue* args, int argc) {
    if (argc != 1 && argc != 2) {
        fprintf(stderr, "Error: read() expects (handle[, count])\n");
        return vex_nothing();
    }
    if (args[0].type != VAL_INT) return vex_nothing();

    int idx = gpu_buffer_find(args[0].as.int_val);
    if (idx < 0) return vex_nothing();

    GpuBufferEntry* entry = &g_gpu_buffers[idx];
    int64_t count = entry->used;
    if (argc == 2) {
        if (args[1].type != VAL_INT || args[1].as.int_val < 0) return vex_nothing();
        count = args[1].as.int_val;
    }
    if (count > entry->used) count = entry->used;
    if (count > entry->capacity) count = entry->capacity;

#ifdef USE_CUDA
    if (entry->device_ptr && count > 0) {
        cudaError_t err = cudaMemcpy(entry->data, entry->device_ptr,
                                     (size_t)count * sizeof(double), cudaMemcpyDeviceToHost);
        if (err != cudaSuccess) return vex_nothing();
    }
#endif

    VexValue out;
    out.type = VAL_ARRAY;
    out.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
    if (!out.as.array_val) return vex_nothing();

    for (int64_t i = 0; i < count; i++) {
        ValueArray* out_arr = out.as.array_val;
        if (out_arr->count >= out_arr->capacity) {
            out_arr->capacity = out_arr->capacity == 0 ? 8 : out_arr->capacity * 2;
            out_arr->items = (VexValue*)realloc(out_arr->items, sizeof(VexValue) * out_arr->capacity);
            if (!out_arr->items) return vex_nothing();
        }
        out_arr->items[out_arr->count++] = vex_float(entry->data[i]);
    }

    return out;
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

/* ══════════════════════════════════════════════════════════════
 *  DATA MODULE (CSV, splits, shuffle)
 * ══════════════════════════════════════════════════════════════ */

/* Helper: parse one CSV field from line[pos..len], returns new pos */
static int data_csv_field(const char* line, int pos, int len, char* buf, int bufsz) {
    int j = 0;
    if (pos < len && line[pos] == '"') {
        pos++;
        while (pos < len && j < bufsz - 1) {
            if (line[pos] == '"') {
                if (pos + 1 < len && line[pos + 1] == '"') { buf[j++] = '"'; pos += 2; }
                else { pos++; break; }
            } else { buf[j++] = line[pos++]; }
        }
    } else {
        while (pos < len && line[pos] != ',' && j < bufsz - 1) buf[j++] = line[pos++];
    }
    buf[j] = '\0';
    if (pos < len && line[pos] == ',') pos++;
    return pos;
}

static VexValue data_csv_coerce(const char* s) {
    if (!s || !*s) return vex_string(s ? s : "", 0);
    char* end;
    double d = strtod(s, &end);
    if (end != s && *end == '\0') {
        if (d == (double)(int64_t)d && !strchr(s, '.'))
            return vex_int((int64_t)d);
        return vex_float(d);
    }
    return vex_string(s, (int)strlen(s));
}

static VexValue data_csv_read_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) return vex_nothing();
    const char* path = args[0].as.string_val.data;
    FILE* fp = fopen(path, "r");
    if (!fp) return vex_nothing();

    VexValue result;
    result.type = VAL_ARRAY;
    result.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
    if (!result.as.array_val) { fclose(fp); return vex_nothing(); }

    char line[8192];
    /* Header row */
    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return result; }
    int ll = (int)strlen(line);
    while (ll > 0 && (line[ll-1] == '\n' || line[ll-1] == '\r')) line[--ll] = '\0';

    char** hdr = NULL; int ncols = 0;
    { int pos = 0; char fld[512];
      while (pos <= ll) {
          pos = data_csv_field(line, pos, ll, fld, sizeof(fld));
          hdr = (char**)realloc(hdr, sizeof(char*) * (ncols + 1));
          hdr[ncols++] = safe_strdup(fld);
          if (pos >= ll) break;
      }
    }

    while (fgets(line, sizeof(line), fp)) {
        ll = (int)strlen(line);
        while (ll > 0 && (line[ll-1] == '\n' || line[ll-1] == '\r')) line[--ll] = '\0';
        if (ll == 0) continue;
        VexValue row; row.type = VAL_MAP;
        row.as.map_val = (ValueMap*)calloc(1, sizeof(ValueMap));
        int pos = 0, col = 0; char fld[8192];
        while (col < ncols) {
            pos = data_csv_field(line, pos, ll, fld, sizeof(fld));
            std_json_map_set(row.as.map_val, hdr[col], data_csv_coerce(fld));
            col++;
            if (pos >= ll) break;
        }
        ValueArray* arr = result.as.array_val;
        if (arr->count >= arr->capacity) {
            arr->capacity = arr->capacity == 0 ? 16 : arr->capacity * 2;
            arr->items = (VexValue*)realloc(arr->items, sizeof(VexValue) * arr->capacity);
        }
        arr->items[arr->count++] = row;
    }
    for (int i = 0; i < ncols; i++) free(hdr[i]);
    free(hdr); fclose(fp);
    return result;
}

static VexValue data_csv_write_builtin(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_ARRAY || !args[1].as.array_val)
        return vex_bool(false);
    const char* path = args[0].as.string_val.data;
    ValueArray* rows = args[1].as.array_val;
    if (rows->count == 0) return vex_bool(true);
    FILE* fp = fopen(path, "w");
    if (!fp) return vex_bool(false);
    if (rows->items[0].type == VAL_MAP && rows->items[0].as.map_val) {
        ValueMap* first = rows->items[0].as.map_val;
        for (int i = 0; i < first->count; i++) {
            if (i > 0) fputc(',', fp);
            fprintf(fp, "%s", first->entries[i].key);
        }
        fputc('\n', fp);
        for (int r = 0; r < rows->count; r++) {
            if (rows->items[r].type != VAL_MAP || !rows->items[r].as.map_val) continue;
            ValueMap* row = rows->items[r].as.map_val;
            for (int c = 0; c < first->count; c++) {
                if (c > 0) fputc(',', fp);
                VexValue v = vex_nothing();
                for (int k = 0; k < row->count; k++) {
                    if (strcmp(row->entries[k].key, first->entries[c].key) == 0) {
                        v = row->entries[k].value; break;
                    }
                }
                if (v.type == VAL_STRING)      fprintf(fp, "%s", v.as.string_val.data);
                else if (v.type == VAL_INT)    fprintf(fp, "%lld", (long long)v.as.int_val);
                else if (v.type == VAL_FLOAT)  fprintf(fp, "%g", v.as.float_val);
                else if (v.type == VAL_BOOL)   fprintf(fp, "%s", v.as.bool_val ? "true" : "false");
            }
            fputc('\n', fp);
        }
    }
    fclose(fp);
    return vex_bool(true);
}

static VexValue data_split_builtin(VexValue* args, int argc) {
    if (argc < 1 || args[0].type != VAL_ARRAY || !args[0].as.array_val) return vex_nothing();
    ValueArray* src = args[0].as.array_val;
    double ratio = 0.8;
    if (argc >= 2) {
        if (args[1].type == VAL_FLOAT) ratio = args[1].as.float_val;
        else if (args[1].type == VAL_INT) ratio = (double)args[1].as.int_val;
    }
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    int train_n = (int)(src->count * ratio);
    int test_n  = src->count - train_n;

    VexValue ta, va;
    ta.type = VAL_ARRAY; ta.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
    va.type = VAL_ARRAY; va.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
    if (train_n > 0) {
        ta.as.array_val->capacity = ta.as.array_val->count = train_n;
        ta.as.array_val->items = (VexValue*)malloc(sizeof(VexValue) * train_n);
        memcpy(ta.as.array_val->items, src->items, sizeof(VexValue) * train_n);
    }
    if (test_n > 0) {
        va.as.array_val->capacity = va.as.array_val->count = test_n;
        va.as.array_val->items = (VexValue*)malloc(sizeof(VexValue) * test_n);
        memcpy(va.as.array_val->items, src->items + train_n, sizeof(VexValue) * test_n);
    }

    VexValue res; res.type = VAL_ARRAY;
    res.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
    res.as.array_val->capacity = res.as.array_val->count = 2;
    res.as.array_val->items = (VexValue*)malloc(sizeof(VexValue) * 2);
    res.as.array_val->items[0] = ta;
    res.as.array_val->items[1] = va;
    return res;
}

static VexValue data_shuffle_builtin(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_ARRAY || !args[0].as.array_val) return vex_nothing();
    ValueArray* src = args[0].as.array_val;
    if (src->count == 0) return args[0];
    VexValue res; res.type = VAL_ARRAY;
    res.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
    res.as.array_val->capacity = res.as.array_val->count = src->count;
    res.as.array_val->items = (VexValue*)malloc(sizeof(VexValue) * src->count);
    memcpy(res.as.array_val->items, src->items, sizeof(VexValue) * src->count);
    VexValue* a = res.as.array_val->items;
    for (int i = (int)src->count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        VexValue tmp = a[i]; a[i] = a[j]; a[j] = tmp;
    }
    return res;
}

static StdlibEntry data_entries[] = {
    {"csv_read",  data_csv_read_builtin},
    {"csv_write", data_csv_write_builtin},
    {"split",     data_split_builtin},
    {"shuffle",   data_shuffle_builtin},
    {NULL, NULL}
};

/* ── Transformer primitives ── */
static VexValue ai_tensor_attention_builtin(VexValue* args, int argc) {
    if (argc != 3 || args[0].type != VAL_INT || args[1].type != VAL_INT || args[2].type != VAL_INT) return vex_int(0);
    int qi = ai_tensor_find(args[0].as.int_val);
    int ki = ai_tensor_find(args[1].as.int_val);
    int vi = ai_tensor_find(args[2].as.int_val);
    if (qi < 0 || ki < 0 || vi < 0) return vex_int(0);
    Tensor* qt = g_ai_tensors[qi].tensor;
    Tensor* kt = g_ai_tensors[ki].tensor;
    Tensor* vt = g_ai_tensors[vi].tensor;
    Tensor* k_t = tensor_transpose(kt);
    if (!k_t) return vex_int(0);
    Tensor* scores = tensor_matmul(qt, k_t);
    tensor_free(k_t);
    if (!scores) return vex_int(0);
    int dk = qt->shape[qt->ndim - 1];
    tensor_imul_scalar(scores, 1.0 / sqrt((double)(dk > 0 ? dk : 1)));
    Tensor* probs = tensor_softmax(scores);
    tensor_free(scores);
    if (!probs) return vex_int(0);
    Tensor* out = tensor_matmul(probs, vt);
    tensor_free(probs);
    return vex_int(ai_tensor_store(out));
}

static VexValue ai_tensor_layernorm_builtin(VexValue* args, int argc) {
    if (argc < 1 || args[0].type != VAL_INT) return vex_int(0);
    float eps = 1e-5f;
    if (argc >= 2) {
        if (args[1].type == VAL_FLOAT) eps = (float)args[1].as.float_val;
        else if (args[1].type == VAL_INT) eps = (float)args[1].as.int_val;
    }
    int idx = ai_tensor_find(args[0].as.int_val);
    if (idx < 0) return vex_int(0);
    Tensor* t = g_ai_tensors[idx].tensor;
    Tensor* out = tensor_clone(t);
    if (!out) return vex_int(0);
    int last_dim = t->shape[t->ndim - 1];
    int n_rows = (int)(t->size / (last_dim > 0 ? last_dim : 1));
    for (int r = 0; r < n_rows; r++) {
        float* row = out->data.f32_data + r * last_dim;
        float mean = 0.0f;
        for (int i = 0; i < last_dim; i++) mean += row[i];
        mean /= (float)last_dim;
        float var = 0.0f;
        for (int i = 0; i < last_dim; i++) { float d = row[i] - mean; var += d * d; }
        var /= (float)last_dim;
        float inv_std = 1.0f / sqrtf(var + eps);
        for (int i = 0; i < last_dim; i++) row[i] = (row[i] - mean) * inv_std;
    }
    return vex_int(ai_tensor_store(out));
}

static VexValue ai_embedding_lookup_builtin(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_INT || args[1].type != VAL_INT) return vex_int(0);
    int ti = ai_tensor_find(args[0].as.int_val);
    if (ti < 0) return vex_int(0);
    Tensor* tbl = g_ai_tensors[ti].tensor;
    if (tbl->ndim < 2) return vex_int(0);
    int64_t tok = args[1].as.int_val;
    if (tok < 0 || tok >= tbl->shape[0]) return vex_int(0);
    int embed_dim = tbl->shape[1];
    int shape1[1] = { embed_dim };
    Tensor* row_t = tensor_new(TENSOR_FLOAT32, 1, shape1);
    if (!row_t) return vex_int(0);
    memcpy(row_t->data.f32_data, tbl->data.f32_data + (int)tok * embed_dim, sizeof(float) * embed_dim);
    return vex_int(ai_tensor_store(row_t));
}

static StdlibEntry ai_entries[] = {
    {"tensor_zeros",     ai_tensor_zeros_builtin},
    {"tensor_ones",      ai_tensor_ones_builtin},
    {"tensor_rand",      ai_tensor_rand_builtin},
    {"tensor_randn",     ai_tensor_randn_builtin},
    {"tensor_fill",      ai_tensor_fill_builtin},
    {"tensor_from_array",ai_tensor_from_array_builtin},
    {"tensor_to_array",  ai_tensor_to_array_builtin},
    {"tensor_clone",     ai_tensor_clone_builtin},
    {"tensor_matmul",    ai_tensor_matmul_builtin},
    {"tensor_add",       ai_tensor_add_builtin},
    {"tensor_sub",       ai_tensor_sub_builtin},
    {"tensor_mul",       ai_tensor_mul_builtin},
    {"tensor_div",       ai_tensor_div_builtin},
    {"tensor_scale",     ai_tensor_scale_builtin},
    {"tensor_relu",      ai_tensor_relu_builtin},
    {"tensor_sigmoid",   ai_tensor_sigmoid_builtin},
    {"tensor_tanh",      ai_tensor_tanh_builtin},
    {"tensor_softmax",   ai_tensor_softmax_builtin},
    {"tensor_reshape",   ai_tensor_reshape_builtin},
    {"tensor_transpose", ai_tensor_transpose_builtin},
    {"tensor_sum",       ai_tensor_sum_builtin},
    {"tensor_mean",      ai_tensor_mean_builtin},
    {"tensor_std",       ai_tensor_std_builtin},
    {"tensor_max",       ai_tensor_max_builtin},
    {"tensor_min",       ai_tensor_min_builtin},
    {"tensor_shape",     ai_tensor_shape_builtin},
    {"tensor_free",      ai_tensor_free_builtin},
    {"optimizer_create", ai_optimizer_create_builtin},
    {"optimizer_step",   ai_optimizer_step_builtin},
    {"optimizer_free",   ai_optimizer_free_builtin},
    {"nn_new",           ai_nn_new_builtin},
    {"nn_add_dense",     ai_nn_add_dense_builtin},
    {"nn_add_conv2d",    ai_nn_add_conv2d_builtin},
    {"nn_add_dropout",   ai_nn_add_dropout_builtin},
    {"nn_add_maxpool2d", ai_nn_add_maxpool2d_builtin},
    {"nn_forward",       ai_nn_forward_builtin},
    {"nn_predict",       ai_nn_predict_builtin},
    {"nn_train_step",    ai_nn_train_step_builtin},
    {"nn_set_lr",        ai_nn_set_lr_builtin},
    {"nn_get_lr",        ai_nn_get_lr_builtin},
    {"nn_save",          ai_nn_save_builtin},
    {"nn_load",          ai_nn_load_builtin},
    {"nn_free",          ai_nn_free_builtin},
    {"reset",            ai_reset_builtin},
    {"nn_onnx_export",   ai_nn_onnx_export_builtin},
    {"nn_onnx_import",   ai_nn_onnx_import_builtin},
    {"nn_export_vxnn",   ai_nn_export_vxnn_builtin},
    {"nn_import_vxnn",   ai_nn_import_vxnn_builtin},
    {"tokenize",         ai_tokenize_builtin},
    {"attention",        ai_tensor_attention_builtin},
    {"layernorm",        ai_tensor_layernorm_builtin},
    {"embedding_lookup", ai_embedding_lookup_builtin},
    {NULL, NULL}
};

static StdlibEntry gpu_entries[] = {
    {"available",    gpu_available_builtin},
    {"backend",      gpu_backend_builtin},
    {"device_count", gpu_device_count_builtin},
    {"dot",          gpu_dot_builtin},
    {"matmul",       gpu_matmul_builtin},
    {"kernel_compile", gpu_kernel_compile_builtin},
    {"kernel_launch",  gpu_kernel_launch_builtin},
    {"alloc",          gpu_alloc_builtin},
    {"free",           gpu_free_builtin},
    {"write",          gpu_write_builtin},
    {"read",           gpu_read_builtin},
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
    {"data",        data_entries},
    {"gpu",         gpu_entries},
    {"ai",          ai_entries},
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
