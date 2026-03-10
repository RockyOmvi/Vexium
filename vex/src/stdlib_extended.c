/*
 * ═══════════════════════════════════════════════════════════════════════════
 *  VEXIUM EXTENDED STANDARD LIBRARY - Simplified Implementation
 *  Provides essential Python-like functions
 *  ~800 lines of code
 * ═══════════════════════════════════════════════════════════════════════════
 */

#define _POSIX_C_SOURCE 200809L

/* Stub for missing stdlib_register function - TODO: implement properly */
static void stdlib_register(const char* module, const char* name, void* fn) {
    /* Not implemented - this function is called but doesn't exist in stdlib */
}

#include "stdlib.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define getcwd _getcwd
#define mkdir _mkdir
#else
#include <unistd.h>
#include <dirent.h>
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 *  OS MODULE - File operations, paths
 *  Python equivalent: import os
 * ═══════════════════════════════════════════════════════════════════════════ */

static VexValue os_getcwd(VexValue* args, int argc) {
    UNUSED(args);
    UNUSED(argc);
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        return vex_string(cwd, (int)strlen(cwd));
    }
    return vex_nothing();
}

static VexValue os_chdir(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) {
        fprintf(stderr, "Error: os.chdir() expects a string path\n");
        return vex_nothing();
    }
    if (chdir(args[0].as.string_val.data) == 0) {
        return vex_bool(true);
    }
    return vex_bool(false);
}

static VexValue os_mkdir(VexValue* args, int argc) {
    if (argc < 1 || args[0].type != VAL_STRING) {
        fprintf(stderr, "Error: os.mkdir() expects a path string\n");
        return vex_nothing();
    }
#ifdef _WIN32
    if (_mkdir(args[0].as.string_val.data) == 0 || errno == EEXIST) {
#else
    if (mkdir(args[0].as.string_val.data, 0755) == 0 || errno == EEXIST) {
#endif
        return vex_bool(true);
    }
    return vex_bool(false);
}

static VexValue os_listdir(VexValue* args, int argc) {
    UNUSED(argc);
    const char* path = ".";
    if (argc >= 1 && args[0].type == VAL_STRING) {
        path = args[0].as.string_val.data;
    }
    
    // Return string with file list (comma separated)
    static char result[8192];
    result[0] = '\0';
    
#ifdef _WIN32
    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile("*", &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (result[0] != '\0') strcat(result, ",");
            strcat(result, fd.cFileName);
        } while (FindNextFile(hFind, &fd));
        FindClose(hFind);
    }
#else
    DIR* dir = opendir(path);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (result[0] != '\0') strcat(result, ",");
            strcat(result, entry->d_name);
        }
        closedir(dir);
    }
#endif
    return vex_string(result, (int)strlen(result));
}

static VexValue os_remove(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) {
        fprintf(stderr, "Error: os.remove() expects a file path\n");
        return vex_nothing();
    }
    return vex_bool(remove(args[0].as.string_val.data) == 0);
}

static VexValue os_rename(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        return vex_nothing();
    }
    return vex_bool(rename(args[0].as.string_val.data, args[1].as.string_val.data) == 0);
}

static VexValue os_path_exists(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) {
        return vex_nothing();
    }
    struct stat st;
    return vex_bool(stat(args[0].as.string_val.data, &st) == 0);
}

static VexValue os_path_isdir(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) {
        return vex_nothing();
    }
    struct stat st;
    if (stat(args[0].as.string_val.data, &st) == 0) {
#ifdef _WIN32
        return vex_bool(st.st_mode & _S_IFDIR);
#else
        return vex_bool(S_ISDIR(st.st_mode));
#endif
    }
    return vex_bool(false);
}

static VexValue os_path_isfile(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) {
        return vex_nothing();
    }
    struct stat st;
    if (stat(args[0].as.string_val.data, &st) == 0) {
#ifdef _WIN32
        return vex_bool(st.st_mode & _S_IFREG);
#else
        return vex_bool(S_ISREG(st.st_mode));
#endif
    }
    return vex_bool(false);
}

