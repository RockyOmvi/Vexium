#ifndef _WIN32
#define _POSIX_C_SOURCE 199309L
#endif
#ifdef _WIN32

#define TokenType WindowsTokenType
#include <windows.h>
#undef TokenType
#endif
#include "stdlib.h"
#include <math.h>
#include <time.h>

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

static void arr_push(ValueArray* arr, VexValue val) {
    if (arr->count >= arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
        arr->items = (VexValue*)realloc(arr->items, sizeof(VexValue) * arr->capacity);
    }
    arr->items[arr->count++] = val;
}

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

static VexValue coll_insert(VexValue* args, int argc) {
    if (argc != 3 || args[0].type != VAL_ARRAY || args[1].type != VAL_INT) {
        fprintf(stderr, "Error: insert() expects (array, index, value)\n"); return vex_nothing();
    }
    ValueArray* arr = args[0].as.array_val;
    int idx = (int)args[1].as.int_val;
    if (idx < 0) idx += arr->count;
    if (idx < 0 || idx > arr->count) { fprintf(stderr, "Error: insert() index out of bounds\n"); return vex_nothing(); }

    arr_push(arr, vex_nothing());

    for (int i = arr->count - 1; i > idx; i--) arr->items[i] = arr->items[i-1];
    arr->items[idx] = args[2];
    return vex_nothing();
}

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

    for (int i = src->count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        VexValue tmp = result.as.array_val->items[i];
        result.as.array_val->items[i] = result.as.array_val->items[j];
        result.as.array_val->items[j] = tmp;
    }
    return result;
}

static VexValue algo_gcd(VexValue* args, int argc) {
    if (argc != 2) { fprintf(stderr, "Error: gcd() expects 2 arguments\n"); return vex_nothing(); }
    int64_t a = args[0].as.int_val, b = args[1].as.int_val;
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) { int64_t t = b; b = a % b; a = t; }
    return vex_int(a);
}

static VexValue algo_lcm(VexValue* args, int argc) {
    if (argc != 2) { fprintf(stderr, "Error: lcm() expects 2 arguments\n"); return vex_nothing(); }
    int64_t a = args[0].as.int_val, b = args[1].as.int_val;
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    if (a == 0 || b == 0) return vex_int(0);

    int64_t ga = a, gb = b;
    while (gb != 0) { int64_t t = gb; gb = ga % gb; ga = t; }
    return vex_int(a / ga * b);
}

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

static VexValue algo_fibonacci(VexValue* args, int argc) {
    if (argc != 1) { fprintf(stderr, "Error: fibonacci() expects 1 argument\n"); return vex_nothing(); }
    int64_t n = args[0].as.int_val;
    if (n <= 0) return vex_int(0);
    if (n == 1) return vex_int(1);
    int64_t a = 0, b = 1;
    for (int64_t i = 2; i <= n; i++) { int64_t t = a + b; a = b; b = t; }
    return vex_int(b);
}

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

static VexValue algo_lerp(VexValue* args, int argc) {
    if (argc != 3) { fprintf(stderr, "Error: lerp() expects 3 arguments (a, b, t)\n"); return vex_nothing(); }
    double a = args[0].type == VAL_INT ? (double)args[0].as.int_val : args[0].as.float_val;
    double b = args[1].type == VAL_INT ? (double)args[1].as.int_val : args[1].as.float_val;
    double t = args[2].type == VAL_INT ? (double)args[2].as.int_val : args[2].as.float_val;
    return vex_float(a + (b - a) * t);
}

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

typedef struct {
    const char* module_name;
    StdlibEntry* entries;
} ModuleTable;

static ModuleTable modules[] = {
    {"math",        math_entries},
    {"string",      string_entries},
    {"file",        file_entries},
    {"sys",         sys_entries},
    {"collections", collections_entries},
    {"algo",        algo_entries},
    {NULL, NULL}
};

static void register_builtin(Environment* env, const char* name, BuiltinFn func) {
    VexValue val;
    val.type = VAL_BUILTIN_FN;
    val.as.builtin_fn.func = func;
    val.as.builtin_fn.name = name;
    env_define(env, name, val, true);
}

bool stdlib_load_module(const char* module_name, Environment* env) {

    static bool rand_seeded = false;
    if (!rand_seeded && strcmp(module_name, "math") == 0) {
        srand((unsigned int)time(NULL));
        rand_seeded = true;
    }

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
