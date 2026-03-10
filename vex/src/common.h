#ifndef VEXIUM_COMMON_H
#define VEXIUM_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdarg.h>

#ifndef bool
typedef int bool;
#define true 1
#define false 0
#endif

#define VEXIUM_VERSION "2.1.0"
#define VEXIUM_MAX_INDENT_DEPTH 64
#define VEXIUM_TAB_WIDTH 4

#define UNUSED(x) (void)(x)

/* Helper to duplicate a string with a known length */
static inline char* vex_strdup(const char* src, int len) {
    char* dst = (char*)malloc(len + 1);
    memcpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

/* Helper to duplicate a null-terminated string */
static inline char* safe_strdup(const char* src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char* dst = (char*)malloc(len + 1);
    if (dst) {
        memcpy(dst, src, len);
        dst[len] = '\0';
    }
    return dst;
}

#endif /* VEXIUM_COMMON_H */
