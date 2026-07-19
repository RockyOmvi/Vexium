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

#define VEXIUM_VERSION "0.1.0"
#define VEXIUM_MAX_INDENT_DEPTH 64
#define VEXIUM_TAB_WIDTH 4

#define UNUSED(x) (void)(x)

static inline char* vex_strdup(const char* src, int len) {
    char* dst = (char*)malloc(len + 1);
    memcpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

#endif
