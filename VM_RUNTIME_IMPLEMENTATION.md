# VM Runtime Implementation Guide

> **For developers implementing the Vex v2 bytecode VM and runtime system**

---

## Overview

The Vex v2 runtime executes bytecode using:
- **Stack-based VM** with 40+ opcodes
- **NaN-boxing value representation** (8 bytes per value)
- **Generational garbage collection** (Gen0, Gen1, Gen2)
- **Call stack** for function frames
- **Constant pool** for string/number literals

This guide specifies VM architecture and implementation details.

---

## NaN-Boxing Value Representation

### Layout (64-bit IEEE 754 Double)

```
IEEE 754 Double: SEEEEEEEEEEMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
              Sign bit, 11 exponent bits, 52 mantissa bits

Values use quiet NaN (0x7FFC prefix) with type tagging in mantissa:

Quiet NaN Prefix:  0x7FFC000000000000
Type Tags:         0x0000NNNNNNNNNNNN (16 bits for type)
Payload:           0x000000000000XXXX (48 bits for data/pointer)

Encoding for Vex values:

1. Small integers (INT tag):
   PATTERN: 0x7FFC000000000XXX
   Example: 42 → 0x7FFC00000000002A
   Range: -2^47 to 2^47-1 (plenty for 64-bit integers)

2. Booleans:
   True:  0x7FFC000000000001
   False: 0x7FFC000000000000
   Nothing/nil: 0x7FFC000000000002

3. Floats (normal IEEE 754):
   Any non-NaN double value
   Example: 3.14 → 0x400921FB54442D18

4. Pointers (OBJ tag):
   On 64-bit systems: 0x7FFC00000000XXXX
   Where XXXX is pointer to heap object
   Handles: strings, arrays, maps, structs, functions

5. NaN values themselves:
   Signaling NaN → runtime error "not a number"
```

### Implementation

```c
// Value type encoding
#define VALUE_TAG_INT       0xFFC0      // Integer tag
#define VALUE_TAG_BOOL      0xFFD0      // Boolean tag
#define VALUE_TAG_OBJ       0xFFE0      // Pointer to object
#define VALUE_TAG_NOTHING   0xFFF0      // nil/nothing

#define NAN_BOX_MASK        0x7FFC000000000000ULL
#define IS_QUIET_NAN        0x7FFC000000000000ULL
#define PAYLOAD_MASK        0x0000FFFFFFFFFFFFULL

typedef uint64_t Value;

// Value constructor functions
Value VALUE_INT(int64_t i) {
    // Encode integer: sign + magnitude in lower 48 bits
    return IS_QUIET_NAN | (VALUE_TAG_INT << 48) | (uint64_t)(i & 0xFFFFFFFFFFFF);
}

Value VALUE_FLOAT(double f) {
    // Floats are IEEE 754 - can't be NaN with our signature
    uint64_t bits = *(uint64_t*)&f;
    // If actual NaN, convert to our NaN-tagged value
    if (isnan(f)) {
        return IS_QUIET_NAN | (VALUE_TAG_OBJ << 48);
    }
    return bits;
}

Value VALUE_BOOL(bool b) {
    return IS_QUIET_NAN | (VALUE_TAG_BOOL << 48) | (b ? 1ULL : 0ULL);
}

Value VALUE_NOTHING() {
    return IS_QUIET_NAN | (VALUE_TAG_NOTHING << 48);
}

Value VALUE_OBJ(Obj* obj) {
    return IS_QUIET_NAN | (VALUE_TAG_OBJ << 48) | (uintptr_t)obj;
}

// Value extractor functions
bool is_int(Value v) {
    return (v >> 48) == VALUE_TAG_INT;
}

bool is_float(Value v) {
    return (v & NAN_BOX_MASK) != IS_QUIET_NAN;
}

bool is_bool(Value v) {
    return (v >> 48) == VALUE_TAG_BOOL;
}

bool is_nothing(Value v) {
    return v == (IS_QUIET_NAN | (VALUE_TAG_NOTHING << 48));
}

bool is_obj(Value v) {
    return (v >> 48) == VALUE_TAG_OBJ;
}

int64_t as_int(Value v) {
    int64_t payload = v & PAYLOAD_MASK;
    // Sign extend from 48 bits
    if (payload & 0x0000800000000000ULL) {
        payload |= 0xFFFF000000000000ULL;
    }
    return payload;
}

double as_float(Value v) {
    return *(double*)&v;
}

bool as_bool(Value v) {
    return (v & 1ULL) != 0;
}

Obj* as_obj(Value v) {
    return (Obj*)(v & PAYLOAD_MASK);
}
```

