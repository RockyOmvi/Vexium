#include "profiler.h"

/* Global allocation counters */
size_t g_vex_alloc_count = 0;
size_t g_vex_alloc_bytes = 0;

void profiler_start(Profiler* p) {
    p->start_time = clock();
    p->start_allocs = g_vex_alloc_count;
    p->start_bytes = g_vex_alloc_bytes;
}

void profiler_stop(Profiler* p) {
    p->end_time = clock();
}

void profiler_report(Profiler* p, const char* file_path) {
    double duration_ms = ((double)(p->end_time - p->start_time) / CLOCKS_PER_SEC) * 1000.0;
    size_t allocs = g_vex_alloc_count - p->start_allocs;
    size_t bytes = g_vex_alloc_bytes - p->start_bytes;

    printf("\n");
    printf("+-------------------------------+---------------------------+\n");
    printf("|       VEXIUM PERFORMANCE PROFILE REPORT                  |\n");
    printf("+-------------------------------+---------------------------+\n");
    printf("| Target File                   | %-25s |\n", file_path);
    printf("| Execution Time                | %-22.3f ms |\n", duration_ms);
    printf("| Heap Allocations              | %-25zu |\n", allocs);
    printf("| Memory Allocated              | %-22zu bytes|\n", bytes);
    printf("| Execution Engine              | AST Tree-Walk Interpreter |\n");
    printf("+-------------------------------+---------------------------+\n\n");
}
