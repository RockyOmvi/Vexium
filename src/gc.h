#ifndef VEXIUM_GC_H
#define VEXIUM_GC_H

#include "value.h"
#include "vm.h"

#define CARD_SHIFT 9                /* 512-byte cards */
#define CARD_SIZE (1 << CARD_SHIFT)
#define CARD_TABLE_CAPACITY 2048

typedef enum {
    GEN_NURSERY = 0,    /* Young Generation */
    GEN_TENURED = 1     /* Old Generation */
} Generation;

typedef struct {
    size_t bytes_allocated;
    size_t next_gc;
    size_t gen0_alloc_count;
    size_t gen1_alloc_count;
    uint8_t card_table[CARD_TABLE_CAPACITY];
} GarbageCollector;

void gc_init(GarbageCollector* gc);
void gc_register_alloc(GarbageCollector* gc, size_t size);
void gc_mark_object(Obj* object);
void gc_mark_value(Value64 value);

/* Active Write Barrier Instrumentation */
void gc_write_barrier(GarbageCollector* gc, Obj* source, Obj* target);
void gc_mark_dirty_card(GarbageCollector* gc, void* address);
void gc_clear_card_table(GarbageCollector* gc);

void gc_collect_nursery(VM* vm);
void gc_collect_garbage(VM* vm);

#endif
