#ifndef VEXIUM_TYPECHECKER_H
#define VEXIUM_TYPECHECKER_H

#include "ast.h"

typedef enum {
    TYPE_KIND_PRIM,
    TYPE_KIND_VAR,
    TYPE_KIND_GENERIC,
    TYPE_KIND_RESULT,
    TYPE_KIND_OPTION
} HMTypeKind;

typedef struct HMType HMType;

struct HMType {
    HMTypeKind kind;
    char* name;
    HMType* param1;
    HMType* param2;
    HMType* instance;
};

typedef struct {
    char* var_name;
    HMType* type;
} TypeEnvEntry;

typedef struct {
    TypeEnvEntry* entries;
    int count;
    int capacity;
} TypeEnv;

typedef struct {
    int error_count;
    bool strict_mode;
    TypeEnv env;
    int var_counter;
} TypeChecker;

void typechecker_init(TypeChecker* tc, bool strict);
bool typechecker_check(TypeChecker* tc, ASTNode* node);

HMType* hm_type_prim(const char* name);
HMType* hm_type_var(TypeChecker* tc);
HMType* hm_type_generic(const char* name, HMType* param);
HMType* hm_type_result(HMType* ok_type, HMType* err_type);
HMType* hm_type_option(HMType* val_type);

bool hm_unify(HMType* t1, HMType* t2);
void typechecker_free(TypeChecker* tc);

#endif