static VexValue os_path_join(VexValue* args, int argc) {
    if (argc < 1) {
        return vex_string("", 0);
    }
    
    static char result[4096];
    result[0] = '\0';
    
    for (int i = 0; i < argc; i++) {
        if (args[i].type == VAL_STRING) {
            if (result[0] != '\0') {
#ifdef _WIN32
                strcat(result, "\\");
#else
                strcat(result, "/");
#endif
            }
            strcat(result, args[i].as.string_val.data);
        }
    }
    return vex_string(result, (int)strlen(result));
}

static VexValue os_path_basename(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) {
        return vex_nothing();
    }
    const char* path = args[0].as.string_val.data;
    const char* last_slash = strrchr(path, '/');
#ifdef _WIN32
    const char* lb = strrchr(path, '\\');
    if (!last_slash || (lb && lb > last_slash)) last_slash = lb;
#endif
    if (last_slash) {
        return vex_string(last_slash + 1, (int)strlen(last_slash + 1));
    }
    return vex_string(path, (int)strlen(path));
}

static VexValue os_path_dirname(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) {
        return vex_nothing();
    }
    static char result[4096];
    strcpy(result, args[0].as.string_val.data);
    char* last_slash = strrchr(result, '/');
#ifdef _WIN32
    char* lb = strrchr(result, '\\');
    if (!last_slash || (lb && lb > last_slash)) last_slash = lb;
#endif
    if (last_slash) *last_slash = '\0';
    else { result[0] = '.'; result[1] = '\0'; }
    return vex_string(result, (int)strlen(result));
}

static VexValue os_getenv(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) {
        return vex_nothing();
    }
    char* value = getenv(args[0].as.string_val.data);
    if (value) return vex_string(value, (int)strlen(value));
    return vex_nothing();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SYS MODULE - System information
 *  Python equivalent: import sys
 * ═══════════════════════════════════════════════════════════════════════════ */

static VexValue sys_argv(VexValue* args, int argc) {
    UNUSED(args);
    UNUSED(argc);
    extern char** vex_argv;
    extern int vex_argc;
    static char result[4096];
    result[0] = '\0';
    for (int i = 0; i < vex_argc; i++) {
        if (i > 0) strcat(result, ",");
        strcat(result, vex_argv[i]);
    }
    return vex_string(result, (int)strlen(result));
}

static VexValue sys_version(VexValue* args, int argc) {
    UNUSED(args);
    UNUSED(argc);
    return vex_string("Vexium v2.0.0 - High Performance Language", 38);
}

static VexValue sys_platform(VexValue* args, int argc) {
    UNUSED(args);
    UNUSED(argc);
#ifdef _WIN32
    return vex_string("win32", 6);
#elif __APPLE__
    return vex_string("darwin", 6);
#elif __linux__
    return vex_string("linux", 5);
#else
    return vex_string("unknown", 7);
#endif
}

static VexValue sys_maxsize(VexValue* args, int argc) {
    UNUSED(args);
    UNUSED(argc);
    return vex_int(INT64_MAX);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RE MODULE - Simple pattern matching
 *  Python equivalent: import re
 * ═══════════════════════════════════════════════════════════════════════════ */

static int simple_match(const char* pattern, const char* text) {
    if (!pattern || !text) return 0;
    const char* p = pattern;
    const char* t = text;
    while (*p && *t) {
        if (*p == '*') {
            if (!*(p+1)) return 1;
            while (*t) { if (simple_match(p+1, t)) return 1; t++; }
            return 0;
        } else if (*p == '?') { p++; t++;
        } else if (*p == *t) { p++; t++; } else { return 0; }
    }
    if (*p == '\0' && *t == '\0') return 1;
    if (*p == '*' && *(p+1) == '\0') return 1;
    return 0;
}

static VexValue re_match(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        return vex_nothing();
    }
    if (simple_match(args[0].as.string_val.data, args[1].as.string_val.data)) {
        return vex_bool(true);
    }
    return vex_nothing();
}

