#ifndef VEXIUM_INTERPRETER_H
#define VEXIUM_INTERPRETER_H

#include "common.h"
#include "ast.h"

typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_BOOL,
    VAL_NOTHING,
    VAL_ARRAY,
    VAL_MAP,
    VAL_FN,
    VAL_STRUCT_DEF,
    VAL_INSTANCE,
    VAL_BUILTIN_FN
} ValueType;

typedef struct VexValue VexValue;
typedef struct Environment Environment;
typedef struct ValueArray ValueArray;
typedef struct ValueMap ValueMap;
typedef struct InstanceValue InstanceValue;

typedef VexValue (*BuiltinFn)(VexValue* args, int arg_count);

typedef struct {
    ASTNode* decl;
    Environment* closure;
} FnValue;

typedef struct {
    char* name;
    ASTNode* decl;
} StructDef;

struct VexValue {
    ValueType type;
    union {
        int64_t int_val;
        double float_val;
        struct { char* data; int length; } string_val;
        bool bool_val;
        ValueArray* array_val;
        ValueMap* map_val;
        FnValue fn_val;
        StructDef struct_def;
        InstanceValue* instance_val;
        struct { BuiltinFn func; const char* name; } builtin_fn;
    } as;
};

struct ValueArray {
    VexValue* items;
    int count;
    int capacity;
};

typedef struct {
    char* key;
    VexValue value;
} ValueMapEntry;

struct ValueMap {
    ValueMapEntry* entries;
    int count;
    int capacity;
};

struct InstanceValue {
    char* struct_name;
    ValueMap fields;
    ASTNode* decl;
    Environment* closure;
};

typedef struct {
    char* name;
    VexValue value;
    bool is_const;
} EnvEntry;

struct Environment {
    EnvEntry* entries;
    int count;
    int capacity;
    Environment* parent;
    int ref_count;
};

typedef enum {
    SIGNAL_NONE,
    SIGNAL_BREAK,
    SIGNAL_SKIP,
    SIGNAL_RETURN
} SignalType;

typedef struct {
    SignalType type;
    VexValue return_value;
} Signal;

typedef struct {
    Environment* global_env;
    Signal signal;
    bool had_error;
} Interpreter;

VexValue vex_int(int64_t val);
VexValue vex_float(double val);
VexValue vex_string(const char* str, int len);
VexValue vex_bool(bool val);
VexValue vex_nothing(void);

void vex_print_value(VexValue val);
char* vex_value_to_string(VexValue val);
bool vex_is_truthy(VexValue val);
const char* vex_type_name(VexValue val);

Environment* env_new(Environment* parent);
void env_define(Environment* env, const char* name, VexValue value, bool is_const);
bool env_set(Environment* env, const char* name, VexValue value);
VexValue* env_get(Environment* env, const char* name);
void env_free(Environment* env);
void env_retain(Environment* env);
void env_release(Environment* env);

void interpreter_init(Interpreter* interp);
void interpreter_free(Interpreter* interp);
VexValue interpret(Interpreter* interp, ASTNode* node, Environment* env);
void interpret_program(Interpreter* interp, ASTNode* program);

#endif