---

## Bytecode Instructions (Opcodes)

```c
typedef enum {
    // Stack operations
    OP_POP,              // Pop top of stack
    OP_DUP,              // Duplicate top of stack
    OP_SWAP,             // Swap top two stack values
    
    // Constants
    OP_CONST,            // Load constant from pool
    OP_INT,              // Push small integer
    OP_TRUE,             // Push bool true
    OP_FALSE,            // Push bool false
    OP_NOTHING,          // Push nil/nothing
    
    // Variables
    OP_DEFINE_GLOBAL,    // Define global variable
    OP_GET_GLOBAL,       // Load global variable
    OP_SET_GLOBAL,       // Store global variable
    OP_DEFINE_LOCAL,     // Define local variable (frame slot)
    OP_GET_LOCAL,        // Load local variable
    OP_SET_LOCAL,        // Store local variable
    
    // Arithmetic
    OP_ADD,              // +
    OP_SUB,              // -
    OP_MUL,              // *
    OP_DIV,              // /
    OP_MOD,              // %
    OP_POW,              // **
    OP_NEG,              // Unary -
    
    // Comparison & Logic
    OP_LT,               // <
    OP_GT,               // >
    OP_LTE,              // <=
    OP_GTE,              // >=
    OP_EQ,               // ==
    OP_NEQ,              // !=
    OP_AND,              // && (logical and)
    OP_OR,               // || (logical or)
    OP_NOT,              // ! (logical not)
    
    // Control flow
    OP_JMP,              // Unconditional jump
    OP_JMP_IF_FALSE,     // Jump if stack top is falsy
    OP_JMP_IF_TRUE,      // Jump if stack top is truthy
    
    // Collections
    OP_ARRAY,            // Create array (pop n elements)
    OP_MAP,              // Create map (pop n key-value pairs)
    OP_INDEX,            // Index into array/map
    OP_INDEX_SET,        // Set index in array/map
    
    // String
    OP_CONCAT,           // String concatenation
    OP_INTERPOLATE,      // String interpolation
    
    // Functions
    OP_DEFINE_FN,        // Define function
    OP_CALL,             // Call function
    OP_RETURN,           // Return from function
    
    // Type checking
    OP_TYPE_CHECK,       // Assert value is specific type
    
    // Error handling
    OP_ATTEMPT,          // Try block
    OP_OTHERWISE,        // Catch/error handler
    OP_THROW,            // Throw error
    
    // VM control
    OP_HALT,             // End of program
} OpCode;
```

---

## VM Structure

```c
typedef struct {
    uint8_t* instructions;         // Bytecode instructions
    Value* constants;              // Constant pool
    int instruction_count;
    int constant_count;
    int constant_capacity;
} Chunk;

typedef struct {
    Value* stack;                  // Value stack (grows upward)
    int sp;                        // Stack pointer (next free slot)
    
    Table<string, Value> globals;  // Global variables
    
    Value* frames;                 // Call frames (each function call)
    int frame_count;
    int frame_capacity;
    
    uint8_t* ip;                   // Instruction pointer (current bytecode)
    
    // GC state
    Obj* objects;                  // Linked list of heap objects
    size_t bytes_allocated;
    size_t next_gc_threshold;
    
    // Constant pool for current chunk
    Chunk* chunk;
} VM;

typedef struct {
    Value* bp;                     // Base pointer (frame start)
    int locals;                    // Local variable slots
    uint8_t* return_ip;            // Instruction to return to
} Frame;
```

---

## VM Execution Loop

