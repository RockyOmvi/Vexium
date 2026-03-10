#ifndef VEXIUM_AST_H
#define VEXIUM_AST_H

#include "common.h"
#include "token.h"

/* ══════════════════════════════════════════════════════════════
 *  AST NODE TYPES
 * ══════════════════════════════════════════════════════════════ */

typedef enum {
    /* ── Expressions ── */
    NODE_INT_LITERAL,       /* 42, 0xFF, 0b1010 */
    NODE_FLOAT_LITERAL,     /* 3.14 */
    NODE_STRING_LITERAL,    /* "hello" */
    NODE_BOOL_LITERAL,      /* true, false */
    NODE_NOTHING_LITERAL,   /* nothing */
    NODE_IDENTIFIER,        /* variable names */
    NODE_BINARY_OP,         /* a + b, x is greater than y */
    NODE_UNARY_OP,          /* not x, -x */
    NODE_CALL,              /* foo(x, y) */
    NODE_INDEX,             /* arr[0] */
    NODE_FIELD_ACCESS,      /* obj.name */
    NODE_ARRAY_LITERAL,     /* [1, 2, 3] */
    NODE_MAP_LITERAL,       /* {"a": 1, "b": 2} */
    NODE_ASSIGN,            /* x be 5, x = 5 */
    NODE_LAMBDA,            /* (a, b) => a + b */
    NODE_LIST_COMP,         /* [x * 2 for each x in arr if x > 3] */
    NODE_AWAIT,             /* await expr */
    NODE_SPAWN,             /* spawn expr (create concurrent task) */
    NODE_CHANNEL_CREATE,    /* channel() - create a channel */
    NODE_DECORATOR,         /* @decorator applied to fn/struct */
    NODE_YIELD,             /* yield expr (generator) */

    /* ── Statements ── */
    NODE_LET_DECL,          /* let x be 5 */
    NODE_CONST_DECL,        /* const x be 5 */
    NODE_EXPR_STMT,         /* expression as statement */
    NODE_DISPLAY,           /* display "hello" */
    NODE_IF,                /* if/elif/else */
    NODE_WHILE,             /* while loop */
    NODE_FOR_RANGE,         /* for i in 1 to 10 */
    NODE_FOR_EACH,          /* for each item in list */
    NODE_REPEAT,            /* repeat N times */
    NODE_FN_DECL,           /* fn name(params): body */
    NODE_GIVE_BACK,         /* give back value */
    NODE_DEFER,             /* defer statement */
    NODE_BREAK,             /* break */
    NODE_SKIP,              /* skip (continue) */
    NODE_STRUCT_DECL,       /* struct Name: has/can */
    NODE_TRAIT,             /* trait Name: methods */
    NODE_IMPL,              /* impl Trait for Struct: methods */
    NODE_OPERATOR_OVERLOAD, /* operator + on struct */
    NODE_MATCH,             /* match expr: cases */
    NODE_USE,               /* use module */
    NODE_FROM_USE,          /* from module use name */
    NODE_ATTEMPT,           /* attempt/otherwise */
    NODE_PASS,              /* pass */
    NODE_UNSAFE,            /* unsafe block */
    NODE_BLOCK,             /* indented block of statements */
    NODE_PROGRAM            /* top-level program */
} NodeType;

/* ══════════════════════════════════════════════════════════════
 *  AST NODE STRUCTURE (tagged union)
 * ══════════════════════════════════════════════════════════════ */

/* Forward declaration */
typedef struct ASTNode ASTNode;

/* Dynamic array of AST nodes */
typedef struct {
    ASTNode** items;
    int count;
    int capacity;
} NodeList;

/* Function parameter */
typedef struct {
    char* name;
    char* type_name;    /* NULL if no type annotation */
    ASTNode* default_value; /* NULL if no default */
} Param;

typedef struct {
    Param* items;
    int count;
    int capacity;
} ParamList;

/* Match case arm */
typedef struct {
    ASTNode* pattern;
    ASTNode* body;
} MatchArm;

typedef struct {
    MatchArm* items;
    int count;
    int capacity;
} MatchArmList;

/* Struct field */
typedef struct {
    char* name;
    char* type_name;
} StructField;

/* Struct method */
typedef struct {
    char* name;
    ParamList params;
    char* return_type;
    ASTNode* body;
} StructMethod;

/* Map entry (key-value pair) */
typedef struct {
    ASTNode* key;
    ASTNode* value;
} MapEntry;

/* ── The AST Node ── */
struct ASTNode {
    NodeType type;
    int line;
    int column;

    union {
        /* Literals */
        struct { int64_t value; } int_literal;
        struct { double value; } float_literal;
        struct { char* value; int length; } string_literal;
        struct { bool value; } bool_literal;

        /* Identifier */
        struct { char* name; } identifier;

        /* Binary op: left OP right */
        struct {
            ASTNode* left;
            ASTNode* right;
            VexiumTokenType op;
        } binary;

        /* Unary op: OP operand */
        struct {
            ASTNode* operand;
            VexiumTokenType op;
        } unary;

        /* Function call: callee(args) */
        struct {
            ASTNode* callee;
            NodeList args;
        } call;

        /* Index: object[index] */
        struct {
            ASTNode* object;
            ASTNode* index;
        } index_access;

        /* Field access: object.field */
        struct {
            ASTNode* object;
            char* field;
        } field_access;

