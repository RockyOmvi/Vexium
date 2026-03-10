#include "gc.h"
#include <stdio.h>
#include <stdlib.h>

/* ══════════════════════════════════════════════════════════════
 *  Garbage Collector - Mark-Sweep Implementation
 * ══════════════════════════════════════════════════════════════ */

/* Mark a value (if it's an object) */
static void gc_mark_value(Value value);

/* Mark an object and its references */
static void gc_mark_object(Obj* obj) {
    if (obj == NULL) return;
    if (obj->is_marked) return;  /* Already marked */
    
    /* Mark this object */
    obj->is_marked = true;
    
    /* Mark references based on object type */
    switch (obj->type) {
        case OBJ_STRING:
            /* Strings have no references */
            break;
            
        case OBJ_FUNCTION: {
            ObjFunction* fn = (ObjFunction*)obj;
            /* Mark function name if it exists */
            /* Note: fn->name is a char*, not an ObjString */
            /* Mark constants in the function's constant pool */
            for (int i = 0; i < fn->const_count; i++) {
                gc_mark_value(fn->constants[i]);
            }
            break;
        }
        
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)obj;
            /* Mark the underlying function */
            gc_mark_object((Obj*)closure->function);
            /* Mark upvalues */
            for (int i = 0; i < closure->upvalue_count; i++) {
                gc_mark_object((Obj*)closure->upvalues[i]);
            }
            break;
        }
        
        case OBJ_UPVALUE: {
            ObjUpvalue* upvalue = (ObjUpvalue*)obj;
            /* Mark the closed value (if closed) or the value on stack */
            gc_mark_value(upvalue->closed);
            break;
        }
        
        case OBJ_ARRAY: {
            ObjArray* arr = (ObjArray*)obj;
            /* Mark all items in the array */
            for (int i = 0; i < arr->count; i++) {
                gc_mark_value(arr->items[i]);
            }
            break;
        }
        
        case OBJ_MAP: {
            ObjMap* map = (ObjMap*)obj;
            /* Mark all keys and values in the map */
            for (int i = 0; i < map->capacity; i++) {
                VMMapEntry* entry = &map->entries[i];
                if (entry->occupied && entry->key != NULL) {
                    gc_mark_object((Obj*)entry->key);
                    gc_mark_value(entry->value);
                }
            }
            break;
        }
        
        case OBJ_STRUCT_DEF: {
            ObjStructDef* def = (ObjStructDef*)obj;
            /* Mark struct name */
            gc_mark_object((Obj*)def->name);
            /* Mark field names */
            for (int i = 0; i < def->field_count; i++) {
                gc_mark_object((Obj*)def->field_names[i]);
            }
            /* Mark methods map */
            gc_mark_object((Obj*)def->methods);
            break;
        }
        
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)obj;
            /* Mark struct definition */
            gc_mark_object((Obj*)instance->struct_def);
            /* Mark fields map */
            gc_mark_object((Obj*)instance->fields);
            break;
        }
        
        case OBJ_ERROR: {
            ObjError* err = (ObjError*)obj;
            /* Mark error message and type */
            gc_mark_object((Obj*)err->message);
            gc_mark_object((Obj*)err->error_type);
            gc_mark_object((Obj*)err->file);
            break;
        }
    }
}

static void gc_mark_value(Value value) {
    if (is_obj(value)) {
        gc_mark_object((Obj*)as_obj(value));
    }
}

/* Mark all roots (stack, globals, call frames, upvalues) */
static void gc_mark_roots(VM* vm) {
    /* Mark values on the stack */
    for (Value* slot = vm->stack; slot < vm->stack_top; slot++) {
        gc_mark_value(*slot);
    }
    
    /* Mark globals */
    for (int i = 0; i < GLOBALS_MAX; i++) {
        if (vm->globals[i].occupied) {
            gc_mark_value(vm->globals[i].value);
        }
    }
    
    /* Mark call frames (closures) */
    for (int i = 0; i < vm->frame_count; i++) {
        gc_mark_object((Obj*)vm->frames[i].closure);
    }
    
    /* Mark open upvalues */
    for (ObjUpvalue* upvalue = vm->open_upvalues; 
         upvalue != NULL; 
         upvalue = upvalue->next) {
        gc_mark_object((Obj*)upvalue);
    }
}