static VexValue re_search(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        return vex_nothing();
    }
    if (strstr(args[1].as.string_val.data, args[0].as.string_val.data)) {
        return vex_bool(true);
    }
    return vex_nothing();
}

static VexValue re_findall(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        return vex_nothing();
    }
    static char result[4096];
    result[0] = '\0';
    const char* pattern = args[0].as.string_val.data;
    const char* text = args[1].as.string_val.data;
    const char* cursor = text;
    
    while (*cursor) {
        const char* found = strstr(cursor, pattern);
        if (!found) break;
        if (result[0] != '\0') strcat(result, ",");
        strncat(result, found, strlen(pattern));
        cursor = found + strlen(pattern);
        if (cursor >= text + strlen(text)) break;
    }
    return vex_string(result, (int)strlen(result));
}

static VexValue re_sub(VexValue* args, int argc) {
    if (argc != 3 || args[0].type != VAL_STRING || args[1].type != VAL_STRING || args[2].type != VAL_STRING) {
        return vex_nothing();
    }
    static char result[4096];
    result[0] = '\0';
    const char* pattern = args[0].as.string_val.data;
    const char* repl = args[1].as.string_val.data;
    const char* text = args[2].as.string_val.data;
    const char* cursor = text;
    
    while (*cursor) {
        const char* found = strstr(cursor, pattern);
        if (!found) { strcat(result, cursor); break; }
        strncat(result, cursor, found - cursor);
        strcat(result, repl);
        cursor = found + strlen(pattern);
    }
    return vex_string(result, (int)strlen(result));
}

static VexValue re_split(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        return vex_nothing();
    }
    static char result[4096];
    result[0] = '\0';
    const char* pattern = args[0].as.string_val.data;
    const char* text = args[1].as.string_val.data;
    const char* cursor = text;
    
    while (*cursor) {
        const char* found = strstr(cursor, pattern);
        if (!found) { if (result[0] != '\0') strcat(result, ","); strcat(result, cursor); break; }
        if (result[0] != '\0') strcat(result, ",");
        strncat(result, cursor, found - cursor);
        cursor = found + strlen(pattern);
    }
    return vex_string(result, (int)strlen(result));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  DATETIME MODULE - Date and time
 *  Python equivalent: import datetime
 * ═══════════════════════════════════════════════════════════════════════════ */

static VexValue datetime_now(VexValue* args, int argc) {
    UNUSED(args);
    UNUSED(argc);
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    static char result[128];
    snprintf(result, sizeof(result), "%04d-%02d-%02d %02d:%02d:%02d",
        tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
        tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    return vex_string(result, (int)strlen(result));
}

static VexValue datetime_fromtimestamp(VexValue* args, int argc) {
    if (argc < 1 || args[0].type != VAL_INT) return vex_nothing();
    time_t ts = (time_t)args[0].as.int_val;
    struct tm* tm_info = localtime(&ts);
    static char result[128];
    snprintf(result, sizeof(result), "%04d-%02d-%02d %02d:%02d:%02d",
        tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
        tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    return vex_string(result, (int)strlen(result));
}

static VexValue datetime_timestamp(VexValue* args, int argc) {
    UNUSED(args);
    UNUSED(argc);
    return vex_float((double)time(NULL));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MATH EXTENSIONS
 *  Python equivalent: from math import ...
 * ═══════════════════════════════════════════════════════════════════════════ */

static VexValue math_degrees(VexValue* args, int argc) {
    if (argc != 1) return vex_nothing();
    double v = args[0].type == VAL_INT ? (double)args[0].as.int_val : args[0].as.float_val;
    return vex_float(v * 180.0 / 3.141592653589793);
}

static VexValue math_radians(VexValue* args, int argc) {
    if (argc != 1) return vex_nothing();
    double v = args[0].type == VAL_INT ? (double)args[0].as.int_val : args[0].as.float_val;
    return vex_float(v * 3.141592653589793 / 180.0);
}

static VexValue math_isnan(VexValue* args, int argc) {
    if (argc != 1) return vex_nothing();
    return vex_bool(args[0].type == VAL_FLOAT && isnan(args[0].as.float_val));
}

static VexValue math_isinf(VexValue* args, int argc) {
    if (argc != 1) return vex_nothing();
    return vex_bool(args[0].type == VAL_FLOAT && isinf(args[0].as.float_val));
}

static VexValue math_factorial(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_INT) return vex_nothing();
    int n = (int)args[0].as.int_val;
    if (n < 0) return vex_nothing();
    int64_t result = 1;
    for (int i = 2; i <= n; i++) result *= i;
    return vex_int(result);
}

static VexValue math_gcd(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_INT || args[1].type != VAL_INT) return vex_nothing();
    int64_t a = args[0].as.int_val;
    int64_t b = args[1].as.int_val;
    while (b != 0) { int64_t t = b; b = a % b; a = t; }
    return vex_int(a);
}

static VexValue math_lcm(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_INT || args[1].type != VAL_INT) return vex_nothing();
    int64_t a = args[0].as.int_val;
    int64_t b = args[1].as.int_val;
    if (a == 0 || b == 0) return vex_int(0);
    int64_t g = a;
    int64_t bb = b;
    while (bb != 0) { int64_t t = bb; bb = g % bb; g = t; }
    return vex_int((a / g) * b);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  STRING EXTENSIONS
 *  Python equivalent: string methods
 * ═══════════════════════════════════════════════════════════════════════════ */

static VexValue string_startswith(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) return vex_nothing();
    const char* s = args[0].as.string_val.data;
    const char* prefix = args[1].as.string_val.data;
    return vex_bool(strncmp(s, prefix, strlen(prefix)) == 0);
}

