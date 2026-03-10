/* ═══════════════════════════════════════════════════════════════════════════════
 *  VEXIUM TYPE SYSTEM
 *  Gradual typing with inference and generics
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef VEXIUM_TYPE_SYSTEM_H
#define VEXIUM_TYPE_SYSTEM_H

#include <stdbool.h>
#include <stdlib.h>
#include "ast.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TYPE KINDS
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    TYPE_DYNAMIC,      /* Any type (untyped values) */
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_STRING,
    TYPE_ARRAY,        /* Generic: Array<T> */
    TYPE_MAP,          /* Generic: Map<K, V> */
    TYPE_TUPLE,        /* Fixed tuple: (int, str, bool) */
    TYPE_STRUCT,       /* Custom struct */
    TYPE_FUNCTION,     /* Function type: (int, str) -> bool */
    TYPE_NOTHING,      /* void / nil */
    TYPE_OPTION,       /* Optional<T> */
    TYPE_UNION,        /* Type A | Type B */
    TYPE_VARIABLE,     /* Type variable for inference (T, U, etc) */
    TYPE_GENERIC       /* Generic type parameter */
} TypeKind;

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TYPE STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct Type {
    TypeKind kind;
    
    /* For error reporting */
    int line;
    int column;
    
    /* For generics (Array<T>, Map<K,V>, Option<T>) */
    struct Type* element_type;      /* For Array<T>, Option<T> */
    struct Type* key_type;          /* For Map<K,V> */
    struct Type* value_type;        /* For Map<K,V>, Array<T> */
    
    /* For functions: (param_types) -> return_type */
    struct Type** param_types;
    int param_count;
    struct Type* return_type;
    
    /* For structs */
    char* struct_name;
    
    /* For tuples: list of element types */
    struct Type** tuple_types;
    int tuple_count;
    
    /* For unions: Type A | Type B | ... */
    struct Type** union_types;
    int union_count;
    
    /* For type variables (inference) */
    char* var_name;
    struct Type* binding;           /* Bound type or NULL */
    int id;                         /* Unique ID for type var */
    
} Type;

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TYPE CONTEXT (for inference)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define TYPE_CONTEXT_MAX_VARS 256
#define TYPE_CONTEXT_MAX_BINDINGS 512

typedef struct {
    char* names[TYPE_CONTEXT_MAX_BINDINGS];
    Type* types[TYPE_CONTEXT_MAX_BINDINGS];
    int count;
} TypeBinding;

typedef struct TypeContext {
    TypeBinding bindings;           /* Variable name → type */
    TypeBinding generic_bindings;   /* Generic param → concrete type */
    bool strict_mode;               /* Strict type checking? */
    int next_type_var_id;           /* For generating fresh type variables */
    
    /* Error tracking */
    bool had_error;
    char error_msg[512];
} TypeContext;

/* ═══════════════════════════════════════════════════════════════════════════════
 *  BASIC TYPE CONSTRUCTORS
 * ═══════════════════════════════════════════════════════════════════════════════ */

Type* type_create_nothing(void);
Type* type_create_dynamic(void);
Type* type_create_int(void);
Type* type_create_float(void);
Type* type_create_bool(void);
Type* type_create_string(void);

/* Generic types */
Type* type_create_array(Type* element);
Type* type_create_map(Type* key, Type* value);
Type* type_create_option(Type* inner);
Type* type_create_function(Type** params, int count, Type* ret);

/* Complex types */
Type* type_create_struct(const char* name);
Type* type_create_tuple(Type** types, int count);
Type* type_create_union(Type** types, int count);
Type* type_create_variable(const char* name, int id);

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TYPE CONTEXT
 * ═══════════════════════════════════════════════════════════════════════════════ */

void type_context_init(TypeContext* ctx, bool strict);
void type_context_free(TypeContext* ctx);

void type_context_bind(TypeContext* ctx, const char* name, Type* type);
Type* type_context_lookup(TypeContext* ctx, const char* name);

Type* type_fresh_variable(TypeContext* ctx, const char* prefix);

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TYPE INFERENCE
 * ═══════════════════════════════════════════════════════════════════════════════ */

Type* infer_from_node(ASTNode* node, TypeContext* ctx);
Type* infer_literal(ASTNode* node);
Type* infer_binary(ASTNode* node, TypeContext* ctx);
Type* infer_unary(ASTNode* node, TypeContext* ctx);
Type* infer_identifier(ASTNode* node, TypeContext* ctx);
Type* infer_call(ASTNode* node, TypeContext* ctx);
Type* infer_array(ASTNode* node, TypeContext* ctx);
Type* infer_map(ASTNode* node, TypeContext* ctx);
Type* infer_function(ASTNode* node, TypeContext* ctx);

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TYPE CHECKING
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool type_equals(Type* a, Type* b);
bool type_unify(Type* a, Type* b);  /* Unification for inference */
bool type_is_subtype(Type* sub, Type* super);
bool type_is_assignable(Type* target, Type* source);

/* Check if type is concrete (no type variables) */
bool type_is_concrete(Type* type);

/* Occurs check for recursive types */
bool type_occurs_in(Type* var, Type* type);

/* Type parser for annotations */
Type* type_parse_string(const char* type_str, TypeContext* ctx);

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TYPE UTILITIES
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* String representation */
char* type_to_string(Type* type);

/* Deep copy */
Type* type_copy(Type* type);

/* Free type */
void type_free(Type* type);

/* Apply substitutions (resolve type variables) */
Type* type_apply(Type* type, TypeContext* ctx);

/* ═══════════════════════════════════════════════════════════════════════════════
 *  BUILT-IN TYPES (singletons)
 * ═══════════════════════════════════════════════════════════════════════════════ */

extern Type* TYPE_NOTHING_SINGLETON;
extern Type* TYPE_DYNAMIC_SINGLETON;
extern Type* TYPE_INT_SINGLETON;
extern Type* TYPE_FLOAT_SINGLETON;
extern Type* TYPE_BOOL_SINGLETON;
extern Type* TYPE_STRING_SINGLETON;

/* Initialize built-in types */
void type_system_init(void);
void type_system_shutdown(void);

/* Public type checking API - called from main.c */
int vex_type_check(ASTNode* program);
int vex_type_check_function(ASTNode* fn_node, bool strict);

#endif /* VEXIUM_TYPE_SYSTEM_H */
