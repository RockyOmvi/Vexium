#include "compiler.h"
#include "opcodes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ══════════════════════════════════════════════════════════════
 *  INTERNAL STATE
 * ══════════════════════════════════════════════════════════════ */

static Compiler* current = NULL;

/* ── Helper: write instruction to current chunk ── */
static void emit_byte(uint8_t byte, int line) {
    ObjFunction* fn = current->function;
    if (fn->chunk_count >= fn->chunk_capacity) {
        int old = fn->chunk_capacity;
        fn->chunk_capacity = old < 8 ? 8 : old * 2;
        fn->code = realloc(fn->code, fn->chunk_capacity);
        fn->lines = realloc(fn->lines, fn->chunk_capacity * sizeof(int));
    }
    fn->code[fn->chunk_count] = byte;
    fn->lines[fn->chunk_count] = line;
    fn->chunk_count++;
}

static void emit_bytes(uint8_t a, uint8_t b, int line) {
    emit_byte(a, line);
    emit_byte(b, line);
}

static void emit_short(uint16_t val, int line) {
    emit_byte((uint8_t)(val & 0xFF), line);
    emit_byte((uint8_t)((val >> 8) & 0xFF), line);
}

static int make_constant(Value val) {
    ObjFunction* fn = current->function;
    if (fn->const_count >= fn->const_capacity) {
        int old = fn->const_capacity;
        fn->const_capacity = old < 8 ? 8 : old * 2;
        fn->constants = realloc(fn->constants, fn->const_capacity * sizeof(Value));
    }
    fn->constants[fn->const_count] = val;
    return fn->const_count++;
}

static void emit_constant(Value val, int line) {
    int idx = make_constant(val);
    emit_byte(OP_CONST, line);
    emit_short((uint16_t)idx, line);
}

static int add_string_constant(const char* str, int length) {
    ObjString* s = obj_string_new(str, length);
    return make_constant(val_obj(s));
}

/* ── Jumps ── */

static int emit_jump(uint8_t instruction, int line) {
    emit_byte(instruction, line);
    emit_byte(0xFF, line);
    emit_byte(0xFF, line);
    return current->function->chunk_count - 2;
}

static void patch_jump(int offset) {
    ObjFunction* fn = current->function;
    int jump = fn->chunk_count - offset - 2;
    if (jump > 0xFFFF) {
        fprintf(stderr, "[Compiler Error] Jump too large.\n");
        current->had_error = true;
        return;
    }
    fn->code[offset]     = (uint8_t)(jump & 0xFF);
    fn->code[offset + 1] = (uint8_t)((jump >> 8) & 0xFF);
}

static void emit_loop(int loop_start, int line) {
    emit_byte(OP_LOOP, line);
    int offset = current->function->chunk_count - loop_start + 2;
    if (offset > 0xFFFF) {
        fprintf(stderr, "[Compiler Error] Loop body too large.\n");
        current->had_error = true;
        return;
    }
    emit_short((uint16_t)offset, line);
}

/* ── Scope ── */

static void begin_scope(void) { current->scope_depth++; }

static void end_scope(int line) {
    current->scope_depth--;
    while (current->local_count > 0 &&
           current->locals[current->local_count - 1].depth > current->scope_depth) {
        emit_byte(OP_POP, line);
        current->local_count--;
    }
}

static void add_local(const char* name) {
    if (current->local_count >= MAX_LOCALS) {
        fprintf(stderr, "[Compiler Error] Too many local variables.\n");
        current->had_error = true;
        return;
    }
    Local* local = &current->locals[current->local_count++];
    strncpy(local->name, name, 255);
    local->name[255] = '\0';
    local->depth = current->scope_depth;
}

static int resolve_local(Compiler* compiler, const char* name) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        if (strcmp(compiler->locals[i].name, name) == 0) return i;
    }
    return -1;
}

/* ══════════════════════════════════════════════════════════════
 *  Forward declarations
 * ══════════════════════════════════════════════════════════════ */

static void compile_node(ASTNode* node);
static void compile_expression(ASTNode* node);

/* ══════════════════════════════════════════════════════════════
 *  COMPILE EXPRESSIONS
 * ══════════════════════════════════════════════════════════════ */