        /* Spawn: spawn expr (create concurrent task) */
        struct {
            char* name;          /* Optional task name */
            ASTNode* function;  /* The function expression */
            NodeList args;       /* Arguments to pass to the function */
        } spawn;

        /* Channel create: channel() */
        struct {
            int capacity;  /* Buffer size, 0 for unbuffered */
        } channel_create;

        /* Array literal: [items] */
        struct { NodeList elements; } array_literal;

        /* Map literal: {entries} */
        struct {
            MapEntry* entries;
            int count;
            int capacity;
        } map_literal;

        /* Assignment: target be value */
        struct {
            ASTNode* target;
            ASTNode* value;
            VexiumTokenType op;   /* TOKEN_BE or TOKEN_ASSIGN, TOKEN_PLUS_ASSIGN, etc */
        } assign;

        /* let x be 5 / const x be 5 */
        struct {
            char* name;
            char* type_name;   /* optional type annotation */
            ASTNode* value;
        } var_decl;

        /* display expr */
        struct { ASTNode* value; } display;

        /* if / elif / else */
        struct {
            ASTNode* condition;
            ASTNode* then_block;
            ASTNode* else_block;   /* can be another if (elif) or block (else) */
        } if_stmt;

        /* while condition: body */
        struct {
            ASTNode* condition;
            ASTNode* body;
        } while_stmt;

        /* for i in start to end (by step): body */
        struct {
            char* var_name;
            ASTNode* start;
            ASTNode* end;
            ASTNode* step;      /* NULL if no step */
            ASTNode* body;
        } for_range;

        /* for each item in iterable: body */
        struct {
            char* var_name;
            ASTNode* iterable;
            ASTNode* body;
        } for_each;

        /* repeat N times: body */
        struct {
            ASTNode* count;
            ASTNode* body;
        } repeat;

        /* fn name(params) -> return_type: body */
        struct {
            char* name;
            ParamList params;
            char* return_type;   /* NULL if no return type */
            ASTNode* body;
            NodeList decorators; /* @decorator expressions */
        } fn_decl;

        /* give back value */
        struct { ASTNode* value; } give_back;

        /* defer expr — deferred execution before function return */
        struct { ASTNode* expr; } defer;

        /* await expr — wait for async operation */
        struct { ASTNode* expr; } await;

        /* struct Name [extends Parent]: fields + methods */
        struct {
            char* name;
            char** parent_names;    /* Array of parent struct names */
            int parent_count;       /* Number of parents */
            StructField* fields;
            int field_count;
            int field_capacity;
            StructMethod* methods;
            int method_count;
            int method_capacity;
        } struct_decl;

        /* trait Name: method_signatures */
        struct {
            char* name;
            StructMethod* methods;  /* Only signatures, no bodies */
            int method_count;
            int method_capacity;
        } trait_decl;

        /* impl Trait for Struct: implementations */
        struct {
            char* trait_name;
            char* struct_name;
            StructMethod* methods;
            int method_count;
            int method_capacity;
        } impl_block;

        /* operator + on struct (or other operators: -, *, /, ==, etc) */
        struct {
            char* struct_name;
            VexiumTokenType op;          /* TOKEN_PLUS, TOKEN_STAR, etc */
            ParamList params;      /* Usually 2 params: self, other */
            char* return_type;
            ASTNode* body;
        } operator_overload;

        /* Unsafe block */
        struct {
            ASTNode* body;
        } unsafe_block;

        /* (params) => expr — lambda */
        struct {
            ParamList params;
            ASTNode* body;         /* single expression or block */
        } lambda;

        /* [expr for each var in iterable (if cond)] */
        struct {
            ASTNode* expr;         /* transform expression */
            char* var_name;        /* iteration variable */
            ASTNode* iterable;     /* source collection */
            ASTNode* condition;    /* optional filter (NULL if none) */
        } list_comp;

        /* match expr: arms */
        struct {
            ASTNode* expr;
            MatchArmList arms;
        } match_stmt;

        /* use module */
        struct { char* module_name; } use_stmt;

        /* from module use name */
        struct {
            char* module_name;
            char* import_name;
        } from_use;

        /* attempt: body otherwise err: handler */
        struct {
            ASTNode* try_block;
            char* error_name;
            ASTNode* catch_block;
        } attempt;

        /* Block of statements */
        struct { NodeList statements; } block;

        /* yield expr */
        struct { ASTNode* expr; } yield;

        /* Program (top-level) */
        struct { NodeList statements; } program;

        /* Expression statement */
        struct { ASTNode* expr; } expr_stmt;
    } as;
};

/* ══════════════════════════════════════════════════════════════
 *  AST API
 * ══════════════════════════════════════════════════════════════ */

/* Node creation */
ASTNode* ast_new_node(NodeType type, int line, int column);

/* NodeList helpers */
void nodelist_init(NodeList* list);
void nodelist_add(NodeList* list, ASTNode* node);

/* ParamList helpers */
void paramlist_init(ParamList* list);
void paramlist_add(ParamList* list, const char* name, const char* type_name);

/* Printing */
void ast_print(ASTNode* node, int indent);

/* Memory management */
void ast_free(ASTNode* node);

/* Utility — duplicate a string */
char* vex_strdup(const char* src, int length);

#endif /* VEXIUM_AST_H */
