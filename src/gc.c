#include "gc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void gc_init(GarbageCollector* gc) {
    gc->bytes_allocated = 0;
    gc->next_gc = 1024 * 1024;
    gc->gen0_alloc_count = 0;
    gc->gen1_alloc_count = 0;
    memset(gc->card_table, 0, sizeof(gc->card_table));
}

void gc_register_alloc(GarbageCollector* gc, size_t size) {
    if (!gc) return;
    gc->bytes_allocated += size;
    gc->gen0_alloc_count++;
}

void gc_mark_dirty_card(GarbageCollector* gc, void* address) {
    if (!gc || !address) return;
    uintptr_t addr = (uintptr_t)address;
    size_t card_idx = (addr >> CARD_SHIFT) % CARD_TABLE_CAPACITY;
    gc->card_table[card_idx] = 1; /* Dirty card */
}

void gc_clear_card_table(GarbageCollector* gc) {
    if (!gc) return;
    memset(gc->card_table, 0, sizeof(gc->card_table));
}

void gc_write_barrier(GarbageCollector* gc, Obj* source, Obj* target) {
    if (!gc || !source || !target) return;
    /* If an Old Gen object is mutated to store a Young Gen object, mark card dirty */
    gc_mark_dirty_card(gc, source);
}

void gc_mark_object(Obj* object) {
    if (object == NULL || object->is_marked) return;
    object->is_marked = true;

    switch (object->type) {
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)object;
            for (int i = 0; i < array->count; i++) {
                gc_mark_value(array->items[i]);
            }
            break;
        }
        case OBJ_MAP: {
            ObjMap* map = (ObjMap*)object;
            for (int i = 0; i < map->count; i++) {
                gc_mark_value(map->entries[i].value);
            }
            break;
        }
        default:
            break;
    }
}

void gc_mark_value(Value64 value) {
    if (is_obj(value)) {
        gc_mark_object(value_to_obj(value));
    }
}

void gc_collect_nursery(VM* vm) {
    if (!vm) return;

    /* 1. Mark stack roots */
    for (Value64* slot = vm->stack; slot < vm->stack_top; slot++) {
        gc_mark_value(*slot);
    }
}

void gc_collect_garbage(VM* vm) {
    if (!vm) return;

    /* 1. Mark VM stack roots */
    for (Value64* slot = vm->stack; slot < vm->stack_top; slot++) {
        gc_mark_value(*slot);
    }

    /* 2. Mark global environment roots */
    for (int i = 0; i < vm->globals_count; i++) {
        gc_mark_value(vm->globals[i].value);
    }

    /* 3. Sweep unreached objects */
    Obj** object = &vm->objects;
    while (*object != NULL) {
        if (!(*object)->is_marked) {
            Obj* unreached = *object;
            *object = unreached->next;
            free_obj(unreached);
        } else {
            (*object)->is_marked = false;
            object = &(*object)->next;
        }
    }
}
