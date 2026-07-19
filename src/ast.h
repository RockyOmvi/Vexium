#ifndef VEXIUM_AST_H
#define VEXIUM_AST_H

#include "common.h"
#include "token.h"

typedef enum {

    NODE_INT_LITERAL,
    NODE_FLOAT_LITERAL,
    NODE_STRING_LITERAL,
    NODE_BOOL_LITERAL,
    NODE_NOTHING_LITERAL,
    NODE_IDENTIFIER,
    NODE_BINARY_OP,
    NODE_UNARY_OP,
    NODE_CALL,
    NODE_INDEX,
    NODE_FIELD_ACCESS,
    NODE_ARRAY_LITERAL,
    NODE_MAP_LITERAL,
    NODE_ASSIGN,
    NODE_LAMBDA,
    NODE_LIST_COMP,

    NODE_LET_DECL,
    NODE_CONST_DECL,
    NODE_EXPR_STMT,
    NODE_DISPLAY,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR_RANGE,
    NODE_FOR_EACH,
    NODE_REPEAT,
    NODE_FN_DECL,
    NODE_GIVE_BACK,
    NODE_BREAK,
    NODE_SKIP,
    NODE_STRUCT_DECL,
    NODE_MATCH,
    NODE_USE,
    NODE_FROM_USE,
    NODE_ATTEMPT,
    NODE_PASS,
    NODE_BLOCK,
    NODE_PROGRAM
} NodeType;

typedef struct ASTNode ASTNode;

typedef struct {
    ASTNode** items;
    int count;
    int capacity;
} NodeList;

typedef struct {
    char* name;
    char* type_name;
    ASTNode* default_value;
} Param;

typedef struct {
    Param* items;
    int count;
    int capacity;
} ParamList;

typedef struct {
    ASTNode* pattern;
    ASTNode* body;
} MatchArm;

typedef struct {
    MatchArm* items;
    int count;
    int capacity;
} MatchArmList;

typedef struct {
    char* name;
    char* type_name;
} StructField;

typedef struct {
    char* name;
    ParamList params;
    char* return_type;
    ASTNode* body;
} StructMethod;

typedef struct {
    ASTNode* key;
    ASTNode* value;
} MapEntry;

struct ASTNode {
    NodeType type;
    int line;
    int column;

    union {

        struct { int64_t value; } int_literal;
        struct { double value; } float_literal;
        struct { char* value; int length; } string_literal;
        struct { bool value; } bool_literal;

        struct { char* name; } identifier;

        struct {
            ASTNode* left;
            ASTNode* right;
            TokenType op;
        } binary;

        struct {
            ASTNode* operand;
            TokenType op;
        } unary;

        struct {
            ASTNode* callee;
            NodeList args;
        } call;

        struct {
            ASTNode* object;
            ASTNode* index;
        } index_access;

        struct {
            ASTNode* object;
            char* field;
        } field_access;

        struct { NodeList elements; } array_literal;

        struct {
            MapEntry* entries;
            int count;
            int capacity;
        } map_literal;

        struct {
            ASTNode* target;
            ASTNode* value;
            TokenType op;
        } assign;

        struct {
            char* name;
            char* type_name;
            ASTNode* value;
        } var_decl;

        struct { ASTNode* value; } display;

        struct {
            ASTNode* condition;
            ASTNode* then_block;
            ASTNode* else_block;
        } if_stmt;

        struct {
            ASTNode* condition;
            ASTNode* body;
        } while_stmt;

        struct {
            char* var_name;
            ASTNode* start;
            ASTNode* end;
            ASTNode* step;
            ASTNode* body;
        } for_range;

        struct {
            char* var_name;
            ASTNode* iterable;
            ASTNode* body;
        } for_each;

        struct {
            ASTNode* count;
            ASTNode* body;
        } repeat;

        struct {
            char* name;
            ParamList params;
            char* return_type;
            ASTNode* body;
        } fn_decl;

        struct { ASTNode* value; } give_back;

        struct {
            char* name;
            char* parent_name;
            StructField* fields;
            int field_count;
            int field_capacity;
            StructMethod* methods;
            int method_count;
            int method_capacity;
        } struct_decl;

        struct {
            ParamList params;
            ASTNode* body;
        } lambda;

        struct {
            ASTNode* expr;
            char* var_name;
            ASTNode* iterable;
            ASTNode* condition;
        } list_comp;

        struct {
            ASTNode* expr;
            MatchArmList arms;
        } match_stmt;

        struct { char* module_name; } use_stmt;

        struct {
            char* module_name;
            char* import_name;
        } from_use;

        struct {
            ASTNode* try_block;
            char* error_name;
            ASTNode* catch_block;
        } attempt;

        struct { NodeList statements; } block;

        struct { NodeList statements; } program;

        struct { ASTNode* expr; } expr_stmt;
    } as;
};

ASTNode* ast_new_node(NodeType type, int line, int column);

void nodelist_init(NodeList* list);
void nodelist_add(NodeList* list, ASTNode* node);

void paramlist_init(ParamList* list);
void paramlist_add(ParamList* list, const char* name, const char* type_name);

void ast_print(ASTNode* node, int indent);

void ast_free(ASTNode* node);

char* vex_strdup(const char* src, int length);

#endif