static void compile_int_literal(ASTNode* node) {
    int64_t val = node->as.int_literal.value;
    if (val >= INT32_MIN && val <= INT32_MAX) {
        emit_constant(val_int((int32_t)val), node->line);
    } else {
        emit_constant(val_number((double)val), node->line);
    }
}

static void compile_float_literal(ASTNode* node) {
    emit_constant(val_number(node->as.float_literal.value), node->line);
}

static void compile_string_literal(ASTNode* node) {
    const char* src = node->as.string_literal.value;
    int src_len = node->as.string_literal.length;
    int line = node->line;

    /* Check if string contains interpolation {…} */
    bool has_interp = false;
    for (int i = 0; i < src_len; i++) {
        if (src[i] == '{') { has_interp = true; break; }
    }

    if (!has_interp) {
        /* Simple string — no interpolation */
        int idx = add_string_constant(src, src_len);
        emit_byte(OP_CONST, line);
        emit_short((uint16_t)idx, line);
        return;
    }

    /* String interpolation: split into segments, concat them */
    int parts = 0;
    int i = 0;
    while (i < src_len) {
        if (src[i] == '{') {
            /* Find matching close brace */
            int start = i + 1;
            int depth = 1;
            i++;
            while (i < src_len && depth > 0) {
                if (src[i] == '{') depth++;
                else if (src[i] == '}') depth--;
                if (depth > 0) i++;
            }
            /* Extract variable name */
            int name_len = i - start;
            if (name_len > 0) {
                /* Emit the variable lookup + OP_TO_STRING */
                int var_idx = add_string_constant(src + start, name_len);
                /* Try resolving as local first */
                char name_buf[256];
                if (name_len > 255) name_len = 255;
                memcpy(name_buf, src + start, name_len);
                name_buf[name_len] = '\0';

                int slot = resolve_local(current, name_buf);
                if (slot != -1) {
                    emit_byte(OP_GET_LOCAL, line);
                    emit_byte((uint8_t)slot, line);
                } else {
                    emit_byte(OP_GET_GLOBAL, line);
                    emit_short((uint16_t)var_idx, line);
                }
                emit_byte(OP_TO_STRING, line);
                parts++;
            }
            i++; /* skip closing '}' */
        } else {
            /* Literal text segment — collect until next '{' or end */
            int seg_start = i;
            while (i < src_len && src[i] != '{') i++;
            int seg_len = i - seg_start;
            if (seg_len > 0) {
                int idx = add_string_constant(src + seg_start, seg_len);
                emit_byte(OP_CONST, line);
                emit_short((uint16_t)idx, line);
                parts++;
            }
        }
    }

    /* Concatenate all parts: ADD them together left-to-right */
    for (int p = 1; p < parts; p++) {
        emit_byte(OP_ADD, line);
    }
}

static void compile_bool_literal(ASTNode* node) {
    emit_byte(node->as.bool_literal.value ? OP_TRUE : OP_FALSE, node->line);
}

static void compile_identifier(ASTNode* node) {
    const char* name = node->as.identifier.name;
    int slot = resolve_local(current, name);
    if (slot != -1) {
        emit_byte(OP_GET_LOCAL, node->line);
        emit_byte((uint8_t)slot, node->line);
    } else {
        int idx = add_string_constant(name, (int)strlen(name));
        emit_byte(OP_GET_GLOBAL, node->line);
        emit_short((uint16_t)idx, node->line);
    }
}

