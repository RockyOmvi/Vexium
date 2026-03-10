#include "interpreter.h"
#include "vex_module.h"

/* ══════════════════════════════════════════════════════════════
 *  VALUE CONSTRUCTORS
 * ══════════════════════════════════════════════════════════════ */

VexValue vex_int(int64_t val) {
    VexValue v; memset(&v, 0, sizeof(v));
    v.type = VAL_INT; v.as.int_val = val;
    return v;
}

VexValue vex_float(double val) {
    VexValue v; memset(&v, 0, sizeof(v));
    v.type = VAL_FLOAT; v.as.float_val = val;
    return v;
}

VexValue vex_string(const char* str, int len) {
    VexValue v; memset(&v, 0, sizeof(v));
    v.type = VAL_STRING;
    v.as.string_val.data = vex_strdup(str, len);
    v.as.string_val.length = len;
    return v;
}

VexValue vex_bool(bool val) {
    VexValue v; memset(&v, 0, sizeof(v));
    v.type = VAL_BOOL; v.as.bool_val = val;
    return v;
}

VexValue vex_nothing(void) {
    VexValue v; memset(&v, 0, sizeof(v));
    v.type = VAL_NOTHING;
    return v;
}

/* ══════════════════════════════════════════════════════════════
 *  VALUE UTILITIES
 * ══════════════════════════════════════════════════════════════ */

const char* vex_type_name(VexValue val) {
    switch (val.type) {
        case VAL_INT:        return "int";
        case VAL_FLOAT:      return "float";
        case VAL_STRING:     return "str";
        case VAL_BOOL:       return "bool";
        case VAL_NOTHING:    return "nothing";
        case VAL_ARRAY:      return "array";
        case VAL_MAP:        return "map";
        case VAL_FN:         return "fn";
        case VAL_STRUCT_DEF: return "struct";
        case VAL_INSTANCE:   return "instance";
        case VAL_BUILTIN_FN: return "builtin_fn";
    }
    return "unknown";
}

bool vex_is_truthy(VexValue val) {
    switch (val.type) {
        case VAL_BOOL:     return val.as.bool_val;
        case VAL_NOTHING:  return false;
        case VAL_INT:      return val.as.int_val != 0;
        case VAL_FLOAT:    return val.as.float_val != 0.0;
        case VAL_STRING:   return val.as.string_val.length > 0;
        case VAL_ARRAY:    return val.as.array_val && val.as.array_val->count > 0;
        case VAL_MAP:      return val.as.map_val && val.as.map_val->count > 0;
        default:           return true;
    }
}

void vex_print_value(VexValue val) {
    switch (val.type) {
        case VAL_INT:
            printf("%lld", (long long)val.as.int_val);
            break;
        case VAL_FLOAT:
            printf("%g", val.as.float_val);
            break;
        case VAL_STRING:
            printf("%s", val.as.string_val.data);
            break;
        case VAL_BOOL:
            printf("%s", val.as.bool_val ? "true" : "false");
            break;
        case VAL_NOTHING:
            /* Don't print 'nothing' for implicit returns - Python style */
            return;
            break;
        case VAL_ARRAY:
            printf("[");
            if (val.as.array_val) {
                for (int i = 0; i < val.as.array_val->count; i++) {
                    if (i > 0) printf(", ");
                    if (val.as.array_val->items[i].type == VAL_STRING) printf("\"");
                    vex_print_value(val.as.array_val->items[i]);
                    if (val.as.array_val->items[i].type == VAL_STRING) printf("\"");
                }
            }
            printf("]");
            break;
        case VAL_MAP:
            printf("{");
            if (val.as.map_val) {
                for (int i = 0; i < val.as.map_val->count; i++) {
                    if (i > 0) printf(", ");
                    printf("\"%s\": ", val.as.map_val->entries[i].key);
                    vex_print_value(val.as.map_val->entries[i].value);
                }
            }
            printf("}");
            break;
        case VAL_FN:
            printf("<fn %s>", val.as.fn_val.decl->as.fn_decl.name);
            break;
        case VAL_STRUCT_DEF:
            printf("<struct %s>", val.as.struct_def.name);
            break;
        case VAL_INSTANCE:
            printf("<instance %s>", val.as.instance_val->struct_name);
            break;
        case VAL_BUILTIN_FN:
            printf("<builtin %s>", val.as.builtin_fn.name);
            break;
    }
}

char* vex_value_to_string(VexValue val) {
    char buf[256];
    switch (val.type) {
        case VAL_INT:
            snprintf(buf, sizeof(buf), "%lld", (long long)val.as.int_val);
            break;
        case VAL_FLOAT:
            snprintf(buf, sizeof(buf), "%g", val.as.float_val);
            break;
        case VAL_STRING:
            return vex_strdup(val.as.string_val.data, val.as.string_val.length);
        case VAL_BOOL:
            return vex_strdup(val.as.bool_val ? "true" : "false",
                val.as.bool_val ? 4 : 5);
        case VAL_NOTHING:
            return vex_strdup("", 1);
        default:
            snprintf(buf, sizeof(buf), "<%s>", vex_type_name(val));
            break;
    }
    return vex_strdup(buf, (int)strlen(buf));
}

/* ══════════════════════════════════════════════════════════════
 *  ENVIRONMENT
 * ══════════════════════════════════════════════════════════════ */

Environment* env_new(Environment* parent) {
    Environment* env = (Environment*)calloc(1, sizeof(Environment));
    env->parent = parent;
    env->ref_count = 1;
    if (parent) env_retain(parent);
    return env;
}

void env_retain(Environment* env) {
    if (env) env->ref_count++;
}

void env_release(Environment* env) {
    if (!env) return;
    env->ref_count--;
    if (env->ref_count <= 0) {
        env_free(env);
    }
}

void env_define(Environment* env, const char* name, VexValue value, bool is_const) {
    /* Check if already defined in THIS scope — overwrite */
    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->entries[i].name, name) == 0) {
            env->entries[i].value = value;
            env->entries[i].is_const = is_const;
            return;
        }
    }

    /* Grow */
    if (env->count >= env->capacity) {
        env->capacity = env->capacity == 0 ? 8 : env->capacity * 2;
        env->entries = (EnvEntry*)realloc(env->entries,
            sizeof(EnvEntry) * env->capacity);
    }
    EnvEntry* e = &env->entries[env->count++];
    e->name = vex_strdup(name, (int)strlen(name));
    e->value = value;
    e->is_const = is_const;
}

bool env_set(Environment* env, const char* name, VexValue value) {
    for (Environment* e = env; e != NULL; e = e->parent) {
        for (int i = 0; i < e->count; i++) {
            if (strcmp(e->entries[i].name, name) == 0) {
                if (e->entries[i].is_const) {
                    fprintf(stderr, "Error: Cannot reassign constant '%s'\n", name);
                    return false;
                }
                e->entries[i].value = value;
                return true;
            }
        }
    }
    fprintf(stderr, "Error: Undefined variable '%s'\n", name);
    return false;
}

VexValue* env_get(Environment* env, const char* name) {
    for (Environment* e = env; e != NULL; e = e->parent) {
        for (int i = 0; i < e->count; i++) {
            if (strcmp(e->entries[i].name, name) == 0) {
                return &e->entries[i].value;
            }
        }
    }
    return NULL;
}

void env_free(Environment* env) {
    if (!env) return;
    for (int i = 0; i < env->count; i++) {
        free(env->entries[i].name);
    }
    free(env->entries);
    if (env->parent) env_release(env->parent);
    free(env);
}

/* ══════════════════════════════════════════════════════════════
 *  BUILT-IN FUNCTIONS
 * ══════════════════════════════════════════════════════════════ */

