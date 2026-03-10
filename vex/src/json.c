#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* ══════════════════════════════════════════════════════════════
 *  JSON Parser State
 * ══════════════════════════════════════════════════════════════ */

typedef struct {
    const char* start;
    const char* current;
    int line;
} JsonParser;

static void json_init_parser(JsonParser* parser, const char* json) {
    parser->start = json;
    parser->current = json;
    parser->line = 1;
}

static void json_skip_whitespace(JsonParser* parser) {
    while (true) {
        char c = *parser->current;
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                parser->current++;
                break;
            case '\n':
                parser->line++;
                parser->current++;
                break;
            default:
                return;
        }
    }
}

static bool json_match(JsonParser* parser, char expected) {
    if (*parser->current == expected) {
        parser->current++;
        return true;
    }
    return false;
}

static char json_peek(JsonParser* parser) {
    return *parser->current;
}

static char json_advance(JsonParser* parser) {
    return *parser->current++;
}

/* Forward declarations */
static Value json_parse_value(JsonParser* parser);

/* Parse a JSON string (handles escape sequences) */
static Value json_parse_string(JsonParser* parser) {
    /* Consume opening quote */
    json_advance(parser);
    
    const char* start = parser->current;
    size_t len = 0;
    
    /* First pass: calculate length */
    while (json_peek(parser) != '"' && json_peek(parser) != '\0') {
        if (json_peek(parser) == '\\') {
            json_advance(parser);
            if (json_peek(parser) != '\0') json_advance(parser);
        } else {
            json_advance(parser);
        }
        len++;
    }
    
    /* Allocate and copy */
    char* result = (char*)malloc(len + 1);
    if (!result) return val_nothing_v();
    
    parser->current = start;
    size_t i = 0;
    while (json_peek(parser) != '"' && json_peek(parser) != '\0') {
        if (json_peek(parser) == '\\') {
            json_advance(parser);
            char escape = json_advance(parser);
            switch (escape) {
                case '"': result[i++] = '"'; break;
                case '\\': result[i++] = '\\'; break;
                case '/': result[i++] = '/'; break;
                case 'b': result[i++] = '\b'; break;
                case 'f': result[i++] = '\f'; break;
                case 'n': result[i++] = '\n'; break;
                case 'r': result[i++] = '\r'; break;
                case 't': result[i++] = '\t'; break;
                case 'u': {
                    /* Unicode escape - simplified: just copy as-is */
                    result[i++] = '\\';
                    result[i++] = 'u';
                    for (int j = 0; j < 4 && isxdigit(json_peek(parser)); j++) {
                        result[i++] = json_advance(parser);
                    }
                    break;
                }
                default: result[i++] = escape; break;
            }
        } else {
            result[i++] = json_advance(parser);
        }
    }
    result[i] = '\0';
    
    /* Consume closing quote */
    if (json_peek(parser) == '"') json_advance(parser);
    
    /* Create ObjString */
    ObjString* str = obj_string_new(result, (int)i);
    free(result);
    return val_obj(str);
}

/* Parse a JSON number */
static Value json_parse_number(JsonParser* parser) {
    const char* start = parser->current;
    
    /* Optional minus */
    if (json_peek(parser) == '-') json_advance(parser);
    
    /* Integer part */
    while (isdigit(json_peek(parser))) json_advance(parser);
    
    /* Fractional part */
    if (json_peek(parser) == '.' && isdigit(parser->current[1])) {
        json_advance(parser);
        while (isdigit(json_peek(parser))) json_advance(parser);
    }
    
    /* Exponent */
    if (json_peek(parser) == 'e' || json_peek(parser) == 'E') {
        json_advance(parser);
        if (json_peek(parser) == '+' || json_peek(parser) == '-') json_advance(parser);
        while (isdigit(json_peek(parser))) json_advance(parser);
    }
    
    /* Convert to number */
    size_t len = parser->current - start;
    char* num_str = (char*)malloc(len + 1);
    memcpy(num_str, start, len);
    num_str[len] = '\0';
    
    /* Try integer first */
    char* end;
    long long int_val = strtoll(num_str, &end, 10);
    if (*end == '\0') {
        free(num_str);
        return val_int((int32_t)int_val);
    }
    
    /* Float */
    double float_val = strtod(num_str, &end);
    free(num_str);
    return val_number(float_val);
}