static void compile_binary(ASTNode* node) {
    TokenType op = node->as.binary.op;

    /* Short-circuit: and */
    if (op == TOKEN_AND) {
        compile_expression(node->as.binary.left);
        int jump = emit_jump(OP_JMP_IF_FALSE, node->line);
        emit_byte(OP_POP, node->line);
        compile_expression(node->as.binary.right);
        patch_jump(jump);
        return;
    }

    /* Short-circuit: or */
    if (op == TOKEN_OR) {
        compile_expression(node->as.binary.left);
        int else_jump = emit_jump(OP_JMP_IF_FALSE, node->line);
        int end_jump = emit_jump(OP_JMP, node->line);
        patch_jump(else_jump);
        emit_byte(OP_POP, node->line);
        compile_expression(node->as.binary.right);
        patch_jump(end_jump);
        return;
    }

    compile_expression(node->as.binary.left);
    compile_expression(node->as.binary.right);

    switch (op) {
        case TOKEN_PLUS:         emit_byte(OP_ADD, node->line); break;
        case TOKEN_MINUS:        emit_byte(OP_SUB, node->line); break;
        case TOKEN_STAR:         emit_byte(OP_MUL, node->line); break;
        case TOKEN_SLASH:        emit_byte(OP_DIV, node->line); break;
        case TOKEN_PERCENT:      emit_byte(OP_MOD, node->line); break;
        case TOKEN_POWER:        emit_byte(OP_POW, node->line); break;
        case TOKEN_EQ:
        case TOKEN_IS:           emit_byte(OP_EQ, node->line); break;
        case TOKEN_NEQ:
        case TOKEN_IS_NOT:       emit_byte(OP_NEQ, node->line); break;
        case TOKEN_LT:
        case TOKEN_IS_LESS:      emit_byte(OP_LT, node->line); break;
        case TOKEN_GT:
        case TOKEN_IS_GREATER:   emit_byte(OP_GT, node->line); break;
        case TOKEN_LTE:
        case TOKEN_IS_AT_MOST:   emit_byte(OP_LTE, node->line); break;
        case TOKEN_GTE:
        case TOKEN_IS_AT_LEAST:  emit_byte(OP_GTE, node->line); break;
        default:
            fprintf(stderr, "[Compiler Error] Unknown binary op at line %d\n", node->line);
            current->had_error = true;
            break;
    }
}

static void compile_unary(ASTNode* node) {
    compile_expression(node->as.unary.operand);
    switch (node->as.unary.op) {
        case TOKEN_MINUS: emit_byte(OP_NEG, node->line); break;
        case TOKEN_NOT:   emit_byte(OP_NOT, node->line); break;
        default:
            fprintf(stderr, "[Compiler Error] Unknown unary op at line %d\n", node->line);
            current->had_error = true;
            break;
    }
}

static void compile_call(ASTNode* node) {
    compile_expression(node->as.call.callee);
    int argc = node->as.call.args.count;
    for (int i = 0; i < argc; i++) {
        compile_expression(node->as.call.args.items[i]);
    }
    emit_byte(OP_CALL, node->line);
    emit_byte((uint8_t)argc, node->line);
}

static void compile_array_literal(ASTNode* node) {
    int count = node->as.array_literal.elements.count;
    for (int i = 0; i < count; i++) {
        compile_expression(node->as.array_literal.elements.items[i]);
    }
    emit_byte(OP_ARRAY, node->line);
    emit_short((uint16_t)count, node->line);
}

static void compile_map_literal(ASTNode* node) {
    int count = node->as.map_literal.count;
    for (int i = 0; i < count; i++) {
        compile_expression(node->as.map_literal.entries[i].key);
        compile_expression(node->as.map_literal.entries[i].value);
    }
    emit_byte(OP_MAP, node->line);
    emit_short((uint16_t)count, node->line);
}

static void compile_index(ASTNode* node) {
    compile_expression(node->as.index_access.object);
    compile_expression(node->as.index_access.index);
    emit_byte(OP_INDEX_GET, node->line);
}

static void compile_assign(ASTNode* node) {
    ASTNode* target = node->as.assign.target;
    TokenType op = node->as.assign.op;

    /* Handle compound assignment: +=, -=, *=, /= */
    if (op == TOKEN_PLUS_ASSIGN || op == TOKEN_MINUS_ASSIGN ||
        op == TOKEN_STAR_ASSIGN || op == TOKEN_SLASH_ASSIGN) {
        /* Get current value */
        compile_expression(target);
        /* Compile RHS */
        compile_expression(node->as.assign.value);
        /* Apply operation */
        switch (op) {
            case TOKEN_PLUS_ASSIGN:  emit_byte(OP_ADD, node->line); break;
            case TOKEN_MINUS_ASSIGN: emit_byte(OP_SUB, node->line); break;
            case TOKEN_STAR_ASSIGN:  emit_byte(OP_MUL, node->line); break;
            case TOKEN_SLASH_ASSIGN: emit_byte(OP_DIV, node->line); break;
            default: break;
        }
    } else {
        compile_expression(node->as.assign.value);
    }

    if (target->type == NODE_IDENTIFIER) {
        const char* name = target->as.identifier.name;
        int slot = resolve_local(current, name);
        if (slot != -1) {
            emit_byte(OP_SET_LOCAL, node->line);
            emit_byte((uint8_t)slot, node->line);
        } else {
            int idx = add_string_constant(name, (int)strlen(name));
            emit_byte(OP_SET_GLOBAL, node->line);
            emit_short((uint16_t)idx, node->line);
        }
    } else if (target->type == NODE_INDEX) {
        compile_expression(target->as.index_access.object);
        compile_expression(target->as.index_access.index);
        emit_byte(OP_INDEX_SET, node->line);
    }
}