static VexValue builtin_len(VexValue* args, int argc) {
    if (argc != 1) {
        fprintf(stderr, "Error: len() expects 1 argument, got %d\n", argc);
        return vex_nothing();
    }
    switch (args[0].type) {
        case VAL_STRING: return vex_int(args[0].as.string_val.length);
        case VAL_ARRAY:  return vex_int(args[0].as.array_val ? args[0].as.array_val->count : 0);
        case VAL_MAP:    return vex_int(args[0].as.map_val ? args[0].as.map_val->count : 0);
        default:
            fprintf(stderr, "Error: len() unsupported for type '%s'\n", vex_type_name(args[0]));
            return vex_nothing();
    }
}

static VexValue builtin_type(VexValue* args, int argc) {
    if (argc != 1) {
        fprintf(stderr, "Error: type() expects 1 argument, got %d\n", argc);
        return vex_nothing();
    }
    const char* t = vex_type_name(args[0]);
    return vex_string(t, (int)strlen(t));
}

static VexValue builtin_input(VexValue* args, int argc) {
    if (argc >= 1 && args[0].type == VAL_STRING) {
        printf("%s", args[0].as.string_val.data);
        fflush(stdout);
    }
    char buf[1024];
    if (fgets(buf, sizeof(buf), stdin)) {
        int len = (int)strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[--len] = '\0';
        if (len > 0 && buf[len - 1] == '\r') buf[--len] = '\0';
        return vex_string(buf, len);
    }
    return vex_string("", 0);
}

static VexValue builtin_int_cast(VexValue* args, int argc) {
    if (argc != 1) { fprintf(stderr, "Error: int() expects 1 argument\n"); return vex_nothing(); }
    switch (args[0].type) {
        case VAL_INT:    return args[0];
        case VAL_FLOAT:  return vex_int((int64_t)args[0].as.float_val);
        case VAL_STRING: return vex_int(strtoll(args[0].as.string_val.data, NULL, 10));
        case VAL_BOOL:   return vex_int(args[0].as.bool_val ? 1 : 0);
        default:
            fprintf(stderr, "Error: Cannot convert %s to int\n", vex_type_name(args[0]));
            return vex_nothing();
    }
}

static VexValue builtin_float_cast(VexValue* args, int argc) {
    if (argc != 1) { fprintf(stderr, "Error: float() expects 1 argument\n"); return vex_nothing(); }
    switch (args[0].type) {
        case VAL_INT:    return vex_float((double)args[0].as.int_val);
        case VAL_FLOAT:  return args[0];
        case VAL_STRING: return vex_float(strtod(args[0].as.string_val.data, NULL));
        default:
            fprintf(stderr, "Error: Cannot convert %s to float\n", vex_type_name(args[0]));
            return vex_nothing();
    }
}

static VexValue builtin_str_cast(VexValue* args, int argc) {
    if (argc != 1) { fprintf(stderr, "Error: str() expects 1 argument\n"); return vex_nothing(); }
    char* s = vex_value_to_string(args[0]);
    VexValue result = vex_string(s, (int)strlen(s));
    free(s);
    return result;
}

/* Global interpreter instance for throw to access */
static Interpreter* g_interpreter = NULL;

/* throw(message) - Raise an exception */
static VexValue builtin_throw(VexValue* args, int argc) {
    if (argc != 1) { 
        fprintf(stderr, "Error: throw() expects 1 argument\n"); 
        return vex_nothing(); 
    }
    /* Print error message */
    char* msg = vex_value_to_string(args[0]);
    fprintf(stderr, "Error [throw]: %s\n", msg);
    free(msg);
    /* Mark that an error occurred */
    if (g_interpreter) {
        g_interpreter->had_error = true;
        g_interpreter->signal.type = SIGNAL_THROW;
        g_interpreter->signal.error_value = args[0];
    }
    return vex_nothing();
}

static VexValue builtin_push(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_ARRAY) {
        fprintf(stderr, "Error: push() expects (array, value)\n");
        return vex_nothing();
    }
    ValueArray* arr = args[0].as.array_val;
    if (arr->count >= arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
        arr->items = (VexValue*)realloc(arr->items, sizeof(VexValue) * arr->capacity);
    }
    arr->items[arr->count++] = args[1];
    return vex_nothing();
}

static VexValue builtin_pop(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_ARRAY) {
        fprintf(stderr, "Error: pop() expects (array)\n");
        return vex_nothing();
    }
    ValueArray* arr = args[0].as.array_val;
    if (arr->count == 0) {
        fprintf(stderr, "Error: pop() on empty array\n");
        return vex_nothing();
    }
    return arr->items[--arr->count];
}

static VexValue builtin_range(VexValue* args, int argc) {
    if (argc < 1 || argc > 3) {
        fprintf(stderr, "Error: range() expects 1-3 arguments\n");
        return vex_nothing();
    }
    int64_t start = 0, end, step = 1;
    if (argc == 1) { end = args[0].as.int_val; }
    else { start = args[0].as.int_val; end = args[1].as.int_val; }
    if (argc == 3) { step = args[2].as.int_val; }
    if (step == 0) { fprintf(stderr, "Error: range() step cannot be 0\n"); return vex_nothing(); }

    VexValue result;
    result.type = VAL_ARRAY;
    result.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
    for (int64_t i = start; step > 0 ? i < end : i > end; i += step) {
        ValueArray* arr = result.as.array_val;
        if (arr->count >= arr->capacity) {
            arr->capacity = arr->capacity == 0 ? 16 : arr->capacity * 2;
            arr->items = (VexValue*)realloc(arr->items, sizeof(VexValue) * arr->capacity);
        }
        arr->items[arr->count++] = vex_int(i);
    }
    return result;
}

/* ══════════════════════════════════════════════════════════════
 *  STRING INTERPOLATION
 * ══════════════════════════════════════════════════════════════ */

