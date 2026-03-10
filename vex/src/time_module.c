#include "time_module.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* ══════════════════════════════════════════════════════════════
 *  Time Module Implementation
 * ══════════════════════════════════════════════════════════════ */

/* time_now() → returns current timestamp as float (seconds since epoch) */
static Value native_time_now(int argCount, Value* args) {
    (void)argCount;
    (void)args;
    
    time_t now = time(NULL);
    return val_number((double)now);
}

/* time_format(timestamp, format) → returns formatted string */
/* Format uses strftime codes: %Y=year, %m=month, %d=day, %H=hour, %M=min, %S=sec */
static Value native_time_format(int argCount, Value* args) {
    if (argCount < 1 || argCount > 2) {
        return val_nothing_v();
    }
    
    /* Get timestamp */
    time_t timestamp;
    if (is_int(args[0])) {
        timestamp = (time_t)as_int(args[0]);
    } else if (is_number(args[0])) {
        timestamp = (time_t)as_number(args[0]);
    } else {
        return val_nothing_v();
    }
    
    /* Get format string (default ISO 8601) */
    const char* format = "%Y-%m-%d %H:%M:%S";
    if (argCount == 2 && is_obj(args[1])) {
        Obj* obj = (Obj*)as_obj(args[1]);
        if (obj->type == OBJ_STRING) {
            format = ((ObjString*)obj)->chars;
        }
    }
    
    struct tm* tm_info = localtime(&timestamp);
    if (!tm_info) {
        return val_nothing_v();
    }
    
    char buffer[256];
    size_t len = strftime(buffer, sizeof(buffer), format, tm_info);
    if (len == 0) {
        return val_nothing_v();
    }
    
    ObjString* result = obj_string_new(buffer, (int)len);
    return val_obj(result);
}

/* time_sleep(milliseconds) → pauses execution */
static Value native_time_sleep(int argCount, Value* args) {
    if (argCount != 1) {
        return val_nothing_v();
    }
    
    int ms;
    if (is_int(args[0])) {
        ms = as_int(args[0]);
    } else if (is_number(args[0])) {
        ms = (int)as_number(args[0]);
    } else {
        return val_nothing_v();
    }
    
    if (ms < 0) ms = 0;
    
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep(ms * 1000);
#endif
    
    return val_nothing_v();
}

/* time_clock() → returns high-resolution clock in seconds as float */
static Value native_time_clock(int argCount, Value* args) {
    (void)argCount;
    (void)args;
    
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return val_number((double)count.QuadPart / (double)freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return val_number((double)ts.tv_sec + (double)ts.tv_nsec / 1e9);
#endif
}

/* ══════════════════════════════════════════════════════════════
 *  Registration
 * ══════════════════════════════════════════════════════════════ */

void vm_register_time_module(VM* vm) {
    vm_define_native(vm, "time_now", native_time_now, 0);
    vm_define_native(vm, "time_format", native_time_format, -1);  /* variadic */
    vm_define_native(vm, "time_sleep", native_time_sleep, 1);
    vm_define_native(vm, "time_clock", native_time_clock, 0);
}