static void compile_expression(ASTNode* node) {
    if (!node) return;
    switch (node->type) {
        case NODE_INT_LITERAL:    compile_int_literal(node); break;
        case NODE_FLOAT_LITERAL:  compile_float_literal(node); break;
        case NODE_STRING_LITERAL: compile_string_literal(node); break;
        case NODE_BOOL_LITERAL:   compile_bool_literal(node); break;
        case NODE_NOTHING_LITERAL:emit_byte(OP_NOTHING, node->line); break;
        case NODE_IDENTIFIER:     compile_identifier(node); break;
        case NODE_BINARY_OP:      compile_binary(node); break;
        case NODE_UNARY_OP:       compile_unary(node); break;
        case NODE_CALL:           compile_call(node); break;
        case NODE_ARRAY_LITERAL:  compile_array_literal(node); break;
        case NODE_MAP_LITERAL:    compile_map_literal(node); break;
        case NODE_INDEX:          compile_index(node); break;
        case NODE_ASSIGN:         compile_assign(node); break;
        default:
            fprintf(stderr, "[Compiler] Expression node type %d not compiled (line %d)\n",
                    node->type, node->line);
            emit_byte(OP_NOTHING, node->line);
            break;
    }
}

/* ══════════════════════════════════════════════════════════════
 *  COMPILE STATEMENTS
 * ══════════════════════════════════════════════════════════════ */

static void compile_var_decl(ASTNode* node) {
    const char* name = node->as.var_decl.name;
    if (node->as.var_decl.value) {
        compile_expression(node->as.var_decl.value);
    } else {
        emit_byte(OP_NOTHING, node->line);
    }

    if (current->scope_depth > 0) {
        add_local(name);
    } else {
        int idx = add_string_constant(name, (int)strlen(name));
        emit_byte(OP_DEFINE_GLOBAL, node->line);
        emit_short((uint16_t)idx, node->line);
    }
}

static void compile_display(ASTNode* node) {
    compile_expression(node->as.display.value);
    emit_byte(OP_PRINT, node->line);
}

static void compile_if(ASTNode* node) {
    /* The AST represents elif as chained if nodes in else_block */
    compile_expression(node->as.if_stmt.condition);
    int then_jump = emit_jump(OP_JMP_IF_FALSE, node->line);
    emit_byte(OP_POP, node->line);

    compile_node(node->as.if_stmt.then_block);

    int else_jump = emit_jump(OP_JMP, node->line);
    patch_jump(then_jump);
    emit_byte(OP_POP, node->line);

    if (node->as.if_stmt.else_block) {
        compile_node(node->as.if_stmt.else_block);
    }

    patch_jump(else_jump);
}

static void compile_while(ASTNode* node) {
    int loop_start = current->function->chunk_count;

    compile_expression(node->as.while_stmt.condition);
    int exit_jump = emit_jump(OP_JMP_IF_FALSE, node->line);
    emit_byte(OP_POP, node->line);

    begin_scope();
    compile_node(node->as.while_stmt.body);
    end_scope(node->line);

    emit_loop(loop_start, node->line);
    patch_jump(exit_jump);
    emit_byte(OP_POP, node->line);
}

static void compile_for_range(ASTNode* node) {
    const char* var_name = node->as.for_range.var_name;

    begin_scope();

    /* Init loop var */
    compile_expression(node->as.for_range.start);
    add_local(var_name);
    int var_slot = current->local_count - 1;

    /* End value */
    compile_expression(node->as.for_range.end);
    add_local("__end__");
    int end_slot = current->local_count - 1;

    /* Step value */
    if (node->as.for_range.step) {
        compile_expression(node->as.for_range.step);
    } else {
        emit_constant(val_int(1), node->line);
    }
    add_local("__step__");
    int step_slot = current->local_count - 1;

    int loop_start = current->function->chunk_count;

    /* Condition: var < end */
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)var_slot, node->line);
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)end_slot, node->line);
    emit_byte(OP_LT, node->line);

    int exit_jump = emit_jump(OP_JMP_IF_FALSE, node->line);
    emit_byte(OP_POP, node->line);

    /* Body */
    compile_node(node->as.for_range.body);

    /* Increment: var = var + step */
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)var_slot, node->line);
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)step_slot, node->line);
    emit_byte(OP_ADD, node->line);
    emit_byte(OP_SET_LOCAL, node->line);
    emit_byte((uint8_t)var_slot, node->line);
    emit_byte(OP_POP, node->line);

    emit_loop(loop_start, node->line);
    patch_jump(exit_jump);
    emit_byte(OP_POP, node->line);

    end_scope(node->line);
}

