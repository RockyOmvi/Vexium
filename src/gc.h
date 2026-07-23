#ifndef VEXIUM_GC_H
#define VEXIUM_GC_H

#include "value.h"
#include "vm.h"

#define CARD_TABLE_SIZE 1024

typedef enum {
    GC_COLOR_WHITE,
    GC_COLOR_GREY,
    GC_COLOR_BLACK
} GCColor;

typedef struct {
    size_t bytes_allocated;
    size_t next_gc;
    size_t gen0_count;
    size_t gen1_count;
    uint8_t card_table[CARD_TABLE_SIZE];
} GarbageCollector;

void gc_init(GarbageCollector* gc);
void gc_register_alloc(GarbageCollector* gc, size_t size);
void gc_mark_object(Obj* object);
void gc_mark_value(Value64 value);
void gc_write_barrier(Obj* source, Obj* target);
void gc_collect_nursery(VM* vm);
void gc_collect_garbage(VM* vm);

#endif