/* Parse a JSON array */
static Value json_parse_array(JsonParser* parser) {
    json_advance(parser); /* consume '[' */
    json_skip_whitespace(parser);
    
    ObjArray* arr = obj_array_new(8);
    
    if (json_peek(parser) == ']') {
        json_advance(parser);
        return val_obj(arr);
    }
    
    while (true) {
        json_skip_whitespace(parser);
        Value elem = json_parse_value(parser);
        /* Append to array - need to grow if needed */
        if (arr->count >= arr->capacity) {
            arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
            arr->items = (Value*)realloc(arr->items, sizeof(Value) * arr->capacity);
        }
        arr->items[arr->count++] = elem;
        
        json_skip_whitespace(parser);
        if (json_match(parser, ',')) {
            continue;
        } else if (json_match(parser, ']')) {
            break;
        } else {
            /* Error: expected , or ] */
            break;
        }
    }
    
    return val_obj(arr);
}

/* Parse a JSON object */
static Value json_parse_object(JsonParser* parser) {
    json_advance(parser); /* consume '{' */
    json_skip_whitespace(parser);
    
    ObjMap* map = obj_map_new();
    
    if (json_peek(parser) == '}') {
        json_advance(parser);
        return val_obj(map);
    }
    
    while (true) {
        json_skip_whitespace(parser);
        
        /* Parse key (must be string) */
        if (json_peek(parser) != '"') {
            /* Error: expected string key */
            break;
        }
        
        Value key = json_parse_string(parser);
        
        json_skip_whitespace(parser);
        if (!json_match(parser, ':')) {
            /* Error: expected : */
            break;
        }
        
        json_skip_whitespace(parser);
        Value value = json_parse_value(parser);
        
        /* Insert into map */
        if (is_obj(key) && ((Obj*)as_obj(key))->type == OBJ_STRING) {
            obj_map_set(map, (ObjString*)as_obj(key), value);
        }
        
        json_skip_whitespace(parser);
        if (json_match(parser, ',')) {
            continue;
        } else if (json_match(parser, '}')) {
            break;
        } else {
            /* Error: expected , or } */
            break;
        }
    }
    
    return val_obj(map);
}

/* Parse a JSON value */
static Value json_parse_value(JsonParser* parser) {
    json_skip_whitespace(parser);
    
    char c = json_peek(parser);
    
    switch (c) {
        case '"': return json_parse_string(parser);
        case '[': return json_parse_array(parser);
        case '{': return json_parse_object(parser);
        case 't':
            if (strncmp(parser->current, "true", 4) == 0) {
                parser->current += 4;
                return val_bool(true);
            }
            break;
        case 'f':
            if (strncmp(parser->current, "false", 5) == 0) {
                parser->current += 5;
                return val_bool(false);
            }
            break;
        case 'n':
            if (strncmp(parser->current, "null", 4) == 0) {
                parser->current += 4;
                return val_nothing_v();
            }
            break;
        default:
            if (c == '-' || isdigit(c)) {
                return json_parse_number(parser);
            }
            break;
    }
    
    return val_nothing_v();
}

/* ══════════════════════════════════════════════════════════════
 *  Public API
 * ══════════════════════════════════════════════════════════════ */

Value json_parse_string_value(const char* json_str) {
    JsonParser parser;
    json_init_parser(&parser, json_str);
    return json_parse_value(&parser);
}

/* ══════════════════════════════════════════════════════════════
 *  JSON Stringification
 * ══════════════════════════════════════════════════════════════ */

typedef struct {
    char* buffer;
    size_t length;
    size_t capacity;
} JsonBuilder;

static void json_builder_init(JsonBuilder* builder) {
    builder->capacity = 256;
    builder->buffer = (char*)malloc(builder->capacity);
    builder->length = 0;
    builder->buffer[0] = '\0';
}

static void json_builder_free(JsonBuilder* builder) {
    free(builder->buffer);
    builder->buffer = NULL;
    builder->length = 0;
    builder->capacity = 0;
}

static void json_builder_ensure(JsonBuilder* builder, size_t needed) {
    if (builder->length + needed >= builder->capacity) {
        while (builder->length + needed >= builder->capacity) {
            builder->capacity *= 2;
        }
        builder->buffer = (char*)realloc(builder->buffer, builder->capacity);
    }
}

static void json_builder_append(JsonBuilder* builder, const char* str) {
    size_t len = strlen(str);
    json_builder_ensure(builder, len + 1);
    memcpy(builder->buffer + builder->length, str, len + 1);
    builder->length += len;
}