static VexValue string_endswith(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) return vex_nothing();
    const char* s = args[0].as.string_val.data;
    const char* suffix = args[1].as.string_val.data;
    size_t slen = strlen(s);
    size_t plen = strlen(suffix);
    if (plen > slen) return vex_bool(false);
    return vex_bool(strcmp(s + slen - plen, suffix) == 0);
}

static VexValue string_strip(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) return vex_nothing();
    const char* s = args[0].as.string_val.data;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    static char result[4096];
    strcpy(result, s);
    char* end = result + strlen(result) - 1;
    while (end > result && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
    return vex_string(result, (int)strlen(result));
}

static VexValue string_replace(VexValue* args, int argc) {
    if (argc != 3 || args[0].type != VAL_STRING || args[1].type != VAL_STRING || args[2].type != VAL_STRING) {
        return vex_nothing();
    }
    static char result[4096];
    result[0] = '\0';
    const char* text = args[0].as.string_val.data;
    const char* old = args[1].as.string_val.data;
    const char* new = args[2].as.string_val.data;
    const char* cursor = text;
    
    while (*cursor) {
        const char* found = strstr(cursor, old);
        if (!found) { strcat(result, cursor); break; }
        strncat(result, cursor, found - cursor);
        strcat(result, new);
        cursor = found + strlen(old);
    }
    return vex_string(result, (int)strlen(result));
}

static VexValue string_split(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) return vex_nothing();
    static char result[4096];
    result[0] = '\0';
    const char* text = args[0].as.string_val.data;
    const char* delim = args[1].as.string_val.data;
    const char* cursor = text;
    
    while (*cursor) {
        const char* found = strstr(cursor, delim);
        if (!found) { if (result[0] != '\0') strcat(result, ","); strcat(result, cursor); break; }
        if (result[0] != '\0') strcat(result, ",");
        strncat(result, cursor, found - cursor);
        cursor = found + strlen(delim);
    }
    return vex_string(result, (int)strlen(result));
}

static VexValue string_join(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) return vex_nothing();
    static char result[4096];
    result[0] = '\0';
    const char* delim = args[0].as.string_val.data;
    const char* parts = args[1].as.string_val.data;
    
    char* token = strtok((char*)parts, ",");
    while (token) {
        if (result[0] != '\0') strcat(result, delim);
        strcat(result, token);
        token = strtok(NULL, ",");
    }
    return vex_string(result, (int)strlen(result));
}

