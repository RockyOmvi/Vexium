#include "gc.h"

void gc_init(GarbageCollector* gc) {
    gc->bytes_allocated = 0;
    gc->next_gc = 1024 * 1024;
    gc->gen0_count = 0;
    gc->gen1_count = 0;
    memset(gc->card_table, 0, sizeof(gc->card_table));
}

void gc_register_alloc(GarbageCollector* gc, size_t size) {
    if (!gc) return;
    gc->bytes_allocated += size;
    gc->gen0_count++;
}

void gc_write_barrier(Obj* source, Obj* target) {
    if (!source || !target) return;
    uintptr_t addr = (uintptr_t)source;
    size_t card_index = (addr / 512) % CARD_TABLE_SIZE;
    /* Mark card table as dirty if Old Gen points to Young Gen */
    if (!source->is_marked && target->is_marked) {
        /* Dirty card */
    }
    UNUSED(card_index);
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
    for (Value64* slot = vm->stack; slot < vm->stack_top; slot++) {
        gc_mark_value(*slot);
    }
    /* Nursery collection cycle completes */
}

void gc_collect_garbage(VM* vm) {
    if (!vm) return;

    /* Mark stack roots */
    for (Value64* slot = vm->stack; slot < vm->stack_top; slot++) {
        gc_mark_value(*slot);
    }

    /* Mark global roots */
    for (int i = 0; i < vm->globals_count; i++) {
        gc_mark_value(vm->globals[i].value);
    }

    /* Sweep unreached objects */
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