static char* interpolate_string(const char* src, int src_len, Environment* env) {
    /* Budget: grow buffer as needed */
    int cap = src_len * 2 + 1;
    char* result = (char*)malloc(cap);
    int out = 0;

    for (int i = 0; i < src_len; i++) {
        if (src[i] == '{') {
            /* Find closing brace */
            int start = i + 1;
            int depth = 1;
            i++;
            while (i < src_len && depth > 0) {
                if (src[i] == '{') depth++;
                else if (src[i] == '}') depth--;
                if (depth > 0) i++;
            }
            /* Extract expression (may contain dots like self.name) */
            int name_len = i - start;
            char* expr = vex_strdup(src + start, name_len);

            /* Resolve dot-access: split on '.' and walk fields */
            char* vs;
            char* dot = strchr(expr, '.');
            if (dot) {
                /* Split: base.field1.field2... */
                *dot = '\0';
                VexValue* base_val = env_get(env, expr);
                if (base_val) {
                    VexValue current = *base_val;
                    char* field_start = dot + 1;
                    bool resolved = true;
                    while (field_start && *field_start) {
                        char* next_dot = strchr(field_start, '.');
                        if (next_dot) *next_dot = '\0';
                        if (current.type == VAL_INSTANCE) {
                            bool found = false;
                            for (int fi = 0; fi < current.as.instance_val->fields.count; fi++) {
                                if (strcmp(current.as.instance_val->fields.entries[fi].key, field_start) == 0) {
                                    current = current.as.instance_val->fields.entries[fi].value;
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) { resolved = false; break; }
                        } else if (current.type == VAL_MAP) {
                            bool found = false;
                            for (int mi = 0; mi < current.as.map_val->count; mi++) {
                                if (strcmp(current.as.map_val->entries[mi].key, field_start) == 0) {
                                    current = current.as.map_val->entries[mi].value;
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) { resolved = false; break; }
                        } else { resolved = false; break; }
                        field_start = next_dot ? next_dot + 1 : NULL;
                    }
                    vs = resolved ? vex_value_to_string(current) : vex_strdup("nothing", 7);
                } else {
                    vs = vex_strdup("nothing", 7);
                }
            } else {
                VexValue* val = env_get(env, expr);
                if (val) vs = vex_value_to_string(*val);
                else vs = vex_strdup("nothing", 7);
            }

            int vs_len = (int)strlen(vs);
            while (out + vs_len + 1 >= cap) { cap *= 2; result = (char*)realloc(result, cap); }
            memcpy(result + out, vs, vs_len);
            out += vs_len;
            free(expr);
            free(vs);
        } else {
            if (out + 2 >= cap) { cap *= 2; result = (char*)realloc(result, cap); }
            result[out++] = src[i];
        }
    }
    result[out] = '\0';
    return result;
}

/* ══════════════════════════════════════════════════════════════
 *  INTERPRETER CORE
 * ══════════════════════════════════════════════════════════════ */

static VexValue eval(Interpreter* interp, ASTNode* node, Environment* env);
static VexValue eval_method_call(Interpreter* interp, InstanceValue* inst,
    const char* method_name, VexValue* args, int argc, Environment* env);

/* ── Number coercion helpers ── */
static double to_double(VexValue v) {
    if (v.type == VAL_INT) return (double)v.as.int_val;
    if (v.type == VAL_FLOAT) return v.as.float_val;
    return 0.0;
}

static bool is_numeric(VexValue v) {
    return v.type == VAL_INT || v.type == VAL_FLOAT;
}

/* ── Binary operation ── */
static VexValue eval_binary(Interpreter* interp, ASTNode* node, Environment* env) {
    VexValue left = eval(interp, node->as.binary.left, env);
    VexValue right = eval(interp, node->as.binary.right, env);
    VexiumTokenType op = node->as.binary.op;

    /* String concatenation */
    if (op == TOKEN_PLUS && left.type == VAL_STRING && right.type == VAL_STRING) {
        int new_len = left.as.string_val.length + right.as.string_val.length;
        char* buf = (char*)malloc(new_len + 1);
        memcpy(buf, left.as.string_val.data, left.as.string_val.length);
        memcpy(buf + left.as.string_val.length, right.as.string_val.data, right.as.string_val.length);
        buf[new_len] = '\0';
        VexValue result = vex_string(buf, new_len);
        free(buf);
        return result;
    }

    /* String repetition: "ha" * 3 = "hahaha" */
    if (op == TOKEN_STAR && left.type == VAL_STRING && right.type == VAL_INT) {
        int rep = (int)right.as.int_val;
        int new_len = left.as.string_val.length * rep;
        char* buf = (char*)malloc(new_len + 1);
        for (int i = 0; i < rep; i++)
            memcpy(buf + i * left.as.string_val.length, left.as.string_val.data, left.as.string_val.length);
        buf[new_len] = '\0';
        VexValue result = vex_string(buf, new_len);
        free(buf);
        return result;
    }

    /* Numeric ops */
    if (is_numeric(left) && is_numeric(right)) {
        bool use_float = (left.type == VAL_FLOAT || right.type == VAL_FLOAT);

        switch (op) {
            case TOKEN_PLUS:
                return use_float ? vex_float(to_double(left) + to_double(right))
                    : vex_int(left.as.int_val + right.as.int_val);
            case TOKEN_MINUS:
                return use_float ? vex_float(to_double(left) - to_double(right))
                    : vex_int(left.as.int_val - right.as.int_val);
            case TOKEN_STAR:
                return use_float ? vex_float(to_double(left) * to_double(right))
                    : vex_int(left.as.int_val * right.as.int_val);
            case TOKEN_SLASH:
                if ((right.type == VAL_INT && right.as.int_val == 0) ||
                    (right.type == VAL_FLOAT && right.as.float_val == 0.0)) {
                    fprintf(stderr, "Error [line %d]: Division by zero\n", node->line);
                    interp->had_error = true;
                    return vex_nothing();
                }
                return use_float ? vex_float(to_double(left) / to_double(right))
                    : vex_int(left.as.int_val / right.as.int_val);
            case TOKEN_PERCENT:
                if (right.type == VAL_INT && right.as.int_val == 0) {
                    fprintf(stderr, "Error [line %d]: Modulo by zero\n", node->line);
                    return vex_nothing();
                }
                return vex_int(left.as.int_val % right.as.int_val);
            case TOKEN_POWER: {
                double result = 1.0;
                double base = to_double(left);
                int64_t exp = (int64_t)to_double(right);
                for (int64_t i = 0; i < exp; i++) result *= base;
                return (left.type == VAL_INT && right.type == VAL_INT)
                    ? vex_int((int64_t)result) : vex_float(result);
            }

            /* Comparison — numeric */
            case TOKEN_LT: case TOKEN_IS_LESS:
                return vex_bool(to_double(left) < to_double(right));
            case TOKEN_GT: case TOKEN_IS_GREATER:
                return vex_bool(to_double(left) > to_double(right));
            case TOKEN_LTE: case TOKEN_IS_AT_MOST:
                return vex_bool(to_double(left) <= to_double(right));
            case TOKEN_GTE: case TOKEN_IS_AT_LEAST:
                return vex_bool(to_double(left) >= to_double(right));
            case TOKEN_EQ: case TOKEN_IS:
                return use_float ? vex_bool(to_double(left) == to_double(right))
                    : vex_bool(left.as.int_val == right.as.int_val);
            case TOKEN_NEQ: case TOKEN_IS_NOT:
                return use_float ? vex_bool(to_double(left) != to_double(right))
                    : vex_bool(left.as.int_val != right.as.int_val);
            default: break;
        }
    }

    /* Equality for any type */
    if (op == TOKEN_EQ || op == TOKEN_IS) {
        if (left.type != right.type) return vex_bool(false);
        switch (left.type) {
            case VAL_BOOL: return vex_bool(left.as.bool_val == right.as.bool_val);
            case VAL_STRING:
                return vex_bool(left.as.string_val.length == right.as.string_val.length &&
                    memcmp(left.as.string_val.data, right.as.string_val.data, left.as.string_val.length) == 0);
            case VAL_NOTHING: return vex_bool(true);
            default: return vex_bool(false);
        }
    }
    if (op == TOKEN_NEQ || op == TOKEN_IS_NOT) {
        if (left.type != right.type) return vex_bool(true);
        switch (left.type) {
            case VAL_BOOL: return vex_bool(left.as.bool_val != right.as.bool_val);
            case VAL_STRING:
                return vex_bool(left.as.string_val.length != right.as.string_val.length ||
                    memcmp(left.as.string_val.data, right.as.string_val.data, left.as.string_val.length) != 0);
            case VAL_NOTHING: return vex_bool(false);
            default: return vex_bool(true);
        }
    }

    /* Logical */
    if (op == TOKEN_AND) return vex_bool(vex_is_truthy(left) && vex_is_truthy(right));
    if (op == TOKEN_OR)  return vex_bool(vex_is_truthy(left) || vex_is_truthy(right));

    /* ── Operator overloading for instances ── */
    if (left.type == VAL_INSTANCE) {
        ASTNode* struct_decl = left.as.instance_val->decl;
        const char* op_method = NULL;
        
        /* Map token to operator method name */
        switch (op) {
            case TOKEN_PLUS: op_method = "operator +"; break;
            case TOKEN_MINUS: op_method = "operator -"; break;
            case TOKEN_STAR: op_method = "operator *"; break;
            case TOKEN_SLASH: op_method = "operator /"; break;
            case TOKEN_PERCENT: op_method = "operator %"; break;
            case TOKEN_POWER: op_method = "operator **"; break;
            case TOKEN_LT: op_method = "operator <"; break;
            case TOKEN_GT: op_method = "operator >"; break;
            case TOKEN_LTE: op_method = "operator <="; break;
            case TOKEN_GTE: op_method = "operator >="; break;
            case TOKEN_EQ: op_method = "operator =="; break;
            case TOKEN_NEQ: op_method = "operator !="; break;
            default: break;
        }
        
        if (op_method) {
            /* Look for operator method in struct */
            for (int i = 0; i < struct_decl->as.struct_decl.method_count; i++) {
                if (strcmp(struct_decl->as.struct_decl.methods[i].name, op_method) == 0) {
                    VexValue rhs = right;
                    VexValue* args = (VexValue*)malloc(sizeof(VexValue));
                    args[0] = rhs;
                    VexValue result = eval_method_call(interp, left.as.instance_val,
                        op_method, args, 1, env);
                    free(args);
                    return result;
                }
            }
        }
    }

    fprintf(stderr, "Error [line %d]: Unsupported binary operation\n", node->line);
    return vex_nothing();
}

/* ── Call function ── */
static VexValue eval_call(Interpreter* interp, ASTNode* node, Environment* env) {
    int argc = node->as.call.args.count;

    /* ── Intercept instance.method() calls ── */
    if (node->as.call.callee->type == NODE_FIELD_ACCESS) {
        ASTNode* fa = node->as.call.callee;
        VexValue obj = eval(interp, fa->as.field_access.object, env);
        const char* method_name = fa->as.field_access.field;

        if (obj.type == VAL_INSTANCE) {
            /* Evaluate arguments */
            VexValue* args = NULL;
            if (argc > 0) {
                args = (VexValue*)malloc(sizeof(VexValue) * argc);
                for (int i = 0; i < argc; i++)
                    args[i] = eval(interp, node->as.call.args.items[i], env);
            }
            VexValue result = eval_method_call(interp, obj.as.instance_val,
                method_name, args, argc, env);
            free(args);
            return result;
        }
        /* Not an instance — fall through to normal field access + call */
    }

    VexValue callee = eval(interp, node->as.call.callee, env);

    /* Evaluate all arguments */
    VexValue* args = NULL;
    if (argc > 0) {
        args = (VexValue*)malloc(sizeof(VexValue) * argc);
        for (int i = 0; i < argc; i++) {
            args[i] = eval(interp, node->as.call.args.items[i], env);
        }
    }

    VexValue result = vex_nothing();

    if (callee.type == VAL_BUILTIN_FN) {
        result = callee.as.builtin_fn.func(args, argc);
    }
    else if (callee.type == VAL_FN) {
        ASTNode* fn = callee.as.fn_val.decl;
        Environment* fn_env = env_new(callee.as.fn_val.closure);

        /* Bind parameters (with default value support) */
        int param_count = fn->as.fn_decl.params.count;
        for (int i = 0; i < param_count; i++) {
            if (i < argc) {
                env_define(fn_env, fn->as.fn_decl.params.items[i].name, args[i], false);
            } else if (fn->as.fn_decl.params.items[i].default_value) {
                VexValue def = eval(interp, fn->as.fn_decl.params.items[i].default_value, env);
                env_define(fn_env, fn->as.fn_decl.params.items[i].name, def, false);
            } else {
                env_define(fn_env, fn->as.fn_decl.params.items[i].name, vex_nothing(), false);
            }
        }

        /* Execute body */
        result = eval(interp, fn->as.fn_decl.body, fn_env);

        if (interp->signal.type == SIGNAL_RETURN) {
            result = interp->signal.return_value;
            interp->signal.type = SIGNAL_NONE;
        }
        /* Handle throw signal - propagate error */
        if (interp->signal.type == SIGNAL_THROW) {
            interp->signal.type = SIGNAL_NONE;
            return result;
        }

        env_release(fn_env);
    }
    else if (callee.type == VAL_STRUCT_DEF) {
        /* Struct constructor — create instance */
        InstanceValue* inst = (InstanceValue*)calloc(1, sizeof(InstanceValue));
        inst->struct_name = vex_strdup(callee.as.struct_def.name, (int)strlen(callee.as.struct_def.name));
        inst->decl = callee.as.struct_def.decl;
        inst->closure = callee.as.fn_val.closure;
        if (inst->closure) env_retain(inst->closure);

        /* Initialize fields from struct definition and parent structs */
        ASTNode* sd = callee.as.struct_def.decl;
        inst->fields.entries = NULL;
        inst->fields.count = 0;
        inst->fields.capacity = 0;

        /* Add fields: collect from all parents first, then this struct */
        /* We need to iterate through parent chain to get all fields */
        int field_arg_idx = 0;
        ASTNode* collect_sd = sd;
        
        /* Collect all fields from this struct and parents */
        while (collect_sd) {
            int next_parent_idx = -1;
            
            /* Process all parents of current struct */
            for (int p = 0; p < collect_sd->as.struct_decl.parent_count; p++) {
                VexValue* parent_def = env_get(env, collect_sd->as.struct_decl.parent_names[p]);
                if (parent_def && parent_def->type == VAL_STRUCT_DEF && next_parent_idx == -1) {
                    next_parent_idx = p;
                    break;
                }
            }
            
            /* Add current struct's fields */
            for (int i = 0; i < collect_sd->as.struct_decl.field_count; i++) {
                if (inst->fields.count >= inst->fields.capacity) {
                    inst->fields.capacity = inst->fields.capacity == 0 ? 4 : inst->fields.capacity * 2;
                    inst->fields.entries = (ValueMapEntry*)realloc(inst->fields.entries,
                        sizeof(ValueMapEntry) * inst->fields.capacity);
                }
                ValueMapEntry* e = &inst->fields.entries[inst->fields.count++];
                e->key = vex_strdup(collect_sd->as.struct_decl.fields[i].name,
                    (int)strlen(collect_sd->as.struct_decl.fields[i].name));
                e->value = (field_arg_idx < argc) ? args[field_arg_idx++] : vex_nothing();
            }
            
            /* Move to first parent if exists */
            if (next_parent_idx >= 0) {
                VexValue* parent_def = env_get(env, collect_sd->as.struct_decl.parent_names[next_parent_idx]);
                if (parent_def && parent_def->type == VAL_STRUCT_DEF) {
                    collect_sd = parent_def->as.struct_def.decl;
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        result.type = VAL_INSTANCE;
        result.as.instance_val = inst;

        /* Auto-call create() method if it exists (primary constructor) */
        bool found_create = false;
        for (int i = 0; i < sd->as.struct_decl.method_count; i++) {
            if (strcmp(sd->as.struct_decl.methods[i].name, "create") == 0) {
                eval_method_call(interp, inst, "create", args, argc, env);
                found_create = true;
                break;
            }
        }
        /* If create doesn't exist, look for init() */
        if (!found_create) {
            for (int i = 0; i < sd->as.struct_decl.method_count; i++) {
                if (strcmp(sd->as.struct_decl.methods[i].name, "init") == 0) {
                    eval_method_call(interp, inst, "init", args, argc, env);
                    break;
                }
            }
        }
    }
    else {
        fprintf(stderr, "Error [line %d]: '%s' is not callable\n",
            node->line, vex_type_name(callee));
        interp->had_error = true;
    }

    free(args);
    return result;
}

/* ── Method call on an instance ── */
static VexValue eval_method_call(Interpreter* interp, InstanceValue* inst,
    const char* method_name, VexValue* args, int argc, Environment* env) {

    /* Look for method in this struct and parent chain */
    ASTNode* sd = inst->decl;
    while (sd != NULL) {
        for (int i = 0; i < sd->as.struct_decl.method_count; i++) {
            if (strcmp(sd->as.struct_decl.methods[i].name, method_name) == 0) {
                StructMethod* m = &sd->as.struct_decl.methods[i];
                Environment* m_env = env_new(env);

                /* Auto-bind self */
                VexValue self_val;
                self_val.type = VAL_INSTANCE;
                self_val.as.instance_val = inst;
                env_define(m_env, "self", self_val, false);

                /* Bind parameters — skip "self" in param list */
                int p = 0;
                for (int j = 0; j < m->params.count; j++) {
                    if (strcmp(m->params.items[j].name, "self") == 0) continue;
                    if (p < argc) {
                        env_define(m_env, m->params.items[j].name, args[p++], false);
                    } else if (m->params.items[j].default_value) {
                        VexValue def = eval(interp, m->params.items[j].default_value, env);
                        env_define(m_env, m->params.items[j].name, def, false);
                    } else {
                        env_define(m_env, m->params.items[j].name, vex_nothing(), false);
                    }
                }

                VexValue result = eval(interp, m->body, m_env);
                if (interp->signal.type == SIGNAL_RETURN) {
                    result = interp->signal.return_value;
                    interp->signal.type = SIGNAL_NONE;
                }
                env_release(m_env);
                return result;
            }
        }
        /* Walk up to parent struct (or check all parents for multiple inheritance) */
        bool found_parent = false;
        if (sd->as.struct_decl.parent_count > 0) {
            /* Check first parent (for chain-based inheritance) */
            VexValue* parent_def = env_get(env, sd->as.struct_decl.parent_names[0]);
            if (parent_def && parent_def->type == VAL_STRUCT_DEF) {
                sd = parent_def->as.struct_def.decl;
                found_parent = true;
            }
        }
        if (!found_parent) {
            break;
        }
    }

    fprintf(stderr, "Error: '%s' has no method '%s'\n", inst->struct_name, method_name);
    return vex_nothing();
}

/* ══════════════════════════════════════════════════════════════
 *  MAIN EVAL — walks the AST
 * ══════════════════════════════════════════════════════════════ */

static VexValue eval(Interpreter* interp, ASTNode* node, Environment* env) {
    if (node == NULL) return vex_nothing();
    if (interp->signal.type != SIGNAL_NONE) return vex_nothing();

    switch (node->type) {
        /* ── Literals ── */
        case NODE_INT_LITERAL:
            return vex_int(node->as.int_literal.value);
        case NODE_FLOAT_LITERAL:
            return vex_float(node->as.float_literal.value);
        case NODE_STRING_LITERAL: {
            /* Handle string interpolation */
            char* interp_str = interpolate_string(
                node->as.string_literal.value,
                node->as.string_literal.length, env);
            VexValue result = vex_string(interp_str, (int)strlen(interp_str));
            free(interp_str);
            return result;
        }
        case NODE_BOOL_LITERAL:
            return vex_bool(node->as.bool_literal.value);
        case NODE_NOTHING_LITERAL:
            return vex_nothing();

        /* ── Identifier ── */
        case NODE_IDENTIFIER: {
            VexValue* val = env_get(env, node->as.identifier.name);
            if (val == NULL) {
                fprintf(stderr, "Error [line %d]: Undefined variable '%s'\n",
                    node->line, node->as.identifier.name);
                interp->had_error = true;
                return vex_nothing();
            }
            return *val;
        }

        /* ── Binary ── */
        case NODE_BINARY_OP:
            return eval_binary(interp, node, env);

        /* ── Unary ── */
        case NODE_UNARY_OP: {
            VexValue operand = eval(interp, node->as.unary.operand, env);
            if (node->as.unary.op == TOKEN_NOT)
                return vex_bool(!vex_is_truthy(operand));
            if (node->as.unary.op == TOKEN_MINUS) {
                if (operand.type == VAL_INT) return vex_int(-operand.as.int_val);
                if (operand.type == VAL_FLOAT) return vex_float(-operand.as.float_val);
            }
            return vex_nothing();
        }

        /* ── Call ── */
        case NODE_CALL:
            return eval_call(interp, node, env);

        /* ── Index ── */
        case NODE_INDEX: {
            VexValue obj = eval(interp, node->as.index_access.object, env);
            VexValue idx = eval(interp, node->as.index_access.index, env);
            if (obj.type == VAL_ARRAY && idx.type == VAL_INT) {
                int i = (int)idx.as.int_val;
                if (i < 0) i += obj.as.array_val->count; /* negative indexing */
                if (i >= 0 && i < obj.as.array_val->count)
                    return obj.as.array_val->items[i];
                fprintf(stderr, "Error [line %d]: Index %d out of bounds\n", node->line, (int)idx.as.int_val);
                return vex_nothing();
            }
            if (obj.type == VAL_MAP && idx.type == VAL_STRING) {
                for (int i = 0; i < obj.as.map_val->count; i++) {
                    if (strcmp(obj.as.map_val->entries[i].key, idx.as.string_val.data) == 0)
                        return obj.as.map_val->entries[i].value;
                }
                return vex_nothing();
            }
            if (obj.type == VAL_STRING && idx.type == VAL_INT) {
                int i = (int)idx.as.int_val;
                if (i < 0) i += obj.as.string_val.length;
                if (i >= 0 && i < obj.as.string_val.length)
                    return vex_string(&obj.as.string_val.data[i], 1);
                return vex_nothing();
            }
            fprintf(stderr, "Error [line %d]: Cannot index %s\n", node->line, vex_type_name(obj));
            return vex_nothing();
        }

        /* ── Field access ── */
        case NODE_FIELD_ACCESS: {
            VexValue obj = eval(interp, node->as.field_access.object, env);
            const char* field = node->as.field_access.field;
            if (obj.type == VAL_INSTANCE) {
                /* Look up in instance fields */
                for (int i = 0; i < obj.as.instance_val->fields.count; i++) {
                    if (strcmp(obj.as.instance_val->fields.entries[i].key, field) == 0)
                        return obj.as.instance_val->fields.entries[i].value;
                }
                /* Could be a method call — return nothing for now, handled by call */
            }
            if (obj.type == VAL_MAP) {
                for (int i = 0; i < obj.as.map_val->count; i++) {
                    if (strcmp(obj.as.map_val->entries[i].key, field) == 0)
                        return obj.as.map_val->entries[i].value;
                }
            }
            /* Method chaining: arr.length, str.length */
            if (obj.type == VAL_ARRAY && strcmp(field, "length") == 0)
                return vex_int(obj.as.array_val->count);
            if (obj.type == VAL_STRING && strcmp(field, "length") == 0)
                return vex_int(obj.as.string_val.length);

            fprintf(stderr, "Error [line %d]: No field '%s' on %s\n", node->line, field, vex_type_name(obj));
            return vex_nothing();
        }

        /* ── Array literal ── */
        case NODE_ARRAY_LITERAL: {
            VexValue result;
            result.type = VAL_ARRAY;
            result.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));
            for (int i = 0; i < node->as.array_literal.elements.count; i++) {
                VexValue elem = eval(interp, node->as.array_literal.elements.items[i], env);
                ValueArray* arr = result.as.array_val;
                if (arr->count >= arr->capacity) {
                    arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
                    arr->items = (VexValue*)realloc(arr->items, sizeof(VexValue) * arr->capacity);
                }
                arr->items[arr->count++] = elem;
            }
            return result;
        }

        /* ── Map literal ── */
        case NODE_MAP_LITERAL: {
            VexValue result;
            result.type = VAL_MAP;
            result.as.map_val = (ValueMap*)calloc(1, sizeof(ValueMap));
            for (int i = 0; i < node->as.map_literal.count; i++) {
                VexValue key = eval(interp, node->as.map_literal.entries[i].key, env);
                VexValue val = eval(interp, node->as.map_literal.entries[i].value, env);
                ValueMap* map = result.as.map_val;
                if (map->count >= map->capacity) {
                    map->capacity = map->capacity == 0 ? 4 : map->capacity * 2;
                    map->entries = (ValueMapEntry*)realloc(map->entries,
                        sizeof(ValueMapEntry) * map->capacity);
                }
                ValueMapEntry* e = &map->entries[map->count++];
                e->key = vex_value_to_string(key);
                e->value = val;
            }
            return result;
        }

        /* ── Assignment ── */
        case NODE_ASSIGN: {
            VexValue value = eval(interp, node->as.assign.value, env);
            VexiumTokenType op = node->as.assign.op;

            if (node->as.assign.target->type == NODE_IDENTIFIER) {
                const char* name = node->as.assign.target->as.identifier.name;
                if (op == TOKEN_BE) {
                    /* TOKEN_BE: Create variable if doesn't exist, else update */
                    VexValue* existing = env_get(env, name);
                    if (existing) {
                        env_set(env, name, value);
                    } else {
                        env_define(env, name, value, false);
                    }
                } else if (op == TOKEN_ASSIGN) {
                    env_set(env, name, value);
                } else {
                    VexValue* current = env_get(env, name);
                    if (!current) {
                        fprintf(stderr, "Error [line %d]: Undefined '%s'\n", node->line, name);
                        return vex_nothing();
                    }
                    VexValue new_val = vex_nothing();
                    if (op == TOKEN_PLUS_ASSIGN && is_numeric(*current) && is_numeric(value))
                        new_val = (current->type == VAL_FLOAT || value.type == VAL_FLOAT)
                            ? vex_float(to_double(*current) + to_double(value))
                            : vex_int(current->as.int_val + value.as.int_val);
                    else if (op == TOKEN_MINUS_ASSIGN && is_numeric(*current) && is_numeric(value))
                        new_val = (current->type == VAL_FLOAT || value.type == VAL_FLOAT)
                            ? vex_float(to_double(*current) - to_double(value))
                            : vex_int(current->as.int_val - value.as.int_val);
                    else if (op == TOKEN_STAR_ASSIGN && is_numeric(*current) && is_numeric(value))
                        new_val = (current->type == VAL_FLOAT || value.type == VAL_FLOAT)
                            ? vex_float(to_double(*current) * to_double(value))
                            : vex_int(current->as.int_val * value.as.int_val);
                    else if (op == TOKEN_SLASH_ASSIGN && is_numeric(*current) && is_numeric(value))
                        new_val = (current->type == VAL_FLOAT || value.type == VAL_FLOAT)
                            ? vex_float(to_double(*current) / to_double(value))
                            : vex_int(current->as.int_val / value.as.int_val);
                    else if (op == TOKEN_PLUS_ASSIGN && current->type == VAL_STRING && value.type == VAL_STRING) {
                        int new_len = current->as.string_val.length + value.as.string_val.length;
                        char* buf = (char*)malloc(new_len + 1);
                        memcpy(buf, current->as.string_val.data, current->as.string_val.length);
                        memcpy(buf + current->as.string_val.length, value.as.string_val.data, value.as.string_val.length);
                        buf[new_len] = '\0';
                        new_val = vex_string(buf, new_len);
                        free(buf);
                    }
                    env_set(env, name, new_val);
                }
            }
            /* Field assignment: obj.field be value */
            else if (node->as.assign.target->type == NODE_FIELD_ACCESS) {
                VexValue obj = eval(interp, node->as.assign.target->as.field_access.object, env);
                const char* field = node->as.assign.target->as.field_access.field;
                if (obj.type == VAL_INSTANCE) {
                    /* Try to update existing field */
                    for (int i = 0; i < obj.as.instance_val->fields.count; i++) {
                        if (strcmp(obj.as.instance_val->fields.entries[i].key, field) == 0) {
                            obj.as.instance_val->fields.entries[i].value = value;
                            return value;
                        }
                    }
                    /* Field doesn't exist — create it dynamically (Python-style) */
                    ValueMap* fmap = &obj.as.instance_val->fields;
                    if (fmap->count >= fmap->capacity) {
                        fmap->capacity = fmap->capacity == 0 ? 4 : fmap->capacity * 2;
                        fmap->entries = (ValueMapEntry*)realloc(fmap->entries,
                            sizeof(ValueMapEntry) * fmap->capacity);
                    }
                    ValueMapEntry* e = &fmap->entries[fmap->count++];
                    e->key = vex_strdup(field, (int)strlen(field));
                    e->value = value;
                    return value;
                }
                if (obj.type == VAL_MAP) {
                    /* Set/create field on map */
                    for (int i = 0; i < obj.as.map_val->count; i++) {
                        if (strcmp(obj.as.map_val->entries[i].key, field) == 0) {
                            obj.as.map_val->entries[i].value = value;
                            return value;
                        }
                    }
                    ValueMap* map = obj.as.map_val;
                    if (map->count >= map->capacity) {
                        map->capacity = map->capacity == 0 ? 4 : map->capacity * 2;
                        map->entries = (ValueMapEntry*)realloc(map->entries,
                            sizeof(ValueMapEntry) * map->capacity);
                    }
                    ValueMapEntry* e = &map->entries[map->count++];
                    e->key = vex_strdup(field, (int)strlen(field));
                    e->value = value;
                    return value;
                }
            }
            /* Index assignment: arr[i] be value */
            else if (node->as.assign.target->type == NODE_INDEX) {
                VexValue obj = eval(interp, node->as.assign.target->as.index_access.object, env);
                VexValue idx = eval(interp, node->as.assign.target->as.index_access.index, env);
                if (obj.type == VAL_ARRAY && idx.type == VAL_INT) {
                    int i = (int)idx.as.int_val;
                    if (i >= 0 && i < obj.as.array_val->count)
                        obj.as.array_val->items[i] = value;
                }
            }
            return value;
        }

        /* ── Let / Const ── */
        case NODE_LET_DECL:
        case NODE_CONST_DECL: {
            VexValue value = eval(interp, node->as.var_decl.value, env);
            env_define(env, node->as.var_decl.name, value,
                node->type == NODE_CONST_DECL);
            return vex_nothing();
        }

        /* ── Display ── */
        case NODE_DISPLAY: {
            VexValue value = eval(interp, node->as.display.value, env);
            vex_print_value(value);
            printf("\n");
            return vex_nothing();
        }

        /* ── If / elif / else ── */
        case NODE_IF: {
            VexValue cond = eval(interp, node->as.if_stmt.condition, env);
            if (vex_is_truthy(cond)) {
                return eval(interp, node->as.if_stmt.then_block, env);
            } else if (node->as.if_stmt.else_block) {
                return eval(interp, node->as.if_stmt.else_block, env);
            }
            return vex_nothing();
        }

        /* ── While ── */
        case NODE_WHILE: {
            while (!interp->had_error) {
                VexValue cond = eval(interp, node->as.while_stmt.condition, env);
                if (!vex_is_truthy(cond)) break;
                eval(interp, node->as.while_stmt.body, env);
                if (interp->signal.type == SIGNAL_BREAK) {
                    interp->signal.type = SIGNAL_NONE;
                    break;
                }
                if (interp->signal.type == SIGNAL_SKIP) {
                    interp->signal.type = SIGNAL_NONE;
                    continue;
                }
                if (interp->signal.type == SIGNAL_RETURN) break;
            }
            return vex_nothing();
        }

        /* ── For range ── */
        case NODE_FOR_RANGE: {
            VexValue start_val = eval(interp, node->as.for_range.start, env);
            VexValue end_val = eval(interp, node->as.for_range.end, env);
            int64_t step = 1;
            if (node->as.for_range.step) {
                VexValue step_val = eval(interp, node->as.for_range.step, env);
                step = step_val.as.int_val;
            }
            Environment* loop_env = env_new(env);
            int64_t start = start_val.as.int_val;
            int64_t end = end_val.as.int_val;
            for (int64_t i = start; step > 0 ? i <= end : i >= end; i += step) {
                env_define(loop_env, node->as.for_range.var_name, vex_int(i), false);
                eval(interp, node->as.for_range.body, loop_env);
                if (interp->signal.type == SIGNAL_BREAK) {
                    interp->signal.type = SIGNAL_NONE;
                    break;
                }
                if (interp->signal.type == SIGNAL_SKIP) {
                    interp->signal.type = SIGNAL_NONE;
                    continue;
                }
                if (interp->signal.type == SIGNAL_RETURN) break;
            }
            env_release(loop_env);
            return vex_nothing();
        }

        /* ── For each ── */
        case NODE_FOR_EACH: {
            VexValue iterable = eval(interp, node->as.for_each.iterable, env);
            Environment* loop_env = env_new(env);
            if (iterable.type == VAL_ARRAY && iterable.as.array_val) {
                for (int i = 0; i < iterable.as.array_val->count; i++) {
                    env_define(loop_env, node->as.for_each.var_name,
                        iterable.as.array_val->items[i], false);
                    eval(interp, node->as.for_each.body, loop_env);
                    if (interp->signal.type == SIGNAL_BREAK) {
                        interp->signal.type = SIGNAL_NONE;
                        break;
                    }
                    if (interp->signal.type == SIGNAL_SKIP) {
                        interp->signal.type = SIGNAL_NONE;
                        continue;
                    }
                    if (interp->signal.type == SIGNAL_RETURN) break;
                }
            }
            else if (iterable.type == VAL_STRING) {
                for (int i = 0; i < iterable.as.string_val.length; i++) {
                    env_define(loop_env, node->as.for_each.var_name,
                        vex_string(&iterable.as.string_val.data[i], 1), false);
                    eval(interp, node->as.for_each.body, loop_env);
                    if (interp->signal.type == SIGNAL_BREAK) { interp->signal.type = SIGNAL_NONE; break; }
                    if (interp->signal.type == SIGNAL_SKIP) { interp->signal.type = SIGNAL_NONE; continue; }
                    if (interp->signal.type == SIGNAL_RETURN) break;
                }
            }
            env_release(loop_env);
            return vex_nothing();
        }

        /* ── Repeat N times ── */
        case NODE_REPEAT: {
            VexValue count_val = eval(interp, node->as.repeat.count, env);
            int64_t n = count_val.as.int_val;
            for (int64_t i = 0; i < n; i++) {
                eval(interp, node->as.repeat.body, env);
                if (interp->signal.type == SIGNAL_BREAK) {
                    interp->signal.type = SIGNAL_NONE;
                    break;
                }
                if (interp->signal.type == SIGNAL_SKIP) {
                    interp->signal.type = SIGNAL_NONE;
                    continue;
                }
                if (interp->signal.type == SIGNAL_RETURN) break;
            }
            return vex_nothing();
        }

        /* ── Function declaration ── */
        case NODE_FN_DECL: {
            VexValue fn;
            fn.type = VAL_FN;
            fn.as.fn_val.decl = node;
            fn.as.fn_val.closure = env;
            env_retain(env);
            env_define(env, node->as.fn_decl.name, fn, false);
            return vex_nothing();
        }

        /* ── Give back (return) ── */
        case NODE_GIVE_BACK: {
            VexValue val = vex_nothing();
            if (node->as.give_back.value)
                val = eval(interp, node->as.give_back.value, env);
            interp->signal.type = SIGNAL_RETURN;
            interp->signal.return_value = val;
            return val;
        }

        /* ── Break / Skip ── */
        case NODE_BREAK:
            interp->signal.type = SIGNAL_BREAK;
            return vex_nothing();
        case NODE_SKIP:
            interp->signal.type = SIGNAL_SKIP;
            return vex_nothing();
        case NODE_PASS:
            return vex_nothing();

        /* ── Defer statement ── */
        case NODE_DEFER: {
            /* Evaluate deferred expression immediately (no real defer stack yet) */
            eval(interp, node->as.defer.expr, env);
            return vex_nothing();
        }

        /* ── Await expression ── */
        case NODE_AWAIT: {
            /* Evaluate the awaited expression (no real async support) */
            return eval(interp, node->as.await.expr, env);
        }

        /* ── Trait declaration ── */
        case NODE_TRAIT: {
            VexValue td;
            td.type = VAL_STRUCT_DEF;  /* Reuse struct def for trait */
            td.as.struct_def.name = node->as.trait_decl.name;
            td.as.struct_def.decl = node;
            env_define(env, node->as.trait_decl.name, td, false);
            return vex_nothing();
        }

        /* ── Impl block ── */
        case NODE_IMPL: {
            /* Attach impl methods to the struct */
            VexValue* struct_val = env_get(env, node->as.impl_block.struct_name);
            if (!struct_val || struct_val->type != VAL_STRUCT_DEF) {
                fprintf(stderr, "Error [line %d]: Struct '%s' not found for impl\n",
                    node->line, node->as.impl_block.struct_name);
                return vex_nothing();
            }
            
            /* Add impl methods to struct's method list */
            ASTNode* struct_decl = struct_val->as.struct_def.decl;
            for (int i = 0; i < node->as.impl_block.method_count; i++) {
                /* Grow struct's method array if needed */
                if (struct_decl->as.struct_decl.method_count >= struct_decl->as.struct_decl.method_capacity) {
                    struct_decl->as.struct_decl.method_capacity = 
                        struct_decl->as.struct_decl.method_capacity == 0 ? 4 : struct_decl->as.struct_decl.method_capacity * 2;
                    struct_decl->as.struct_decl.methods = (StructMethod*)realloc(
                        struct_decl->as.struct_decl.methods,
                        sizeof(StructMethod) * struct_decl->as.struct_decl.method_capacity);
                }
                
                /* Copy method */
                StructMethod* dest = &struct_decl->as.struct_decl.methods[struct_decl->as.struct_decl.method_count++];
                StructMethod* src = &node->as.impl_block.methods[i];
                dest->name = src->name;
                dest->params = src->params;
                dest->return_type = src->return_type;
                dest->body = src->body;
            }
            return vex_nothing();
        }

        /* ── Operator overload ── */
        case NODE_OPERATOR_OVERLOAD: {
            /* Store operator overload in struct for later use */
            VexValue* struct_val = env_get(env, node->as.operator_overload.struct_name);
            if (!struct_val || struct_val->type != VAL_STRUCT_DEF) {
                fprintf(stderr, "Error [line %d]: Struct '%s' not found for operator\n",
                    node->line, node->as.operator_overload.struct_name);
            }
            /* Operator would be attached to struct during binary op resolution */
            return vex_nothing();
        }

        /* ── Struct declaration ── */
        case NODE_STRUCT_DECL: {
            VexValue sd;
            sd.type = VAL_STRUCT_DEF;
            sd.as.struct_def.name = node->as.struct_decl.name;
            sd.as.struct_def.decl = node;
            env_define(env, node->as.struct_decl.name, sd, false);

            /* If extends parent(s), verify they exist */
            for (int p = 0; p < node->as.struct_decl.parent_count; p++) {
                VexValue* parent = env_get(env, node->as.struct_decl.parent_names[p]);
                if (!parent || parent->type != VAL_STRUCT_DEF) {
                    fprintf(stderr, "Error [line %d]: Parent struct '%s' not found\n",
                        node->line, node->as.struct_decl.parent_names[p]);
                }
                /* Parent fields are inherited — they'll be resolved at instance creation */
                /* Methods are resolved via chain in eval_method_call */
            }
            return vex_nothing();
        }

        /* ── Lambda expression ── */
        case NODE_LAMBDA: {
            VexValue fn;
            fn.type = VAL_FN;
            /* Reuse fn_val for lambda — create a synthetic fn decl node */
            ASTNode* synthetic = ast_new_node(NODE_FN_DECL, node->line, node->column);
            synthetic->as.fn_decl.name = vex_strdup("<lambda>", 8);
            synthetic->as.fn_decl.params = node->as.lambda.params;
            synthetic->as.fn_decl.return_type = NULL;
            synthetic->as.fn_decl.body = node->as.lambda.body;
            fn.as.fn_val.decl = synthetic;
            fn.as.fn_val.closure = env;
            env_retain(env);
            return fn;
        }

        /* ── List comprehension ── */
        case NODE_LIST_COMP: {
            VexValue iterable = eval(interp, node->as.list_comp.iterable, env);
            if (iterable.type != VAL_ARRAY) {
                fprintf(stderr, "Error [line %d]: List comprehension requires array\n", node->line);
                return vex_nothing();
            }
            VexValue result;
            result.type = VAL_ARRAY;
            result.as.array_val = (ValueArray*)calloc(1, sizeof(ValueArray));

            ValueArray* src = iterable.as.array_val;
            for (int i = 0; i < src->count; i++) {
                Environment* comp_env = env_new(env);
                env_define(comp_env, node->as.list_comp.var_name, src->items[i], false);

                /* Check filter condition if present */
                bool include = true;
                if (node->as.list_comp.condition) {
                    VexValue cond = eval(interp, node->as.list_comp.condition, comp_env);
                    include = vex_is_truthy(cond);
                }

                if (include) {
                    VexValue val = eval(interp, node->as.list_comp.expr, comp_env);
                    ValueArray* arr = result.as.array_val;
                    if (arr->count >= arr->capacity) {
                        arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
                        arr->items = (VexValue*)realloc(arr->items, sizeof(VexValue) * arr->capacity);
                    }
                    arr->items[arr->count++] = val;
                }
                env_release(comp_env);
            }
            return result;
        }

        /* ── Match ── */
        case NODE_MATCH: {
            VexValue expr = eval(interp, node->as.match_stmt.expr, env);
            for (int i = 0; i < node->as.match_stmt.arms.count; i++) {
                VexValue pattern = eval(interp, node->as.match_stmt.arms.items[i].pattern, env);
                bool match = false;
                if (expr.type == pattern.type) {
                    if (expr.type == VAL_INT) match = expr.as.int_val == pattern.as.int_val;
                    else if (expr.type == VAL_STRING) match =
                        strcmp(expr.as.string_val.data, pattern.as.string_val.data) == 0;
                    else if (expr.type == VAL_BOOL) match = expr.as.bool_val == pattern.as.bool_val;
                    else if (expr.type == VAL_NOTHING) match = true;
                }
                if (match) {
                    return eval(interp, node->as.match_stmt.arms.items[i].body, env);
                }
            }
            return vex_nothing();
        }

        /* ── Use module ── */
        case NODE_USE: {
            if (!stdlib_load_module(node->as.use_stmt.module_name, env)) {
                fprintf(stderr, "Error [line %d]: Unknown module '%s'\n",
                    node->line, node->as.use_stmt.module_name);
                interp->had_error = true;
            }
            return vex_nothing();
        }

        /* ── From module use symbol ── */
        case NODE_FROM_USE: {
            if (!stdlib_load_symbol(node->as.from_use.module_name,
                    node->as.from_use.import_name, env)) {
                interp->had_error = true;
            }
            return vex_nothing();
        }

        /* ── Attempt / Otherwise ── */
        case NODE_ATTEMPT: {
            /* Simple: run try block, if error run catch block */
            bool had_err_before = interp->had_error;
            interp->had_error = false;
            eval(interp, node->as.attempt.try_block, env);
            if (interp->had_error) {
                interp->had_error = false;
                if (node->as.attempt.error_name) {
                    env_define(env, node->as.attempt.error_name,
                        vex_string("error", 5), false);
                }
                eval(interp, node->as.attempt.catch_block, env);
            }
            interp->had_error = had_err_before;
            return vex_nothing();
        }

        /* ── Block ── */
        case NODE_BLOCK: {
            Environment* block_env = env_new(env);
            VexValue last = vex_nothing();
            for (int i = 0; i < node->as.block.statements.count; i++) {
                last = eval(interp, node->as.block.statements.items[i], block_env);
                if (interp->signal.type != SIGNAL_NONE) break;
            }
            env_release(block_env);
            return last;
        }

        /* ── Program ── */
        case NODE_PROGRAM: {
            VexValue last = vex_nothing();
            for (int i = 0; i < node->as.program.statements.count; i++) {
                last = eval(interp, node->as.program.statements.items[i], env);
                if (interp->signal.type != SIGNAL_NONE) break;
            }
            return last;
        }

        /* ── Expression statement ── */
        case NODE_EXPR_STMT:
            return eval(interp, node->as.expr_stmt.expr, env);

        default:
            fprintf(stderr, "Error [line %d]: Unhandled node type %d\n",
                node->line, node->type);
            return vex_nothing();
    }
}

/* ══════════════════════════════════════════════════════════════
 *  INTERPRETER INIT / RUN
 * ══════════════════════════════════════════════════════════════ */

void interpreter_init(Interpreter* interp) {
    interp->global_env = env_new(NULL);
    interp->signal.type = SIGNAL_NONE;
    interp->had_error = false;
    g_interpreter = interp;

    /* Register built-in functions */
    VexValue v;

    v.type = VAL_BUILTIN_FN; v.as.builtin_fn.func = builtin_len; v.as.builtin_fn.name = "len";
    env_define(interp->global_env, "len", v, true);

    v.type = VAL_BUILTIN_FN; v.as.builtin_fn.func = builtin_type; v.as.builtin_fn.name = "type";
    env_define(interp->global_env, "type", v, true);

    v.type = VAL_BUILTIN_FN; v.as.builtin_fn.func = builtin_input; v.as.builtin_fn.name = "input";
    env_define(interp->global_env, "input", v, true);

    v.type = VAL_BUILTIN_FN; v.as.builtin_fn.func = builtin_int_cast; v.as.builtin_fn.name = "int";
    env_define(interp->global_env, "int", v, true);

    v.type = VAL_BUILTIN_FN; v.as.builtin_fn.func = builtin_float_cast; v.as.builtin_fn.name = "float";
    env_define(interp->global_env, "float", v, true);

    v.type = VAL_BUILTIN_FN; v.as.builtin_fn.func = builtin_str_cast; v.as.builtin_fn.name = "str";
    env_define(interp->global_env, "str", v, true);

    v.type = VAL_BUILTIN_FN; v.as.builtin_fn.func = builtin_throw; v.as.builtin_fn.name = "throw";
    env_define(interp->global_env, "throw", v, true);

    v.type = VAL_BUILTIN_FN; v.as.builtin_fn.func = builtin_push; v.as.builtin_fn.name = "push";
    env_define(interp->global_env, "push", v, true);

    v.type = VAL_BUILTIN_FN; v.as.builtin_fn.func = builtin_pop; v.as.builtin_fn.name = "pop";
    env_define(interp->global_env, "pop", v, true);

    v.type = VAL_BUILTIN_FN; v.as.builtin_fn.func = builtin_range; v.as.builtin_fn.name = "range";
    env_define(interp->global_env, "range", v, true);

    /* Global constants */
    env_define(interp->global_env, "true", vex_bool(true), true);
    env_define(interp->global_env, "false", vex_bool(false), true);
}

void interpreter_free(Interpreter* interp) {
    env_release(interp->global_env);
}

void interpret_program(Interpreter* interp, ASTNode* program) {
    eval(interp, program, interp->global_env);
}

VexValue interpret(Interpreter* interp, ASTNode* node, Environment* env) {
    return eval(interp, node, env);
}