```c
typedef enum {
    INTERPET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

InterpretResult vm_run(VM* vm) {
    #define READ_BYTE()     (*vm->ip++)
    #define READ_SHORT()    (vm->ip += 2, (vm->ip[-2] << 8) | vm->ip[-1])
    #define READ_CONST()    (vm->chunk->constants[READ_SHORT()])
    
    #define PUSH(v)         (vm->stack[vm->sp++] = (v))
    #define POP()           (vm->stack[--vm->sp])
    #define PEEK()          (vm->stack[vm->sp - 1])
    
    while (true) {
        // Fetch-decode-execute cycle
        uint8_t opcode = READ_BYTE();
        
        switch (opcode) {
            case OP_CONST: {
                Value constant = READ_CONST();
                PUSH(constant);
                break;
            }
            
            case OP_INT: {
                // Inline small integer (5-bit value)
                int val = READ_BYTE();
                PUSH(VALUE_INT(val));
                break;
            }
            
            case OP_TRUE:
                PUSH(VALUE_BOOL(true));
                break;
                
            case OP_FALSE:
                PUSH(VALUE_BOOL(false));
                break;
                
            case OP_NOTHING:
                PUSH(VALUE_NOTHING());
                break;
            
            case OP_POP:
                POP();
                break;
            
            case OP_ADD: {
                Value b = POP();
                Value a = POP();
                
                if (is_int(a) && is_int(b)) {
                    int64_t result = as_int(a) + as_int(b);
                    PUSH(VALUE_INT(result));
                } else {
                    double result = as_float(a) + as_float(b);
                    PUSH(VALUE_FLOAT(result));
                }
                break;
            }
            
            case OP_SUB: {
                Value b = POP();
                Value a = POP();
                
                if (is_int(a) && is_int(b)) {
                    int64_t result = as_int(a) - as_int(b);
                    PUSH(VALUE_INT(result));
                } else {
                    double result = as_float(a) - as_float(b);
                    PUSH(VALUE_FLOAT(result));
                }
                break;
            }
            
            case OP_MUL: {
                Value b = POP();
                Value a = POP();
                
                if (is_int(a) && is_int(b)) {
                    // Check for overflow
                    int64_t x = as_int(a), y = as_int(b);
                    // Overflow: switch to float
                    PUSH(VALUE_FLOAT((double)x * (double)y));
                } else {
                    double result = as_float(a) * as_float(b);
                    PUSH(VALUE_FLOAT(result));
                }
                break;
            }
            
            case OP_DIV: {
                Value b = POP();
                Value a = POP();
                
                double denom = as_float(b);
                if (denom == 0.0) {
                    runtime_error("Division by zero");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                double result = as_float(a) / denom;
                PUSH(VALUE_FLOAT(result));
                break;
            }
            
            case OP_EQ: {
                Value b = POP();
                Value a = POP();
                
                bool equal;
                if (is_int(a) && is_int(b)) {
                    equal = as_int(a) == as_int(b);
                } else if (is_float(a) && is_float(b)) {
                    equal = as_float(a) == as_float(b);
                } else if (is_bool(a) && is_bool(b)) {
                    equal = as_bool(a) == as_bool(b);
                } else if (is_nothing(a) && is_nothing(b)) {
                    equal = true;
                } else {
                    equal = false;
                }
                
                PUSH(VALUE_BOOL(equal));
                break;
            }
            
            case OP_LT: {
                Value b = POP();
                Value a = POP();
                double result = as_float(a) < as_float(b);
                PUSH(VALUE_BOOL(result));
                break;
            }
            
            case OP_JMP: {
                uint16_t offset = READ_SHORT();
                vm->ip += offset;
                break;
            }
            
            case OP_JMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                Value condition = POP();
                
                if (!is_truthy(condition)) {
                    vm->ip += offset;
                }
                break;
            }
            
            case OP_CALL: {
                uint8_t arg_count = READ_BYTE();
                Value callee = vm->stack[vm->sp - 1 - arg_count];
                
                if (!is_obj(callee)) {
                    runtime_error("Can only call functions");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                Obj* obj = as_obj(callee);
                if (obj->type != OBJ_FUNCTION) {
                    runtime_error("Can only call functions");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                ObjFunction* fn = (ObjFunction*)obj;
                
                if (arg_count != fn->arity) {
                    runtime_error("Expected %d arguments, got %d",
                                fn->arity, arg_count);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (!call_function(vm, fn, arg_count)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            
            case OP_RETURN: {
                Value result = POP();
                
                // Pop frame
                vm->frame_count--;
                if (vm->frame_count == 0) {
                    // Program end
                    POP();  // Pop <script> sentinel
                    return INTERPRET_OK;
                }
                
                Frame* frame = &vm->frames[vm->frame_count];
                
                vm->sp = (int)(frame->bp - vm->stack);
                PUSH(result);
                vm->ip = frame->return_ip;
                break;
            }
            
            case OP_HALT:
                return INTERPRET_OK;
            
            default:
                runtime_error("Unknown opcode: %d", opcode);
                return INTERPRET_RUNTIME_ERROR;
        }
    }
    
    return INTERPRET_OK;
}

bool is_truthy(Value v) {
    if (is_nothing(v)) return false;
    if (is_bool(v)) return as_bool(v);
    return true;
}
```