static void json_builder_append_char(JsonBuilder* builder, char c) {
    json_builder_ensure(builder, 2);
    builder->buffer[builder->length++] = c;
    builder->buffer[builder->length] = '\0';
}

static void json_builder_append_int(JsonBuilder* builder, int32_t value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    json_builder_append(builder, buf);
}

static void json_builder_append_float(JsonBuilder* builder, double value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", value);
    json_builder_append(builder, buf);
}

static void json_stringify_value(JsonBuilder* builder, Value value);

static void json_stringify_string(JsonBuilder* builder, const char* str, int length) {
    json_builder_append_char(builder, '"');
    for (int i = 0; i < length; i++) {
        char c = str[i];
        switch (c) {
            case '"': json_builder_append(builder, "\\\""); break;
            case '\\': json_builder_append(builder, "\\\\"); break;
            case '\b': json_builder_append(builder, "\\b"); break;
            case '\f': json_builder_append(builder, "\\f"); break;
            case '\n': json_builder_append(builder, "\\n"); break;
            case '\r': json_builder_append(builder, "\\r"); break;
            case '\t': json_builder_append(builder, "\\t"); break;
            default:
                if (c >= 0x20) {
                    json_builder_append_char(builder, c);
                } else {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    json_builder_append(builder, buf);
                }
        }
    }
    json_builder_append_char(builder, '"');
}

static void json_stringify_array(JsonBuilder* builder, ObjArray* arr) {
    json_builder_append_char(builder, '[');
    for (int i = 0; i < arr->count; i++) {
        if (i > 0) json_builder_append(builder, ",");
        json_stringify_value(builder, arr->items[i]);
    }
    json_builder_append_char(builder, ']');
}

static void json_stringify_map(JsonBuilder* builder, ObjMap* map) {
    json_builder_append_char(builder, '{');
    bool first = true;
    for (int i = 0; i < map->capacity; i++) {
        VMMapEntry* entry = &map->entries[i];
        if (entry->occupied && entry->key != NULL) {
            if (!first) json_builder_append(builder, ",");
            first = false;
            json_stringify_string(builder, entry->key->chars, entry->key->length);
            json_builder_append_char(builder, ':');
            json_stringify_value(builder, entry->value);
        }
    }
    json_builder_append_char(builder, '}');
}

static void json_stringify_value(JsonBuilder* builder, Value value) {
    if (is_nothing(value)) {
        json_builder_append(builder, "null");
    } else if (is_bool(value)) {
        json_builder_append(builder, as_bool(value) ? "true" : "false");
    } else if (is_int(value)) {
        json_builder_append_int(builder, as_int(value));
    } else if (is_number(value)) {
        json_builder_append_float(builder, as_number(value));
    } else if (is_obj(value)) {
        Obj* obj = (Obj*)as_obj(value);
        switch (obj->type) {
            case OBJ_STRING: {
                ObjString* str = (ObjString*)obj;
                json_stringify_string(builder, str->chars, str->length);
                break;
            }
            case OBJ_ARRAY:
                json_stringify_array(builder, (ObjArray*)obj);
                break;
            case OBJ_MAP:
                json_stringify_map(builder, (ObjMap*)obj);
                break;
            default:
                json_builder_append(builder, "null");
                break;
        }
    } else {
        json_builder_append(builder, "null");
    }
}

char* json_stringify(Value value) {
    JsonBuilder builder;
    json_builder_init(&builder);
    json_stringify_value(&builder, value);
    return builder.buffer;  /* Caller must free */
}

/* ══════════════════════════════════════════════════════════════
 *  VM Native Functions
 * ══════════════════════════════════════════════════════════════ */

static Value native_json_parse(int argCount, Value* args) {
    if (argCount != 1 || !is_obj(args[0])) {
        return val_nothing_v();
    }
    
    Obj* obj = (Obj*)as_obj(args[0]);
    if (obj->type != OBJ_STRING) {
        return val_nothing_v();
    }
    
    ObjString* json_str = (ObjString*)obj;
    return json_parse_string_value(json_str->chars);
}

static Value native_json_stringify(int argCount, Value* args) {
    if (argCount != 1) {
        return val_nothing_v();
    }
    
    char* json = json_stringify(args[0]);
    ObjString* result = obj_string_new(json, (int)strlen(json));
    free(json);
    return val_obj(result);
}

void vm_register_json_module(VM* vm) {
    vm_define_native(vm, "json_parse", native_json_parse, 1);
    vm_define_native(vm, "json_stringify", native_json_stringify, 1);
}
