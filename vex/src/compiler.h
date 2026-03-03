#ifndef VEXIUM_COMPILER_H
#define VEXIUM_COMPILER_H

#include "common.h"
#include "ast.h"
#include "opcodes.h"

/* ══════════════════════════════════════════════════════════════
 *  COMPILER — AST → Bytecode
 * ══════════════════════════════════════════════════════════════ */

/* Local variable tracking */
typedef struct {
    char name[256];
    int depth;          /* scope depth (0 = global) */
} Local;

#define MAX_LOCALS 256

/* Compiler state */
typedef struct Compiler {
    ObjFunction* function;      /* function being compiled */
    struct Compiler* enclosing;  /* parent compiler (for nested fns) */

    Local locals[MAX_LOCALS];
    int local_count;
    int scope_depth;

    bool had_error;
} Compiler;

/* ── Public API ── */
ObjFunction* compile(ASTNode* program);

#endif /* VEXIUM_COMPILER_H */
