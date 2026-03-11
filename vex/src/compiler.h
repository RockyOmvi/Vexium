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
    bool is_captured;
} Local;

#define MAX_LOCALS 256
#define MAX_UPVALUES 256

typedef struct {
    uint8_t index;
    bool is_local;
} Upvalue;

/* Max break statements per loop */
#define MAX_BREAK_JUMPS 32

/* Compiler state */
typedef struct Compiler {
    ObjFunction* function;      /* function being compiled */
    struct Compiler* enclosing;  /* parent compiler (for nested fns) */

    Local locals[MAX_LOCALS];
    int local_count;
    int scope_depth;
    Upvalue upvalues[MAX_UPVALUES];
    int upvalue_count;

    /* Loop context for break/skip (continue) */
    int loop_start;             /* bytecode offset of loop condition (-1 if not in loop) */
    int loop_scope_depth;       /* scope depth when loop began */
    int loop_exit_jumps[MAX_BREAK_JUMPS]; /* pending break jump offsets to patch */
    int loop_exit_count;        /* number of pending break jumps */
    int loop_continue_jumps[MAX_BREAK_JUMPS]; /* pending skip (continue) jump offsets to patch */
    int loop_continue_count;    /* number of pending skip jumps */

    bool is_generator;
    int yield_slot;

    bool had_error;
} Compiler;

/* ── Public API ── */
ObjFunction* compile(ASTNode* program);

#endif /* VEXIUM_COMPILER_H */
