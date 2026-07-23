#include "typechecker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

HMType* hm_type_prim(const char* name) {
    HMType* t = (HMType*)malloc(sizeof(HMType));
    t->kind = TYPE_KIND_PRIM;
    t->name = vex_strdup(name, (int)strlen(name));
    t->param1 = NULL;
    t->param2 = NULL;
    t->instance = NULL;
    return t;
}

HMType* hm_type_var(TypeChecker* tc) {
    HMType* t = (HMType*)malloc(sizeof(HMType));
    t->kind = TYPE_KIND_VAR;
    char buf[32];
    snprintf(buf, sizeof(buf), "'t%d", tc->var_counter++);
    t->name = vex_strdup(buf, (int)strlen(buf));
    t->param1 = NULL;
    t->param2 = NULL;
    t->instance = NULL;
    return t;
}

HMType* hm_type_generic(const char* name, HMType* param) {
    HMType* t = (HMType*)malloc(sizeof(HMType));
    t->kind = TYPE_KIND_GENERIC;
    t->name = vex_strdup(name, (int)strlen(name));
    t->param1 = param;
    t->param2 = NULL;
    t->instance = NULL;
    return t;
}

HMType* hm_type_result(HMType* ok_type, HMType* err_type) {
    HMType* t = (HMType*)malloc(sizeof(HMType));
    t->kind = TYPE_KIND_RESULT;
    t->name = vex_strdup("Result", 6);
    t->param1 = ok_type;
    t->param2 = err_type;
    t->instance = NULL;
    return t;
}

HMType* hm_type_option(HMType* val_type) {
    HMType* t = (HMType*)malloc(sizeof(HMType));
    t->kind = TYPE_KIND_OPTION;
    t->name = vex_strdup("Option", 6);
    t->param1 = val_type;
    t->param2 = NULL;
    t->instance = NULL;
    return t;
}

static HMType* prune(HMType* t) {
    if (t->kind == TYPE_KIND_VAR && t->instance != NULL) {
        t->instance = prune(t->instance);
        return t->instance;
    }
    return t;
}

bool hm_unify(HMType* t1, HMType* t2) {
    HMType* a = prune(t1);
    HMType* b = prune(t2);

    if (a->kind == TYPE_KIND_VAR) {
        if (a != b) {
            a->instance = b;
        }
        return true;
    }
    if (b->kind == TYPE_KIND_VAR) {
        return hm_unify(b, a);
    }
    if (a->kind == TYPE_KIND_PRIM && b->kind == TYPE_KIND_PRIM) {
        return strcmp(a->name, b->name) == 0;
    }
    if (a->kind == TYPE_KIND_RESULT && b->kind == TYPE_KIND_RESULT) {
        return hm_unify(a->param1, b->param1) && hm_unify(a->param2, b->param2);
    }
    if (a->kind == TYPE_KIND_OPTION && b->kind == TYPE_KIND_OPTION) {
        return hm_unify(a->param1, b->param1);
    }
    if (a->kind == TYPE_KIND_GENERIC && b->kind == TYPE_KIND_GENERIC) {
        return strcmp(a->name, b->name) == 0 && hm_unify(a->param1, b->param1);
    }
    return false;
}

void typechecker_init(TypeChecker* tc, bool strict) {
    tc->error_count = 0;
    tc->strict_mode = strict;
    tc->var_counter = 0;
    tc->env.entries = NULL;
    tc->env.count = 0;
    tc->env.capacity = 0;
}

void typechecker_free(TypeChecker* tc) {
    for (int i = 0; i < tc->env.count; i++) {
        free(tc->env.entries[i].var_name);
    }
    free(tc->env.entries);
}

static void env_bind(TypeChecker* tc, const char* var, HMType* type) {
    if (tc->env.count >= tc->env.capacity) {
        tc->env.capacity = tc->env.capacity < 8 ? 8 : tc->env.capacity * 2;
        tc->env.entries = (TypeEnvEntry*)realloc(tc->env.entries, sizeof(TypeEnvEntry) * tc->env.capacity);
    }
    tc->env.entries[tc->env.count].var_name = vex_strdup(var, (int)strlen(var));
    tc->env.entries[tc->env.count].type = type;
    tc->env.count++;
}

static HMType* infer(TypeChecker* tc, ASTNode* node) {
    if (!node) return hm_type_prim("nothing");
    switch (node->type) {
        case NODE_INT_LITERAL: return hm_type_prim("int");
        case NODE_FLOAT_LITERAL: return hm_type_prim("float");
        case NODE_STRING_LITERAL: return hm_type_prim("str");
        case NODE_BOOL_LITERAL: return hm_type_prim("bool");
        case NODE_NOTHING_LITERAL: return hm_type_prim("nothing");
        case NODE_BINARY_OP: {
            HMType* left_t = infer(tc, node->as.binary.left);
            HMType* right_t = infer(tc, node->as.binary.right);
            if (node->as.binary.op == TOKEN_PLUS || node->as.binary.op == TOKEN_MINUS ||
                node->as.binary.op == TOKEN_STAR || node->as.binary.op == TOKEN_SLASH) {
                if (!hm_unify(left_t, right_t) && tc->strict_mode) {
                    fprintf(stderr, "[Type Error] Line %d: Mismatched types in binary operation.\n", node->line);
                    tc->error_count++;
                }
                return left_t;
            }
            return hm_type_prim("bool");
        }
        default:
            return hm_type_var(tc);
    }
}

bool typechecker_check(TypeChecker* tc, ASTNode* node) {
    if (!node) return true;
    if (node->type == NODE_PROGRAM || node->type == NODE_BLOCK) {
        for (int i = 0; i < node->as.program.statements.count; i++) {
            ASTNode* stmt = node->as.program.statements.items[i];
            if (stmt->type == NODE_LET_DECL || stmt->type == NODE_CONST_DECL) {
                HMType* t = infer(tc, stmt->as.var_decl.value);
                env_bind(tc, stmt->as.var_decl.name, t);
            }
        }
    }
    return tc->error_count == 0;
}
