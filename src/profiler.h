#ifndef VEXIUM_PROFILER_H
#define VEXIUM_PROFILER_H

#include "common.h"
#include <time.h>

/* Global allocation counters — incremented by vex_tracked_malloc/calloc/realloc */
extern size_t g_vex_alloc_count;
extern size_t g_vex_alloc_bytes;

/* Tracked allocation wrappers */
static inline void* vex_tracked_malloc(size_t size) {
    g_vex_alloc_count++;
    g_vex_alloc_bytes += size;
    return malloc(size);
}

static inline void* vex_tracked_calloc(size_t count, size_t size) {
    g_vex_alloc_count++;
    g_vex_alloc_bytes += count * size;
    return calloc(count, size);
}

static inline void* vex_tracked_realloc(void* ptr, size_t size) {
    g_vex_alloc_count++;
    g_vex_alloc_bytes += size;
    return realloc(ptr, size);
}

typedef struct {
    clock_t start_time;
    clock_t end_time;
    size_t start_allocs;
    size_t start_bytes;
} Profiler;

void profiler_start(Profiler* p);
void profiler_stop(Profiler* p);
void profiler_report(Profiler* p, const char* file_path);

#endif
