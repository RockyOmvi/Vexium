#ifndef VEXIUM_GC_H
#define VEXIUM_GC_H

#include "vm.h"

/* Garbage Collector for Vexium VM
 * 
 * Simple mark-sweep collector:
 * 1. Mark: Traverse all reachable objects from roots (stack, globals, call frames)
 * 2. Sweep: Free unmarked objects
 */

/* GC Configuration */
#define GC_HEAP_GROW_FACTOR 2
#define GC_INITIAL_THRESHOLD (1024 * 1024)  /* 1MB initial threshold */

/* Initialize GC tracking within VM */
void gc_init(VM* vm);

/* Trigger garbage collection */
void gc_collect(VM* vm);

/* Free all remaining objects (call at VM shutdown) */
void gc_free_all(VM* vm);

/* Should be called after object allocation to potentially trigger GC */
void gc_maybe_collect(VM* vm);

#endif /* VEXIUM_GC_H */