static void compile_for_each(ASTNode* node) {
    const char* var_name = node->as.for_each.var_name;

    begin_scope();

    /* Collection on stack */
    compile_expression(node->as.for_each.iterable);
    add_local("__iter__");
    int iter_slot = current->local_count - 1;

    /* Index = 0 */
    emit_constant(val_int(0), node->line);
    add_local("__idx__");
    int idx_slot = current->local_count - 1;

    /* Loop variable placeholder */
    emit_byte(OP_NOTHING, node->line);
    add_local(var_name);
    int var_slot = current->local_count - 1;

    int loop_start = current->function->chunk_count;

    /* Condition: idx < len(iter) */
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)idx_slot, node->line);
    /* Call len(iter) */
    int len_idx = add_string_constant("len", 3);
    emit_byte(OP_GET_GLOBAL, node->line);
    emit_short((uint16_t)len_idx, node->line);
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)iter_slot, node->line);
    emit_byte(OP_CALL, node->line);
    emit_byte(1, node->line);
    emit_byte(OP_LT, node->line);

    int exit_jump = emit_jump(OP_JMP_IF_FALSE, node->line);
    emit_byte(OP_POP, node->line);

    /* Set var = iter[idx] */
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)iter_slot, node->line);
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)idx_slot, node->line);
    emit_byte(OP_INDEX_GET, node->line);
    emit_byte(OP_SET_LOCAL, node->line);
    emit_byte((uint8_t)var_slot, node->line);
    emit_byte(OP_POP, node->line);

    /* Body */
    compile_node(node->as.for_each.body);

    /* Increment idx */
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)idx_slot, node->line);
    emit_constant(val_int(1), node->line);
    emit_byte(OP_ADD, node->line);
    emit_byte(OP_SET_LOCAL, node->line);
    emit_byte((uint8_t)idx_slot, node->line);
    emit_byte(OP_POP, node->line);

    emit_loop(loop_start, node->line);
    patch_jump(exit_jump);
    emit_byte(OP_POP, node->line);

    end_scope(node->line);
}

static void compile_repeat(ASTNode* node) {
    begin_scope();

    emit_constant(val_int(0), node->line);
    add_local("__rep_i__");
    int counter_slot = current->local_count - 1;

    compile_expression(node->as.repeat.count);
    add_local("__rep_n__");
    int limit_slot = current->local_count - 1;

    int loop_start = current->function->chunk_count;

    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)counter_slot, node->line);
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)limit_slot, node->line);
    emit_byte(OP_LT, node->line);

    int exit_jump = emit_jump(OP_JMP_IF_FALSE, node->line);
    emit_byte(OP_POP, node->line);

    compile_node(node->as.repeat.body);

    /* counter++ */
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)counter_slot, node->line);
    emit_constant(val_int(1), node->line);
    emit_byte(OP_ADD, node->line);
    emit_byte(OP_SET_LOCAL, node->line);
    emit_byte((uint8_t)counter_slot, node->line);
    emit_byte(OP_POP, node->line);

    emit_loop(loop_start, node->line);
    patch_jump(exit_jump);
    emit_byte(OP_POP, node->line);

    end_scope(node->line);
}