/* Sweep unmarked objects */
static void gc_sweep(VM* vm) {
    Obj** obj_ref = &vm->objects;
    
    while (*obj_ref != NULL) {
        Obj* obj = *obj_ref;
        
        if (obj->is_marked) {
            /* Object is reachable - keep it and reset mark for next GC */
            obj->is_marked = false;
            obj_ref = &obj->next;
        } else {
            /* Object is unreachable - free it */
            *obj_ref = obj->next;
            
            /* Update bytes allocated tracking */
            size_t size = 0;
            switch (obj->type) {
                case OBJ_STRING: {
                    ObjString* s = (ObjString*)obj;
                    size = sizeof(ObjString) + s->length + 1;
                    break;
                }
                case OBJ_FUNCTION: {
                    ObjFunction* f = (ObjFunction*)obj;
                    size = sizeof(ObjFunction);
                    /* Note: we don't track the exact size of dynamic arrays */
                    break;
                }
                case OBJ_CLOSURE: {
                    ObjClosure* c = (ObjClosure*)obj;
                    size = sizeof(ObjClosure) + sizeof(ObjUpvalue*) * c->upvalue_count;
                    break;
                }
                case OBJ_UPVALUE:
                    size = sizeof(ObjUpvalue);
                    break;
                case OBJ_ARRAY: {
                    ObjArray* a = (ObjArray*)obj;
                    size = sizeof(ObjArray) + sizeof(Value) * a->capacity;
                    free(a->items);  /* Free the items array */
                    break;
                }
                case OBJ_MAP: {
                    ObjMap* m = (ObjMap*)obj;
                    size = sizeof(ObjMap) + sizeof(VMMapEntry) * m->capacity;
                    free(m->entries);  /* Free the entries array */
                    break;
                }
                case OBJ_STRUCT_DEF: {
                    ObjStructDef* d = (ObjStructDef*)obj;
                    size = sizeof(ObjStructDef) + sizeof(ObjString*) * d->field_count;
                    free(d->field_names);
                    break;
                }
                case OBJ_INSTANCE:
                    size = sizeof(ObjInstance);
                    break;
                case OBJ_ERROR: {
                    ObjError* e = (ObjError*)obj;
                    size = sizeof(ObjError);
                    break;
                }
            }
            
            if (vm->bytes_allocated >= size) {
                vm->bytes_allocated -= size;
            } else {
                vm->bytes_allocated = 0;
            }
            
            /* Free the object-specific data */
            switch (obj->type) {
                case OBJ_FUNCTION: {
                    ObjFunction* f = (ObjFunction*)obj;
                    free(f->code);
                    free(f->lines);
                    free(f->constants);
                    free(f->name);
                    break;
                }
                case OBJ_CLOSURE: {
                    ObjClosure* c = (ObjClosure*)obj;
                    free(c->upvalues);
                    break;
                }
                case OBJ_UPVALUE:
                    /* Nothing extra to free */
                    break;
                default:
                    break;
            }
            
            free(obj);
        }
    }
}

/* ══════════════════════════════════════════════════════════════
 *  Public API
 * ══════════════════════════════════════════════════════════════ */

void gc_init(VM* vm) {
    vm->bytes_allocated = 0;
    vm->next_gc = GC_INITIAL_THRESHOLD;
}

void gc_collect(VM* vm) {
#ifdef DEBUG_GC
    printf("-- GC: starting collection\n");
    size_t before = vm->bytes_allocated;
#endif
    
    /* Mark phase */
    gc_mark_roots(vm);
    
    /* Sweep phase */
    gc_sweep(vm);
    
    /* Adjust threshold for next GC */
    vm->next_gc = vm->bytes_allocated * GC_HEAP_GROW_FACTOR;
    if (vm->next_gc < GC_INITIAL_THRESHOLD) {
        vm->next_gc = GC_INITIAL_THRESHOLD;
    }
    
#ifdef DEBUG_GC
    printf("-- GC: collected %zu bytes (from %zu to %zu), next GC at %zu\n",
           before - vm->bytes_allocated, before, vm->bytes_allocated, vm->next_gc);
#endif
}

void gc_free_all(VM* vm) {
    Obj* obj = vm->objects;
    while (obj != NULL) {
        Obj* next = obj->next;
        
        /* Free object-specific data */
        switch (obj->type) {
            case OBJ_FUNCTION: {
                ObjFunction* f = (ObjFunction*)obj;
                free(f->code);
                free(f->lines);
                free(f->constants);
                free(f->name);
                break;
            }
            case OBJ_CLOSURE: {
                ObjClosure* c = (ObjClosure*)obj;
                free(c->upvalues);
                break;
            }
            case OBJ_ARRAY: {
                ObjArray* a = (ObjArray*)obj;
                free(a->items);
                break;
            }
            case OBJ_MAP: {
                ObjMap* m = (ObjMap*)obj;
                free(m->entries);
                break;
            }
            case OBJ_STRUCT_DEF: {
                ObjStructDef* d = (ObjStructDef*)obj;
                free(d->field_names);
                break;
            }
            default:
                break;
        }
        
        free(obj);
        obj = next;
    }
    vm->objects = NULL;
    vm->bytes_allocated = 0;
}

void gc_maybe_collect(VM* vm) {
    if (vm->bytes_allocated > vm->next_gc) {
        gc_collect(vm);
    }
}