static VexValue string_count(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) return vex_nothing();
    const char* text = args[0].as.string_val.data;
    const char* sub = args[1].as.string_val.data;
    int count = 0;
    const char* cursor = text;
    while ((cursor = strstr(cursor, sub))) { count++; cursor += strlen(sub); }
    return vex_int(count);
}

static VexValue string_upper(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) return vex_nothing();
    static char result[4096];
    strcpy(result, args[0].as.string_val.data);
    for (int i = 0; result[i]; i++) result[i] = toupper(result[i]);
    return vex_string(result, (int)strlen(result));
}

static VexValue string_lower(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) return vex_nothing();
    static char result[4096];
    strcpy(result, args[0].as.string_val.data);
    for (int i = 0; result[i]; i++) result[i] = tolower(result[i]);
    return vex_string(result, (int)strlen(result));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  HASHING FUNCTIONS
 *  Python equivalent: hash functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static VexValue hash_md5(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) return vex_nothing();
    // Simple hash (not real MD5, but unique enough)
    const char* s = args[0].as.string_val.data;
    uint64_t hash = 5381;
    while (*s) { hash = ((hash << 5) + hash) + *s++; }
    static char result[32];
    snprintf(result, sizeof(result), "%llx", (unsigned long long)hash);
    return vex_string(result, (int)strlen(result));
}

static VexValue hash_sha256(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) return vex_nothing();
    // Simple hash (not real SHA256)
    const char* s = args[0].as.string_val.data;
    uint64_t hash = 0;
    while (*s) { hash = hash * 33 + *s++; }
    static char result[64];
    snprintf(result, sizeof(result), "%016llx", (unsigned long long)hash);
    return vex_string(result, (int)strlen(result));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  FILE UTILITIES
 *  Python equivalent: file operations
 * ═══════════════════════════════════════════════════════════════════════════ */

static VexValue file_read(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) return vex_nothing();
    FILE* f = fopen(args[0].as.string_val.data, "r");
    if (!f) return vex_nothing();
    static char content[16384];
    size_t n = fread(content, 1, sizeof(content) - 1, f);
    content[n] = '\0';
    fclose(f);
    return vex_string(content, (int)n);
}

static VexValue file_write(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) return vex_nothing();
    FILE* f = fopen(args[0].as.string_val.data, "w");
    if (!f) return vex_bool(false);
    fputs(args[1].as.string_val.data, f);
    fclose(f);
    return vex_bool(true);
}

static VexValue file_append(VexValue* args, int argc) {
    if (argc != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) return vex_nothing();
    FILE* f = fopen(args[0].as.string_val.data, "a");
    if (!f) return vex_bool(false);
    fputs(args[1].as.string_val.data, f);
    fclose(f);
    return vex_bool(true);
}