---

## Generational Garbage Collection

```c
// Mark-sweep with generations
typedef struct {
    Obj* objects;                  // All heap objects
    Obj** gray_stack;              // Objects to process
    int gray_count, gray_capacity;
    
    // Generational state
    Obj* gen0_objects;             // Young generation (recently created)
    Obj* gen1_objects;             // Intermediate
    Obj* gen2_objects;             // Old generation
    
    size_t bytes_allocated;
    size_t next_gc_threshold;
} GC;

void gc_mark(Value value) {
    if (is_obj(value)) {
        Obj* obj = as_obj(value);
        mark_object(obj);
    }
}

void mark_object(Obj* obj) {
    if (obj == NULL || obj->is_marked) return;
    
    obj->is_marked = true;
    
    // Add to gray stack for processing
    gc.gray_stack[gc.gray_count++] = obj;
}

void gc_mark_roots(VM* vm) {
    // Stack values
    for (int i = 0; i < vm->sp; i++) {
        gc_mark(vm->stack[i]);
    }
    
    // Global variables
    for (int i = 0; i < vm->globals.capacity; i++) {
        if (vm->globals.entries[i].key != NULL) {
            gc_mark(vm->globals.entries[i].value);
        }
    }
}

void gc_mark_grays() {
    while (gc.gray_count > 0) {
        Obj* obj = gc.gray_stack[--gc.gray_count];
        gc_blacken(obj);
    }
}

void gc_blacken(Obj* obj) {
    switch (obj->type) {
        case OBJ_STRING:
            // Strings have no references
            break;
        
        case OBJ_ARRAY: {
            ObjArray* arr = (ObjArray*)obj;
            for (int i = 0; i < arr->count; i++) {
                gc_mark(arr->elements[i]);
            }
            break;
        }
        
        case OBJ_MAP: {
            ObjMap* map = (ObjMap*)obj;
            for (int i = 0; i < map->capacity; i++) {
                if (map->entries[i].key != NULL) {
                    gc_mark(map->entries[i].key);
                    gc_mark(map->entries[i].value);
                }
            }
            break;
        }
        
        case OBJ_FUNCTION: {
            ObjFunction* fn = (ObjFunction*)obj;
            // Mark captured variables (closures)
            for (int i = 0; i < fn->closure_vars.count; i++) {
                gc_mark(fn->closure_vars.values[i]);
            }
            break;
        }
    }
}

void gc_sweep() {
    Obj** object = &gc.objects;
    while (*object != NULL) {
        if ((*object)->is_marked) {
            (*object)->is_marked = false;  // Reset for next cycle
            object = &(*object)->next;
        } else {
            // Collect this object
            Obj* unreached = *object;
            *object = unreached->next;
            free_object(unreached);
        }
    }
}

void gc_collect(VM* vm) {
    size_t before = gc.bytes_allocated;
    
    gc_mark_roots(vm);
    gc_mark_grays();
    gc_sweep();
    
    gc.next_gc_threshold = gc.bytes_allocated * 2;
    
    #ifdef DEBUG_GC
    printf("[GC] Collected: %zu bytes (before %zu, now %zu)\n",
        before - gc.bytes_allocated, before, gc.bytes_allocated);
    #endif
}

void* gc_allocate(void* pointer, size_t old_size, size_t new_size) {
    gc.bytes_allocated += new_size - old_size;
    
    if (new_size > old_size && gc.bytes_allocated > gc.next_gc_threshold) {
        gc_collect(NULL);  // TODO: pass VM
    }
    
    if (new_size == 0) {
        free(pointer);
        return NULL;
    }
    
    return realloc(pointer, new_size);
}
```