static void compile_fn_decl(ASTNode* node) {
    const char* name = node->as.fn_decl.name;

    ObjFunction* fn = obj_function_new(name);
    fn->arity = node->as.fn_decl.params.count;

    Compiler fn_compiler;
    fn_compiler.function = fn;
    fn_compiler.enclosing = current;
    fn_compiler.local_count = 0;
    fn_compiler.scope_depth = 0;
    fn_compiler.had_error = false;
    current = &fn_compiler;

    begin_scope();

    /* Reserve slot 0 for the callee function object itself.
       The VM sets frame->slots to point at the callee on the stack,
       so local slot 0 is the callee, and params start at slot 1. */
    add_local("");

    for (int i = 0; i < node->as.fn_decl.params.count; i++) {
        add_local(node->as.fn_decl.params.items[i].name);
    }

    compile_node(node->as.fn_decl.body);

    /* Implicit return nothing */
    emit_byte(OP_NOTHING, node->line);
    emit_byte(OP_RETURN, node->line);

    bool had_err = fn_compiler.had_error;
    current = fn_compiler.enclosing;
    if (had_err) current->had_error = true;

    int fn_idx = make_constant(val_obj(fn));
    emit_byte(OP_CONST, node->line);
    emit_short((uint16_t)fn_idx, node->line);

    if (current->scope_depth > 0) {
        add_local(name);
    } else {
        int name_idx = add_string_constant(name, (int)strlen(name));
        emit_byte(OP_DEFINE_GLOBAL, node->line);
        emit_short((uint16_t)name_idx, node->line);
    }
}

static void compile_give_back(ASTNode* node) {
    if (node->as.give_back.value) {
        compile_expression(node->as.give_back.value);
    } else {
        emit_byte(OP_NOTHING, node->line);
    }
    emit_byte(OP_RETURN, node->line);
}

/* ══════════════════════════════════════════════════════════════
 *  DISPATCH
 * ══════════════════════════════════════════════════════════════ */

static void compile_node(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        /* Expressions used as statements (rare but possible) */
        case NODE_INT_LITERAL:
        case NODE_FLOAT_LITERAL:
        case NODE_STRING_LITERAL:
        case NODE_BOOL_LITERAL:
        case NODE_NOTHING_LITERAL:
        case NODE_IDENTIFIER:
        case NODE_BINARY_OP:
        case NODE_UNARY_OP:
        case NODE_CALL:
        case NODE_ARRAY_LITERAL:
        case NODE_MAP_LITERAL:
        case NODE_INDEX:
            compile_expression(node);
            break;

        case NODE_ASSIGN:
            compile_assign(node);
            emit_byte(OP_POP, node->line);
            break;

        case NODE_LET_DECL:
        case NODE_CONST_DECL:
            compile_var_decl(node);
            break;

        case NODE_DISPLAY:      compile_display(node);    break;
        case NODE_IF:           compile_if(node);         break;
        case NODE_WHILE:        compile_while(node);      break;
        case NODE_FOR_RANGE:    compile_for_range(node);  break;
        case NODE_FOR_EACH:     compile_for_each(node);   break;
        case NODE_REPEAT:       compile_repeat(node);     break;
        case NODE_FN_DECL:      compile_fn_decl(node);    break;
        case NODE_GIVE_BACK:    compile_give_back(node);  break;

        case NODE_EXPR_STMT:
            compile_expression(node->as.expr_stmt.expr);
            emit_byte(OP_POP, node->line);
            break;

        case NODE_BLOCK:
            for (int i = 0; i < node->as.block.statements.count; i++) {
                compile_node(node->as.block.statements.items[i]);
            }
            break;

        case NODE_PROGRAM:
            for (int i = 0; i < node->as.program.statements.count; i++) {
                compile_node(node->as.program.statements.items[i]);
            }
            break;

        case NODE_USE:
        case NODE_FROM_USE:
            /* For now, skip module system in VM — handled via builtins */
            break;

        case NODE_BREAK:
        case NODE_SKIP:
        case NODE_PASS:
            break;

        default:
            fprintf(stderr, "[Compiler] Node type %d not compiled (line %d)\n",
                    node->type, node->line);
            break;
    }
}

/* ══════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ══════════════════════════════════════════════════════════════ */

ObjFunction* compile(ASTNode* program) {
    Compiler compiler;
    ObjFunction* fn = obj_function_new("<script>");
    fn->arity = 0;

    compiler.function = fn;
    compiler.enclosing = NULL;
    compiler.local_count = 0;
    compiler.scope_depth = 0;
    compiler.had_error = false;
    current = &compiler;

    compile_node(program);
    emit_byte(OP_HALT, 0);

    current = NULL;

    if (compiler.had_error) {
        fprintf(stderr, "[Compiler] Compilation finished with errors.\n");
        return NULL;
    }

    return fn;
}
