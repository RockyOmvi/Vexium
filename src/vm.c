#include "vm.h"
#include "jit.h"

void vm_init(VM* vm) {
    vm->chunk = NULL;
    vm->ip = NULL;
    vm->stack_top = vm->stack;
    vm->objects = NULL;
    vm->globals_count = 0;
    memset(vm->globals, 0, sizeof(vm->globals));
}

void vm_free(VM* vm) {
    Obj* object = vm->objects;
    while (object != NULL) {
        Obj* next = object->next;
        free_obj(object);
        object = next;
    }
    for (int i = 0; i < vm->globals_count; i++) {
        if (vm->globals[i].name) free(vm->globals[i].name);
    }
    vm_init(vm);
}

void vm_push(VM* vm, Value64 value) {
    *vm->stack_top = value;
    vm->stack_top++;
}

Value64 vm_pop(VM* vm) {
    vm->stack_top--;
    return *vm->stack_top;
}

static Value64 vm_peek(VM* vm, int distance) {
    return vm->stack_top[-1 - distance];
}

static void vm_set_global(VM* vm, const char* name, Value64 val) {
    for (int i = 0; i < vm->globals_count; i++) {
        if (strcmp(vm->globals[i].name, name) == 0) {
            vm->globals[i].value = val;
            return;
        }
    }
    if (vm->globals_count < GLOBALS_MAX) {
        vm->globals[vm->globals_count].name = vex_strdup(name, (int)strlen(name));
        vm->globals[vm->globals_count].value = val;
        vm->globals_count++;
    }
}

static Value64 vm_get_global(VM* vm, const char* name) {
    for (int i = 0; i < vm->globals_count; i++) {
        if (strcmp(vm->globals[i].name, name) == 0) {
            return vm->globals[i].value;
        }
    }
    return TAG_NIL;
}

InterpretResult vm_run(VM* vm, Chunk* chunk) {
    vm->chunk = chunk;
    vm->ip = vm->chunk->code;

    /* JIT HOT-PATH EXECUTION WIRING */
    if (chunk && chunk->count > 0) {
        JITCompiler jit;
        jit_init(&jit);
        JITFunction jit_fn = jit_compile_chunk(&jit, chunk);
        if (jit_fn) {
            printf("⚡ [Vexium JIT Runtime] Lowered bytecode chunk (%d bytecodes) to x86_64 native machine code.\n", chunk->count);
            jit_fn((uint64_t*)vm->stack, (uint64_t*)vm->globals);
        }
        jit_free(&jit);
    }

#define READ_BYTE() (*vm->ip++)
#define READ_SHORT() (vm->ip += 2, (uint16_t)((vm->ip[-2] << 8) | vm->ip[-1]))
#define READ_CONSTANT() (vm->chunk->constants[READ_BYTE()])
#define BINARY_OP(op_type, val_type_fn) \
    do { \
        Value64 b = vm_pop(vm); \
        Value64 a = vm_pop(vm); \
        double double_a = is_double(a) ? value_to_double(a) : (double)value_to_int(a); \
        double double_b = is_double(b) ? value_to_double(b) : (double)value_to_int(b); \
        vm_push(vm, val_type_fn(double_a op_type double_b)); \
    } while (false)

    for (;;) {
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value64 constant = READ_CONSTANT();
                vm_push(vm, constant);
                break;
            }
            case OP_NIL:   vm_push(vm, TAG_NIL); break;
            case OP_TRUE:  vm_push(vm, TAG_TRUE); break;
            case OP_FALSE: vm_push(vm, TAG_FALSE); break;
            case OP_POP:   vm_pop(vm); break;

            case OP_GET_GLOBAL: {
                Value64 name_val = READ_CONSTANT();
                if (is_obj(name_val)) {
                    ObjString* name_obj = (ObjString*)value_to_obj(name_val);
                    Value64 val = vm_get_global(vm, name_obj->chars);
                    vm_push(vm, val);
                } else {
                    vm_push(vm, TAG_NIL);
                }
                break;
            }
            case OP_SET_GLOBAL: {
                Value64 name_val = READ_CONSTANT();
                if (is_obj(name_val)) {
                    ObjString* name_obj = (ObjString*)value_to_obj(name_val);
                    Value64 val = vm_peek(vm, 0);
                    vm_set_global(vm, name_obj->chars, val);
                }
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                vm->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (is_nothing(vm_peek(vm, 0)) || (is_bool(vm_peek(vm, 0)) && !value_to_bool(vm_peek(vm, 0)))) {
                    vm->ip += offset;
                }
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                vm->ip -= offset;
                break;
            }
            case OP_EQUAL: {
                Value64 b = vm_pop(vm);
                Value64 a = vm_pop(vm);
                vm_push(vm, bool_to_value(a == b));
                break;
            }
            case OP_GREATER:  BINARY_OP(>, bool_to_value); break;
            case OP_LESS:     BINARY_OP(<, bool_to_value); break;
            case OP_ADD:      BINARY_OP(+, double_to_value); break;
            case OP_SUBTRACT: BINARY_OP(-, double_to_value); break;
            case OP_MULTIPLY: BINARY_OP(*, double_to_value); break;
            case OP_DIVIDE:   BINARY_OP(/, double_to_value); break;

            case OP_NOT:
                vm_push(vm, bool_to_value(is_nothing(vm_peek(vm, 0)) || (is_bool(vm_peek(vm, 0)) && !value_to_bool(vm_pop(vm)))));
                break;
            case OP_NEGATE:
                if (is_int(vm_peek(vm, 0))) {
                    vm_push(vm, int_to_value(-value_to_int(vm_pop(vm))));
                } else if (is_double(vm_peek(vm, 0))) {
                    vm_push(vm, double_to_value(-value_to_double(vm_pop(vm))));
                }
                break;
            case OP_PRINT: {
                print_value64(vm_pop(vm));
                printf("\n");
                break;
            }
            case OP_CLASS: {
                Value64 name = READ_CONSTANT();
                vm_push(vm, name);
                break;
            }
            case OP_METHOD: {
                Value64 method_name = READ_CONSTANT();
                (void)method_name;
                break;
            }
            case OP_INVOKE: {
                Value64 method_name = READ_CONSTANT();
                (void)method_name;
                break;
            }
            case OP_CLOSURE: {
                Value64 fn = READ_CONSTANT();
                vm_push(vm, fn);
                break;
            }
            case OP_IMPORT: {
                Value64 mod_name = READ_CONSTANT();
                (void)mod_name;
                break;
            }
            case OP_RETURN: {
                return INTERPRET_OK;
            }
            default:
                return INTERPRET_RUNTIME_ERROR;
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef BINARY_OP
}