---

## Function Definitions and Calls

```c
typedef struct {
    Chunk chunk;
    int arity;
    ObjString* name;
    int closure_vars_count;
} ObjFunction;

// Emit function definition bytecode
void compile_function(Compiler* compiler, Stmt* stmt) {
    ObjFunction* fn = allocate<ObjFunction>();
    fn->arity = stmt->as_function.params.count;
    fn->name = copy_string(stmt->as_function.name, strlen(stmt->as_function.name));
    
    // Compile function body in new chunk
    Chunk saved_chunk = compiler->chunk;
    compiler->chunk = (Chunk){0};
    
    // Compile all statements in function body
    for (int i = 0; i < stmt->as_function.body.count; i++) {
        compile_statement(compiler, stmt->as_function.body.stmts[i]);
    }
    
    // Implicit return nothing
    if (compiler->chunk.instruction_count == 0 ||
        compiler->chunk.instructions[compiler->chunk.instruction_count - 1] != OP_RETURN) {
        emit_byte(compiler, OP_NOTHING);
        emit_byte(compiler, OP_RETURN);
    }
    
    fn->chunk = compiler->chunk;
    compiler->chunk = saved_chunk;
    
    // Emit opcode to define function
    emit_constant(compiler, VALUE_OBJ((Obj*)fn));
}

bool call_function(VM* vm, ObjFunction* fn, int arg_count) {
    // Check frame capacity
    if (vm->frame_count >= FRAMES_MAX) {
        runtime_error("Stack overflow");
        return false;
    }
    
    // Create new frame
    Frame* frame = &vm->frames[vm->frame_count++];
    frame->bp = vm->stack + vm->sp - arg_count;  // Base of arguments
    frame->locals = fn->arity;
    frame->return_ip = vm->ip;
    
    // Switch to function code
    vm->ip = fn->chunk.instructions;
    vm->chunk = &fn->chunk;
    
    return true;
}
```

---

## VM Initialization and Cleanup

```c
VM* vm_create(void) {
    VM* vm = malloc(sizeof(VM));
    
    // Stack (assuming 256KB for execution)
    vm->stack = malloc(sizeof(Value) * STACK_SIZE);
    vm->sp = 0;
    
    // Frames
    vm->frames = malloc(sizeof(Frame) * FRAMES_MAX);
    vm->frame_count = 0;
    
    // Global variables table
    table_init(&vm->globals);
    
    // GC
    vm->objects = NULL;
    vm->bytes_allocated = 0;
    vm->next_gc_threshold = 1024 * 1024;  // 1MB initial threshold
    
    return vm;
}

void vm_free(VM* vm) {
    table_free(&vm->globals);
    
    Obj* obj = vm->objects;
    while (obj != NULL) {
        Obj* next = obj->next;
        free_object(obj);
        obj = next;
    }
    
    free(vm->stack);
    free(vm->frames);
    free(vm);
}

void free_object(Obj* obj) {
    switch (obj->type) {
        case OBJ_STRING: {
            ObjString* str = (ObjString*)obj;
            free(str->chars);
            free(str);
            break;
        }
        case OBJ_ARRAY: {
            ObjArray* arr = (ObjArray*)obj;
            free(arr->elements);
            free(arr);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* fn = (ObjFunction*)obj;
            free(fn->chunk.instructions);
            free(fn->chunk.constants);
            free(fn);
            break;
        }
        // ... etc
    }
}
```

---

## Implementation Checklist

- [ ] NaN-boxing value encoding/decoding
- [ ] All 40+ opcodes implemented
- [ ] Stack-based instruction execution
- [ ] Local and global variable storage
- [ ] Function definition and calls with proper frame management
- [ ] Array and map creation/indexing
- [ ] String concatenation and interpolation
- [ ] Generational garbage collection (mark-sweep)
- [ ] Stack overflow detection
- [ ] Type checking at runtime
- [ ] Error propagation with stack traces
- [ ] Debug output (bytecode disassembly)
- [ ] Performance profiling

---