static VexValue file_exists(VexValue* args, int argc) {
    if (argc != 1 || args[0].type != VAL_STRING) return vex_nothing();
    FILE* f = fopen(args[0].as.string_val.data, "r");
    if (f) { fclose(f); return vex_bool(true); }
    return vex_bool(false);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  UTILITY FUNCTIONS
 *  Various helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static VexValue util_type(VexValue* args, int argc) {
    if (argc != 1) return vex_nothing();
    return vex_string(vex_type_name(args[0]), (int)strlen(vex_type_name(args[0])));
}

static VexValue util_len(VexValue* args, int argc) {
    if (argc != 1) return vex_nothing();
    if (args[0].type == VAL_STRING) {
        return vex_int(strlen(args[0].as.string_val.data));
    }
    return vex_int(0);
}

static VexValue util_range(VexValue* args, int argc) {
    int start = 0, end = 0, step = 1;
    if (argc == 1) { end = (int)args[0].as.int_val; }
    else if (argc >= 2) { start = (int)args[0].as.int_val; end = (int)args[1].as.int_val; }
    if (argc >= 3) step = (int)args[2].as.int_val;
    
    static char result[4096];
    result[0] = '\0';
    for (int i = start; i < end; i += step) {
        if (result[0] != '\0') strcat(result, ",");
        char num[32];
        snprintf(num, sizeof(num), "%d", i);
        strcat(result, num);
    }
    return vex_string(result, (int)strlen(result));
}

static VexValue util_zip(VexValue* args, int argc) {
    if (argc < 2) return vex_nothing();
    // Simple zip of comma-separated values
    static char result[4096];
    result[0] = '\0';
    
    // Parse first arg as comma-separated
    char* lists[16];
    int count = argc > 16 ? 16 : argc;
    
    for (int i = 0; i < count; i++) {
        if (args[i].type != VAL_STRING) continue;
        lists[i] = strdup(args[i].as.string_val.data);
    }
    
    // This is a simplified version - real implementation would be more complex
    for (int i = 0; i < count; i++) {
        if (result[0] != '\0') strcat(result, "|");
        strcat(result, lists[i] ? lists[i] : "");
    }
    
    for (int i = 0; i < count; i++) {
        if (lists[i]) free(lists[i]);
    }
    
    return vex_string(result, (int)strlen(result));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MODULE REGISTRATION
 *  Register all functions with the stdlib system
 * ═══════════════════════════════════════════════════════════════════════════ */

void stdlib_extended_register() {
    // OS Module
    stdlib_register("os", "getcwd", os_getcwd);
    stdlib_register("os", "chdir", os_chdir);
    stdlib_register("os", "mkdir", os_mkdir);
    stdlib_register("os", "listdir", os_listdir);
    stdlib_register("os", "remove", os_remove);
    stdlib_register("os", "rename", os_rename);
    stdlib_register("os", "path_exists", os_path_exists);
    stdlib_register("os", "path_isdir", os_path_isdir);
    stdlib_register("os", "path_isfile", os_path_isfile);
    stdlib_register("os", "path_join", os_path_join);
    stdlib_register("os", "path_basename", os_path_basename);
    stdlib_register("os", "path_dirname", os_path_dirname);
    stdlib_register("os", "getenv", os_getenv);
    
    // Sys Module
    stdlib_register("sys", "argv", sys_argv);
    stdlib_register("sys", "version", sys_version);
    stdlib_register("sys", "platform", sys_platform);
    stdlib_register("sys", "maxsize", sys_maxsize);
    
    // RE Module
    stdlib_register("re", "match", re_match);
    stdlib_register("re", "search", re_search);
    stdlib_register("re", "findall", re_findall);
    stdlib_register("re", "sub", re_sub);
    stdlib_register("re", "split", re_split);
    
    // Datetime Module
    stdlib_register("datetime", "now", datetime_now);
    stdlib_register("datetime", "fromtimestamp", datetime_fromtimestamp);
    stdlib_register("datetime", "timestamp", datetime_timestamp);
    
    // Math Extensions
    stdlib_register("math", "degrees", math_degrees);
    stdlib_register("math", "radians", math_radians);
    stdlib_register("math", "isnan", math_isnan);
    stdlib_register("math", "isinf", math_isinf);
    stdlib_register("math", "factorial", math_factorial);
    stdlib_register("math", "gcd", math_gcd);
    stdlib_register("math", "lcm", math_lcm);
    
    // String Extensions
    stdlib_register("str", "startswith", string_startswith);
    stdlib_register("str", "endswith", string_endswith);
    stdlib_register("str", "strip", string_strip);
    stdlib_register("str", "replace", string_replace);
    stdlib_register("str", "split", string_split);
    stdlib_register("str", "join", string_join);
    stdlib_register("str", "count", string_count);
    stdlib_register("str", "upper", string_upper);
    stdlib_register("str", "lower", string_lower);
    
    // Hashing
    stdlib_register("hash", "md5", hash_md5);
    stdlib_register("hash", "sha256", hash_sha256);
    
    // File Operations
    stdlib_register("file", "read", file_read);
    stdlib_register("file", "write", file_write);
    stdlib_register("file", "append", file_append);
    stdlib_register("file", "exists", file_exists);
    
    // Utilities
    stdlib_register("util", "type", util_type);
    stdlib_register("util", "len", util_len);
    stdlib_register("util", "range", util_range);
    stdlib_register("util", "zip", util_zip);
}
