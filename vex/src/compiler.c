#include "compiler.h"
#include "opcodes.h"
#include "type_system.h"
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

static bool node_contains_yield(ASTNode* node) {
    if (!node) return false;

    switch (node->type) {
        case NODE_YIELD:
            return true;
        case NODE_BINARY_OP:
            return node_contains_yield(node->as.binary.left) || node_contains_yield(node->as.binary.right);
        case NODE_UNARY_OP:
            return node_contains_yield(node->as.unary.operand);
        case NODE_CALL:
            if (node_contains_yield(node->as.call.callee)) return true;
            for (int i = 0; i < node->as.call.args.count; i++) {
                if (node_contains_yield(node->as.call.args.items[i])) return true;
            }
            return false;
        case NODE_INDEX:
            return node_contains_yield(node->as.index_access.object) || node_contains_yield(node->as.index_access.index);
        case NODE_FIELD_ACCESS:
            return node_contains_yield(node->as.field_access.object);
        case NODE_ARRAY_LITERAL:
            for (int i = 0; i < node->as.array_literal.elements.count; i++) {
                if (node_contains_yield(node->as.array_literal.elements.items[i])) return true;
            }
            return false;
        case NODE_MAP_LITERAL:
            for (int i = 0; i < node->as.map_literal.count; i++) {
                if (node_contains_yield(node->as.map_literal.entries[i].key) ||
                    node_contains_yield(node->as.map_literal.entries[i].value)) return true;
            }
            return false;
        case NODE_ASSIGN:
            return node_contains_yield(node->as.assign.target) || node_contains_yield(node->as.assign.value);
        case NODE_LET_DECL:
        case NODE_CONST_DECL:
            return node_contains_yield(node->as.var_decl.value);
        case NODE_EXPR_STMT:
            return node_contains_yield(node->as.expr_stmt.expr);
        case NODE_DISPLAY:
            return node_contains_yield(node->as.display.value);
        case NODE_IF:
            return node_contains_yield(node->as.if_stmt.condition) ||
                   node_contains_yield(node->as.if_stmt.then_block) ||
                   node_contains_yield(node->as.if_stmt.else_block);
        case NODE_WHILE:
            return node_contains_yield(node->as.while_stmt.condition) ||
                   node_contains_yield(node->as.while_stmt.body);
        case NODE_FOR_RANGE:
            return node_contains_yield(node->as.for_range.start) ||
                   node_contains_yield(node->as.for_range.end) ||
                   node_contains_yield(node->as.for_range.step) ||
                   node_contains_yield(node->as.for_range.body);
        case NODE_FOR_EACH:
            return node_contains_yield(node->as.for_each.iterable) || node_contains_yield(node->as.for_each.body);
        case NODE_REPEAT:
            return node_contains_yield(node->as.repeat.count) || node_contains_yield(node->as.repeat.body);
        case NODE_GIVE_BACK:
            return node_contains_yield(node->as.give_back.value);
        case NODE_DEFER:
            return node_contains_yield(node->as.defer.expr);
        case NODE_AWAIT:
            return node_contains_yield(node->as.await.expr);
        case NODE_SPAWN:
            if (node_contains_yield(node->as.spawn.function)) return true;
            for (int i = 0; i < node->as.spawn.args.count; i++) {
                if (node_contains_yield(node->as.spawn.args.items[i])) return true;
            }
            return false;
        case NODE_LIST_COMP:
            return node_contains_yield(node->as.list_comp.expr) ||
                   node_contains_yield(node->as.list_comp.iterable) ||
                   node_contains_yield(node->as.list_comp.condition);
        case NODE_MATCH:
            if (node_contains_yield(node->as.match_stmt.expr)) return true;
            for (int i = 0; i < node->as.match_stmt.arms.count; i++) {
                if (node_contains_yield(node->as.match_stmt.arms.items[i].pattern) ||
                    node_contains_yield(node->as.match_stmt.arms.items[i].body)) return true;
            }
            return false;
        case NODE_ATTEMPT:
            return node_contains_yield(node->as.attempt.try_block) || node_contains_yield(node->as.attempt.catch_block);
        case NODE_BLOCK:
            for (int i = 0; i < node->as.block.statements.count; i++) {
                if (node_contains_yield(node->as.block.statements.items[i])) return true;
            }
            return false;
        case NODE_PROGRAM:
            for (int i = 0; i < node->as.program.statements.count; i++) {
                if (node_contains_yield(node->as.program.statements.items[i])) return true;
            }
            return false;
        case NODE_FN_DECL:
        case NODE_LAMBDA:
            return false;
        default:
            return false;
    }
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
        if (current->locals[current->local_count - 1].is_captured) {
            emit_byte(OP_CLOSE_UPVALUE, line);
        } else {
            emit_byte(OP_POP, line);
        }
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
    local->is_captured = false;
}

static int resolve_local(Compiler* compiler, const char* name) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        if (strcmp(compiler->locals[i].name, name) == 0) return i;
    }
    return -1;
}

static int add_upvalue(Compiler* compiler, uint8_t index, bool is_local) {
    for (int i = 0; i < compiler->upvalue_count; i++) {
        if (compiler->upvalues[i].index == index && compiler->upvalues[i].is_local == is_local) {
            return i;
        }
    }

    if (compiler->upvalue_count >= MAX_UPVALUES) {
        fprintf(stderr, "[Compiler Error] Too many closure variables.\n");
        compiler->had_error = true;
        return -1;
    }

    int slot = compiler->upvalue_count++;
    compiler->upvalues[slot].index = index;
    compiler->upvalues[slot].is_local = is_local;
    return slot;
}

static int resolve_upvalue(Compiler* compiler, const char* name) {
    if (compiler->enclosing == NULL) return -1;

    int local = resolve_local(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].is_captured = true;
        return add_upvalue(compiler, (uint8_t)local, true);
    }

    int up = resolve_upvalue(compiler->enclosing, name);
    if (up != -1) {
        return add_upvalue(compiler, (uint8_t)up, false);
    }

    return -1;
}

/* ══════════════════════════════════════════════════════════════
 *  Forward declarations
 * ══════════════════════════════════════════════════════════════ */

static void compile_node(ASTNode* node);
static void compile_expression(ASTNode* node);
static void compile_list_comp(ASTNode* node);
static void compile_match(ASTNode* node);
static void compile_lambda(ASTNode* node);
static void compile_defer(ASTNode* node);
static void compile_yield(ASTNode* node);

/* Helper: check if a node type is an expression */
static bool is_expression_node(NodeType type) {
    switch (type) {
        case NODE_INT_LITERAL:
        case NODE_FLOAT_LITERAL:
        case NODE_STRING_LITERAL:
        case NODE_BOOL_LITERAL:
        case NODE_NOTHING_LITERAL:
        case NODE_IDENTIFIER:
        case NODE_BINARY_OP:
        case NODE_UNARY_OP:
        case NODE_CALL:
        case NODE_INDEX:
        case NODE_FIELD_ACCESS:
        case NODE_ARRAY_LITERAL:
        case NODE_MAP_LITERAL:
        case NODE_ASSIGN:
        case NODE_LAMBDA:
        case NODE_LIST_COMP:
        case NODE_AWAIT:
        case NODE_SPAWN:
        case NODE_CHANNEL_CREATE:
        case NODE_YIELD:
            return true;
        default:
            return false;
    }
}
static void compile_await(ASTNode* node);
static void compile_spawn(ASTNode* node);
static void compile_channel_create(ASTNode* node);

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

    /* Concatenate all parts: CONCAT them together left-to-right */
    for (int p = 1; p < parts; p++) {
        emit_byte(OP_CONCAT, line);
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
        int up = resolve_upvalue(current, name);
        if (up != -1) {
            emit_byte(OP_GET_UPVALUE, node->line);
            emit_byte((uint8_t)up, node->line);
        } else {
            int idx = add_string_constant(name, (int)strlen(name));
            emit_byte(OP_GET_GLOBAL, node->line);
            emit_short((uint16_t)idx, node->line);
        }
    }
}

static void compile_binary(ASTNode* node) {
    VexiumTokenType op = node->as.binary.op;

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

static void compile_tail_call(ASTNode* node) {
    compile_expression(node->as.call.callee);
    int argc = node->as.call.args.count;
    for (int i = 0; i < argc; i++) {
        compile_expression(node->as.call.args.items[i]);
    }
    emit_byte(OP_TAIL_CALL, node->line);
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
    if (node->as.index_access.is_slice) {
        int slice_idx = add_string_constant("slice", 5);
        emit_byte(OP_GET_GLOBAL, node->line);
        emit_short((uint16_t)slice_idx, node->line);
        compile_expression(node->as.index_access.object);
        if (node->as.index_access.index) compile_expression(node->as.index_access.index); else emit_byte(OP_NOTHING, node->line);
        if (node->as.index_access.end) compile_expression(node->as.index_access.end); else emit_byte(OP_NOTHING, node->line);
        if (node->as.index_access.step) compile_expression(node->as.index_access.step); else emit_byte(OP_NOTHING, node->line);
        emit_byte(OP_CALL, node->line);
        emit_byte(4, node->line);
        return;
    }
    compile_expression(node->as.index_access.object);
    compile_expression(node->as.index_access.index);
    emit_byte(OP_INDEX_GET, node->line);
}

static void compile_field_access(ASTNode* node) {
    /* obj.field — compile object, then emit OP_GET_FIELD with field name */
    compile_expression(node->as.field_access.object);
    int name_idx = add_string_constant(node->as.field_access.field,
                                        (int)strlen(node->as.field_access.field));
    emit_byte(OP_GET_FIELD, node->line);
    emit_short((uint16_t)name_idx, node->line);
}

static void compile_assign(ASTNode* node) {
    ASTNode* target = node->as.assign.target;
    VexiumTokenType op = node->as.assign.op;

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
            int up = resolve_upvalue(current, name);
            if (up != -1) {
                emit_byte(OP_SET_UPVALUE, node->line);
                emit_byte((uint8_t)up, node->line);
            } else {
                int idx = add_string_constant(name, (int)strlen(name));
                emit_byte(OP_SET_GLOBAL, node->line);
                emit_short((uint16_t)idx, node->line);
            }
        }
    } else if (target->type == NODE_INDEX) {
        compile_expression(target->as.index_access.object);
        compile_expression(target->as.index_access.index);
        emit_byte(OP_INDEX_SET, node->line);
    } else if (target->type == NODE_FIELD_ACCESS) {
        compile_expression(target->as.field_access.object);
        int name_idx = add_string_constant(target->as.field_access.field,
                                            (int)strlen(target->as.field_access.field));
        emit_byte(OP_SET_FIELD, node->line);
        emit_short((uint16_t)name_idx, node->line);
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
        case NODE_FIELD_ACCESS:  compile_field_access(node); break;
        case NODE_ASSIGN:         compile_assign(node); break;
        case NODE_LAMBDA:         compile_lambda(node); break;
        case NODE_LIST_COMP:      compile_list_comp(node); break;
        case NODE_AWAIT:          compile_await(node); break;
        case NODE_SPAWN:          compile_spawn(node); break;
        case NODE_CHANNEL_CREATE: compile_channel_create(node); break;
        case NODE_YIELD:          compile_yield(node); break;
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
    /* Save outer loop context */
    int outer_loop_start = current->loop_start;
    int outer_loop_scope = current->loop_scope_depth;
    int outer_exit_count = current->loop_exit_count;
    int outer_exits[MAX_BREAK_JUMPS];
    memcpy(outer_exits, current->loop_exit_jumps, outer_exit_count * sizeof(int));
    int outer_continue_count = current->loop_continue_count;
    int outer_continues[MAX_BREAK_JUMPS];
    memcpy(outer_continues, current->loop_continue_jumps, outer_continue_count * sizeof(int));

    int loop_start = current->function->chunk_count;
    current->loop_start = loop_start;
    current->loop_scope_depth = current->scope_depth;
    current->loop_exit_count = 0;
    current->loop_continue_count = 0;

    compile_expression(node->as.while_stmt.condition);
    int exit_jump = emit_jump(OP_JMP_IF_FALSE, node->line);
    emit_byte(OP_POP, node->line);

    begin_scope();
    compile_node(node->as.while_stmt.body);
    end_scope(node->line);

    /* Patch all skip (continue) jumps here — for while, continue = loop start */
    for (int i = 0; i < current->loop_continue_count; i++) {
        patch_jump(current->loop_continue_jumps[i]);
    }

    emit_loop(loop_start, node->line);
    patch_jump(exit_jump);
    emit_byte(OP_POP, node->line);

    /* Patch all break jumps to here */
    for (int i = 0; i < current->loop_exit_count; i++) {
        patch_jump(current->loop_exit_jumps[i]);
    }

    /* Restore outer loop context */
    current->loop_start = outer_loop_start;
    current->loop_scope_depth = outer_loop_scope;
    current->loop_exit_count = outer_exit_count;
    memcpy(current->loop_exit_jumps, outer_exits, outer_exit_count * sizeof(int));
    current->loop_continue_count = outer_continue_count;
    memcpy(current->loop_continue_jumps, outer_continues, outer_continue_count * sizeof(int));
}

static void compile_for_range(ASTNode* node) {
    const char* var_name = node->as.for_range.var_name;

    /* Save outer loop context */
    int outer_loop_start = current->loop_start;
    int outer_loop_scope = current->loop_scope_depth;
    int outer_exit_count = current->loop_exit_count;
    int outer_exits[MAX_BREAK_JUMPS];
    memcpy(outer_exits, current->loop_exit_jumps, outer_exit_count * sizeof(int));
    int outer_continue_count = current->loop_continue_count;
    int outer_continues[MAX_BREAK_JUMPS];
    memcpy(outer_continues, current->loop_continue_jumps, outer_continue_count * sizeof(int));

    begin_scope();
    current->loop_scope_depth = current->scope_depth;
    current->loop_exit_count = 0;
    current->loop_continue_count = 0;

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
    current->loop_start = loop_start;

    /* Condition: handles both counting up and down.
     * We check: (step > 0 && var < end) || (step < 0 && var > end)
     * Simplified: (var - end) * step < 0  ... but for zero step we skip.
     * Pragmatic approach: check step sign at runtime with jumps.
     *   step >= 0 ? (var < end) : (var > end) */
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)step_slot, node->line);
    emit_constant(val_int(0), node->line);
    emit_byte(OP_GTE, node->line);  /* step >= 0 ? */
    int step_positive_jump = emit_jump(OP_JMP_IF_FALSE, node->line);
    emit_byte(OP_POP, node->line);

    /* step >= 0: use var < end */
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)var_slot, node->line);
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)end_slot, node->line);
        emit_byte(OP_LTE, node->line);
    int after_cond = emit_jump(OP_JMP, node->line);

    /* step < 0: use var > end */
    patch_jump(step_positive_jump);
    emit_byte(OP_POP, node->line);
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)var_slot, node->line);
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)end_slot, node->line);
        emit_byte(OP_GTE, node->line);

    patch_jump(after_cond);

    int exit_jump = emit_jump(OP_JMP_IF_FALSE, node->line);
    emit_byte(OP_POP, node->line);

    /* Body */
    compile_node(node->as.for_range.body);

    /* Patch all skip (continue) jumps to the increment point */
    for (int i = 0; i < current->loop_continue_count; i++) {
        patch_jump(current->loop_continue_jumps[i]);
    }

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

    /* Patch all break jumps to here */
    for (int i = 0; i < current->loop_exit_count; i++) {
        patch_jump(current->loop_exit_jumps[i]);
    }

    end_scope(node->line);

    /* Restore outer loop context */
    current->loop_start = outer_loop_start;
    current->loop_scope_depth = outer_loop_scope;
    current->loop_exit_count = outer_exit_count;
    memcpy(current->loop_exit_jumps, outer_exits, outer_exit_count * sizeof(int));
    current->loop_continue_count = outer_continue_count;
    memcpy(current->loop_continue_jumps, outer_continues, outer_continue_count * sizeof(int));
}

static void compile_for_each(ASTNode* node) {
    const char* var_name = node->as.for_each.var_name;

    /* Save outer loop context */
    int outer_loop_start = current->loop_start;
    int outer_loop_scope = current->loop_scope_depth;
    int outer_exit_count = current->loop_exit_count;
    int outer_exits[MAX_BREAK_JUMPS];
    memcpy(outer_exits, current->loop_exit_jumps, outer_exit_count * sizeof(int));
    int outer_continue_count = current->loop_continue_count;
    int outer_continues[MAX_BREAK_JUMPS];
    memcpy(outer_continues, current->loop_continue_jumps, outer_continue_count * sizeof(int));

    begin_scope();
    current->loop_scope_depth = current->scope_depth;
    current->loop_exit_count = 0;
    current->loop_continue_count = 0;

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
    current->loop_start = loop_start;

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

    /* Patch all skip (continue) jumps to the increment point */
    for (int i = 0; i < current->loop_continue_count; i++) {
        patch_jump(current->loop_continue_jumps[i]);
    }

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

    /* Patch all break jumps to here */
    for (int i = 0; i < current->loop_exit_count; i++) {
        patch_jump(current->loop_exit_jumps[i]);
    }

    end_scope(node->line);

    /* Restore outer loop context */
    current->loop_start = outer_loop_start;
    current->loop_scope_depth = outer_loop_scope;
    current->loop_exit_count = outer_exit_count;
    memcpy(current->loop_exit_jumps, outer_exits, outer_exit_count * sizeof(int));
    current->loop_continue_count = outer_continue_count;
    memcpy(current->loop_continue_jumps, outer_continues, outer_continue_count * sizeof(int));
}

static void compile_repeat(ASTNode* node) {
    /* Save outer loop context */
    int outer_loop_start = current->loop_start;
    int outer_loop_scope = current->loop_scope_depth;
    int outer_exit_count = current->loop_exit_count;
    int outer_exits[MAX_BREAK_JUMPS];
    memcpy(outer_exits, current->loop_exit_jumps, outer_exit_count * sizeof(int));
    int outer_continue_count = current->loop_continue_count;
    int outer_continues[MAX_BREAK_JUMPS];
    memcpy(outer_continues, current->loop_continue_jumps, outer_continue_count * sizeof(int));

    begin_scope();
    current->loop_scope_depth = current->scope_depth;
    current->loop_exit_count = 0;
    current->loop_continue_count = 0;

    emit_constant(val_int(0), node->line);
    add_local("__rep_i__");
    int counter_slot = current->local_count - 1;

    compile_expression(node->as.repeat.count);
    add_local("__rep_n__");
    int limit_slot = current->local_count - 1;

    int loop_start = current->function->chunk_count;
    current->loop_start = loop_start;

    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)counter_slot, node->line);
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)limit_slot, node->line);
    emit_byte(OP_LT, node->line);

    int exit_jump = emit_jump(OP_JMP_IF_FALSE, node->line);
    emit_byte(OP_POP, node->line);

    compile_node(node->as.repeat.body);

    /* Patch all skip (continue) jumps to the increment point */
    for (int i = 0; i < current->loop_continue_count; i++) {
        patch_jump(current->loop_continue_jumps[i]);
    }

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

    /* Patch all break jumps to here */
    for (int i = 0; i < current->loop_exit_count; i++) {
        patch_jump(current->loop_exit_jumps[i]);
    }

    end_scope(node->line);

    /* Restore outer loop context */
    current->loop_start = outer_loop_start;
    current->loop_scope_depth = outer_loop_scope;
    current->loop_exit_count = outer_exit_count;
    memcpy(current->loop_exit_jumps, outer_exits, outer_exit_count * sizeof(int));
    current->loop_continue_count = outer_continue_count;
    memcpy(current->loop_continue_jumps, outer_continues, outer_continue_count * sizeof(int));
}

static void compile_list_comp(ASTNode* node) {
    begin_scope();
    
    emit_byte(OP_ARRAY, node->line);
    emit_short(0, node->line);
    add_local("__res__");
    int res_slot = current->local_count - 1;
    
    compile_expression(node->as.list_comp.iterable);
    add_local("__iter__");
    int iter_slot = current->local_count - 1;

    emit_constant(val_int(0), node->line);
    add_local("__idx__");
    int idx_slot = current->local_count - 1;

    emit_byte(OP_NOTHING, node->line);
    add_local(node->as.list_comp.var_name);
    int var_slot = current->local_count - 1;

    int loop_start = current->function->chunk_count;

    /* Condition: idx < len(iter) */
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)idx_slot, node->line);
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

    if (node->as.list_comp.condition) {
        compile_expression(node->as.list_comp.condition);
        int skip_jump = emit_jump(OP_JMP_IF_FALSE, node->line);
        emit_byte(OP_POP, node->line);
        
        emit_byte(OP_GET_LOCAL, node->line);
        emit_byte((uint8_t)res_slot, node->line);
        compile_expression(node->as.list_comp.expr);
        emit_byte(OP_ARRAY_PUSH, node->line);
        emit_byte(OP_POP, node->line); /* pop array dup */
        
        patch_jump(skip_jump);
        emit_byte(OP_POP, node->line); /* pop condition */
    } else {
        emit_byte(OP_GET_LOCAL, node->line);
        emit_byte((uint8_t)res_slot, node->line);
        compile_expression(node->as.list_comp.expr);
        emit_byte(OP_ARRAY_PUSH, node->line);
        emit_byte(OP_POP, node->line); /* pop array dup */
    }

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

    /* Manual scope end to keep __res__ on stack */
    current->scope_depth--;
    emit_byte(OP_POP, node->line); /* pop var */
    emit_byte(OP_POP, node->line); /* pop __idx__ */
    emit_byte(OP_POP, node->line); /* pop __iter__ */
    current->local_count -= 4; /* __res__, __iter__, __idx__, var */
    /* __res__ is left on the stack as the expression result */
}

static void compile_match(ASTNode* node) {
    /* match is a statement */
    compile_expression(node->as.match_stmt.expr);
    
    int end_jumps[128]; // Max 128 arms for simplicity
    int arm_count = node->as.match_stmt.arms.count;
    if (arm_count > 128) arm_count = 128;
    
    for (int i = 0; i < arm_count; i++) {
        MatchArm* arm = &node->as.match_stmt.arms.items[i];
        if (arm->pattern == NULL) {
            /* otherwise: default case */
            emit_byte(OP_POP, node->line); // pop match expr
            compile_node(arm->body);
            end_jumps[i] = emit_jump(OP_JMP, node->line);
        } else {
            /* Duplicate match expr to compare */
            emit_byte(OP_DUP, node->line);
            compile_expression(arm->pattern);
            emit_byte(OP_EQ, node->line);
            int false_jump = emit_jump(OP_JMP_IF_FALSE, node->line);
            emit_byte(OP_POP, node->line); /* pop true comparison result */
            
            emit_byte(OP_POP, node->line); /* pop match expr to execute body cleanly */
            compile_node(arm->body);
            end_jumps[i] = emit_jump(OP_JMP, node->line);
            
            patch_jump(false_jump);
            emit_byte(OP_POP, node->line); /* pop false comparison result */
        }
    }
    
    /* Pop match expr if no arms matched and no otherwise was hit */
    /* Because otherwise pops and jumps, if we reach here we didn't hit otherwise */
    /* Wait, if the last arm is not 'otherwise', we need to pop the match expr. */
    if (arm_count == 0 || node->as.match_stmt.arms.items[arm_count - 1].pattern != NULL) {
        emit_byte(OP_POP, node->line);
    }
    
    for (int i = 0; i < arm_count; i++) {
        patch_jump(end_jumps[i]);
    }
}

static void compile_fn_decl(ASTNode* node) {
    const char* name = node->as.fn_decl.name;
    bool is_generator = node_contains_yield(node->as.fn_decl.body);

    ObjFunction* fn = obj_function_new(name);
    {
        int pcount = node->as.fn_decl.params.count;
        bool has_var = (pcount > 0 &&
                        node->as.fn_decl.params.items[pcount - 1].is_variadic);
        fn->arity = has_var ? -pcount : pcount;
    }

    Compiler fn_compiler;
    fn_compiler.function = fn;
    fn_compiler.enclosing = current;
    fn_compiler.local_count = 0;
    fn_compiler.scope_depth = 0;
    fn_compiler.upvalue_count = 0;
    fn_compiler.had_error = false;
    fn_compiler.loop_start = -1;
    fn_compiler.loop_scope_depth = 0;
    fn_compiler.loop_exit_count = 0;
    fn_compiler.loop_continue_count = 0;
    fn_compiler.is_generator = is_generator;
    fn_compiler.yield_slot = -1;
    current = &fn_compiler;

    begin_scope();

    /* Reserve slot 0 for the callee function object itself.
       The VM sets frame->slots to point at the callee on the stack,
       so local slot 0 is the callee, and params start at slot 1. */
    add_local("");

    for (int i = 0; i < node->as.fn_decl.params.count; i++) {
        add_local(node->as.fn_decl.params.items[i].name);
    }

    if (is_generator) {
        emit_byte(OP_ARRAY, node->line);
        emit_short(0, node->line);
        add_local("__yield__");
        current->yield_slot = current->local_count - 1;
    }

    compile_node(node->as.fn_decl.body);

    /* Implicit return nothing, or collected yields for generator functions. */
    if (is_generator) {
        emit_byte(OP_GET_LOCAL, node->line);
        emit_byte((uint8_t)current->yield_slot, node->line);
    } else {
        emit_byte(OP_NOTHING, node->line);
    }
    emit_byte(OP_RETURN, node->line);

    bool had_err = fn_compiler.had_error;
    current = fn_compiler.enclosing;
    if (had_err) current->had_error = true;

    fn->upvalue_count = fn_compiler.upvalue_count;

    int fn_idx = make_constant(val_obj(fn));
    emit_byte(OP_CLOSURE, node->line);
    emit_short((uint16_t)fn_idx, node->line);
    for (int i = 0; i < fn_compiler.upvalue_count; i++) {
        emit_byte(fn_compiler.upvalues[i].is_local ? 1 : 0, node->line);
        emit_byte(fn_compiler.upvalues[i].index, node->line);
    }

    if (current->scope_depth > 0) {
        add_local(name);
    } else {
        int name_idx = add_string_constant(name, (int)strlen(name));
        emit_byte(OP_DEFINE_GLOBAL, node->line);
        emit_short((uint16_t)name_idx, node->line);
    }
}

static void compile_lambda(ASTNode* node) {
    ObjFunction* fn = obj_function_new("<lambda>");
    fn->arity = node->as.lambda.params.count;

    Compiler fn_compiler;
    fn_compiler.function = fn;
    fn_compiler.enclosing = current;
    fn_compiler.local_count = 0;
    fn_compiler.scope_depth = 0;
    fn_compiler.upvalue_count = 0;
    fn_compiler.had_error = false;
    fn_compiler.loop_start = -1;
    fn_compiler.loop_scope_depth = 0;
    fn_compiler.loop_exit_count = 0;
    fn_compiler.loop_continue_count = 0;
    fn_compiler.is_generator = false;
    fn_compiler.yield_slot = -1;
    current = &fn_compiler;

    begin_scope();

    /* Reserve slot 0 for callee */
    add_local("");

    for (int i = 0; i < node->as.lambda.params.count; i++) {
        add_local(node->as.lambda.params.items[i].name);
    }

    ASTNode* body = node->as.lambda.body;
    
    if (body->type == NODE_EXPR_STMT) {
        compile_expression(body->as.expr_stmt.expr);
        emit_byte(OP_RETURN, node->line);
    } else if (is_expression_node(body->type)) {
        compile_expression(body);
        emit_byte(OP_RETURN, node->line);
    } else {
        compile_node(body);
        emit_byte(OP_NOTHING, node->line);
        emit_byte(OP_RETURN, node->line);
    }

    bool had_err = fn_compiler.had_error;
    current = fn_compiler.enclosing;
    if (had_err) current->had_error = true;

    fn->upvalue_count = fn_compiler.upvalue_count;

    int fn_idx = make_constant(val_obj(fn));
    emit_byte(OP_CLOSURE, node->line);
    emit_short((uint16_t)fn_idx, node->line);
    for (int i = 0; i < fn_compiler.upvalue_count; i++) {
        emit_byte(fn_compiler.upvalues[i].is_local ? 1 : 0, node->line);
        emit_byte(fn_compiler.upvalues[i].index, node->line);
    }
}

static void compile_give_back(ASTNode* node) {
    if (current->is_generator) {
        if (node->as.give_back.value) {
            compile_expression(node->as.give_back.value);
            emit_byte(OP_POP, node->line);
        }
        emit_byte(OP_GET_LOCAL, node->line);
        emit_byte((uint8_t)current->yield_slot, node->line);
        emit_byte(OP_RETURN, node->line);
        return;
    }
    if (node->as.give_back.value) {
        compile_expression(node->as.give_back.value);
    } else {
        emit_byte(OP_NOTHING, node->line);
    }
    emit_byte(OP_RETURN, node->line);
}

/* ── Defer: schedule cleanup code ── */
static void compile_defer(ASTNode* node) {
    /* defer expr — compile the expression for deferred execution
     * In a full implementation, this would register the expression
     * with the function's defer stack to run before return.
     * For now, we'll emit it as a stub opcode. */
    compile_expression(node->as.defer.expr);
    emit_byte(OP_DEFER, node->line);
}

/* ── Await: wait for async result ── */
static void compile_await(ASTNode* node) {
    /* await expr — for now, this just evaluates the expression
     * In a real implementation with async support, this would block
     * waiting for a promise/future. */
    compile_expression(node->as.await.expr);
    emit_byte(OP_AWAIT, node->line);
}

static void compile_yield(ASTNode* node) {
    if (current->is_generator && current->yield_slot >= 0) {
        emit_byte(OP_GET_LOCAL, node->line);
        emit_byte((uint8_t)current->yield_slot, node->line);
        if (node->as.yield.expr) {
            compile_expression(node->as.yield.expr);
        } else {
            emit_byte(OP_NOTHING, node->line);
        }
        emit_byte(OP_ARRAY_PUSH, node->line);
        emit_byte(OP_POP, node->line);
        emit_byte(OP_NOTHING, node->line);
        return;
    }

    if (node->as.yield.expr) {
        compile_expression(node->as.yield.expr);
    } else {
        emit_byte(OP_NOTHING, node->line);
    }
}

/* ── Spawn: create a new concurrent task ── */
static void compile_spawn(ASTNode* node) {
    /* spawn expr — create a task from callable + args without eager call */
    uint8_t argc = 0;

    if (node->as.spawn.function && node->as.spawn.function->type == NODE_CALL) {
        ASTNode* call = node->as.spawn.function;
        compile_expression(call->as.call.callee);
        for (int i = 0; i < call->as.call.args.count; i++) {
            compile_expression(call->as.call.args.items[i]);
            argc++;
        }
    } else {
        compile_expression(node->as.spawn.function);
    }

    for (int i = 0; i < node->as.spawn.args.count; i++) {
        compile_expression(node->as.spawn.args.items[i]);
        argc++;
    }

    emit_byte(OP_SPAWN, node->line);
    emit_byte(argc, node->line);
}

/* ── Channel creation: channel() ── */
static void compile_channel_create(ASTNode* node) {
    emit_byte(OP_CHANNEL_CREATE, node->line);
}

/* ══════════════════════════════════════════════════════════════
 *  BREAK & SKIP (continue)
 * ══════════════════════════════════════════════════════════════ */

static void compile_break(ASTNode* node) {
    if (current->loop_start == -1) {
        fprintf(stderr, "[Compiler Error] 'break' outside of a loop at line %d\n", node->line);
        current->had_error = true;
        return;
    }
    /* Pop locals that are deeper than the loop scope */
    int pops = 0;
    for (int i = current->local_count - 1; i >= 0; i--) {
        if (current->locals[i].depth <= current->loop_scope_depth) break;
        pops++;
    }
    for (int i = 0; i < pops; i++) {
        emit_byte(OP_POP, node->line);
    }
    /* Emit forward jump — will be patched at end of loop */
    if (current->loop_exit_count < MAX_BREAK_JUMPS) {
        current->loop_exit_jumps[current->loop_exit_count++] = emit_jump(OP_JMP, node->line);
    } else {
        fprintf(stderr, "[Compiler Error] Too many break statements in loop at line %d\n", node->line);
        current->had_error = true;
    }
}

static void compile_skip(ASTNode* node) {
    if (current->loop_start == -1) {
        fprintf(stderr, "[Compiler Error] 'skip' outside of a loop at line %d\n", node->line);
        current->had_error = true;
        return;
    }
    /* Pop locals that are deeper than the loop scope */
    int pops = 0;
    for (int i = current->local_count - 1; i >= 0; i--) {
        if (current->locals[i].depth <= current->loop_scope_depth) break;
        pops++;
    }
    for (int i = 0; i < pops; i++) {
        emit_byte(OP_POP, node->line);
    }
    /* Emit a forward jump to be patched at the loop increment point.
     * This avoids skipping the increment in for-range/for-each/repeat loops. */
    if (current->loop_continue_count < MAX_BREAK_JUMPS) {
        current->loop_continue_jumps[current->loop_continue_count++] = emit_jump(OP_JMP, node->line);
    } else {
        fprintf(stderr, "[Compiler Error] Too many skip statements in loop at line %d\n", node->line);
        current->had_error = true;
    }
}

/* ══════════════════════════════════════════════════════════════
 *  STRUCT DECLARATIONS
 * ══════════════════════════════════════════════════════════════ */

static void compile_struct_decl(ASTNode* node) {
    const char* name = node->as.struct_decl.name;
    int field_count = node->as.struct_decl.field_count;
    int method_count = node->as.struct_decl.method_count;

    /* Create a constructor function for this struct.
     * The constructor takes field values and returns an ObjInstance. */
    ObjFunction* ctor = obj_function_new(name);
    ctor->arity = field_count;

    Compiler fn_compiler;
    fn_compiler.function = ctor;
    fn_compiler.enclosing = current;
    fn_compiler.local_count = 0;
    fn_compiler.scope_depth = 0;
    fn_compiler.upvalue_count = 0;
    fn_compiler.had_error = false;
    fn_compiler.loop_start = -1;
    fn_compiler.loop_exit_count = 0;
    fn_compiler.loop_continue_count = 0;
    fn_compiler.loop_scope_depth = 0;
    fn_compiler.is_generator = false;
    fn_compiler.yield_slot = -1;
    current = &fn_compiler;

    begin_scope();
    add_local("");  /* slot 0 for callee */

    /* Add parameters for each field */
    for (int i = 0; i < field_count; i++) {
        add_local(node->as.struct_decl.fields[i].name);
    }

    /* Emit OP_STRUCT to create a new instance with N fields */
    emit_byte(OP_STRUCT, node->line);
    emit_short((uint16_t)field_count, node->line);
    add_local("__inst__");
    int inst_slot = current->local_count - 1;

    /* Set each field on the instance */
    for (int i = 0; i < field_count; i++) {
        emit_byte(OP_GET_LOCAL, node->line);
        emit_byte((uint8_t)(i + 1), node->line);  /* param slot */
        emit_byte(OP_GET_LOCAL, node->line);
        emit_byte((uint8_t)inst_slot, node->line);
        int fname_idx = add_string_constant(node->as.struct_decl.fields[i].name,
                                             (int)strlen(node->as.struct_decl.fields[i].name));
        emit_byte(OP_SET_FIELD, node->line);
        emit_short((uint16_t)fname_idx, node->line);
        emit_byte(OP_POP, node->line);  /* pop set result */
    }

    /* Return the instance */
    emit_byte(OP_GET_LOCAL, node->line);
    emit_byte((uint8_t)inst_slot, node->line);
    emit_byte(OP_RETURN, node->line);

    bool had_err = fn_compiler.had_error;
    current = fn_compiler.enclosing;
    if (had_err) current->had_error = true;

    /* Compile methods separately */
    for (int i = 0; i < method_count; i++) {
        /* Methods are compiled as standalone functions named struct.method */
        StructMethod* m = &node->as.struct_decl.methods[i];
        if (m->body) {
            char method_name[512];
            snprintf(method_name, sizeof(method_name), "%s.%s", name, m->name);

            ObjFunction* mfn = obj_function_new(method_name);
            mfn->arity = m->params.count;

            Compiler m_compiler;
            m_compiler.function = mfn;
            m_compiler.enclosing = current;
            m_compiler.local_count = 0;
            m_compiler.scope_depth = 0;
            m_compiler.upvalue_count = 0;
            m_compiler.had_error = false;
            m_compiler.loop_start = -1;
            m_compiler.loop_scope_depth = 0;
            m_compiler.loop_exit_count = 0;
            m_compiler.loop_continue_count = 0;
            m_compiler.is_generator = false;
            m_compiler.yield_slot = -1;
            current = &m_compiler;

            begin_scope();
            add_local("");  /* slot 0 */
            for (int j = 0; j < m->params.count; j++) {
                add_local(m->params.items[j].name);
            }
            compile_node(m->body);
            emit_byte(OP_NOTHING, node->line);
            emit_byte(OP_RETURN, node->line);

            bool m_err = m_compiler.had_error;
            current = m_compiler.enclosing;
            if (m_err) current->had_error = true;

            mfn->upvalue_count = m_compiler.upvalue_count;

            int mfn_idx = make_constant(val_obj(mfn));
            emit_byte(OP_CLOSURE, node->line);
            emit_short((uint16_t)mfn_idx, node->line);
            for (int j = 0; j < m_compiler.upvalue_count; j++) {
                emit_byte(m_compiler.upvalues[j].is_local ? 1 : 0, node->line);
                emit_byte(m_compiler.upvalues[j].index, node->line);
            }

            int mname_idx = add_string_constant(method_name, (int)strlen(method_name));
            emit_byte(OP_DEFINE_GLOBAL, node->line);
            emit_short((uint16_t)mname_idx, node->line);
        }
    }

    /* Register constructor as a global */
    int fn_idx = make_constant(val_obj(ctor));
    emit_byte(OP_CONST, node->line);
    emit_short((uint16_t)fn_idx, node->line);

    int name_idx = add_string_constant(name, (int)strlen(name));
    emit_byte(OP_DEFINE_GLOBAL, node->line);
    emit_short((uint16_t)name_idx, node->line);
}

/* ══════════════════════════════════════════════════════════════
 *  ATTEMPT/OTHERWISE (try/catch)
 * ══════════════════════════════════════════════════════════════ */

static void compile_attempt(ASTNode* node) {
    /* attempt/otherwise compiles to:
     *   try_block code
     *   JMP end
     *   catch_block code (error name as local)
     *   end:
     * Without VM exception support, we compile both blocks
     * but the otherwise block is skipped on success. */
    compile_node(node->as.attempt.try_block);
    int end_jump = emit_jump(OP_JMP, node->line);

    /* Otherwise block — in normal execution this is skipped.
     * When VM exception support is added, errors would jump here. */
    if (node->as.attempt.catch_block) {
        begin_scope();
        /* Push error message placeholder */
        emit_byte(OP_NOTHING, node->line);
        if (node->as.attempt.error_name) {
            add_local(node->as.attempt.error_name);
        } else {
            add_local("__err__");
        }
        compile_node(node->as.attempt.catch_block);
        end_scope(node->line);
    }

    patch_jump(end_jump);
}

/* ══════════════════════════════════════════════════════════════
 *  USE / FROM-USE (Module Import)
 * ══════════════════════════════════════════════════════════════ */

static void compile_use(ASTNode* node) {
    int mod_idx = add_string_constant(node->as.use_stmt.module_name,
                                      (int)strlen(node->as.use_stmt.module_name));
    emit_byte(OP_USE_MODULE, node->line);
    emit_short((uint16_t)mod_idx, node->line);
}

static void compile_from_use(ASTNode* node) {
    int mod_idx = add_string_constant(node->as.from_use.module_name,
                                      (int)strlen(node->as.from_use.module_name));
    int sym_idx = add_string_constant(node->as.from_use.import_name,
                                      (int)strlen(node->as.from_use.import_name));
    emit_byte(OP_USE_SYMBOL, node->line);
    emit_short((uint16_t)mod_idx, node->line);
    emit_short((uint16_t)sym_idx, node->line);
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
        case NODE_FIELD_ACCESS:
        case NODE_LAMBDA:
        case NODE_LIST_COMP:
        case NODE_AWAIT:
        case NODE_CHANNEL_CREATE:
            compile_expression(node);
            break;

        case NODE_YIELD:
            compile_yield(node);
            break;

        case NODE_ASSIGN:
            compile_assign(node);
            emit_byte(OP_POP, node->line);
            break;

        case NODE_LET_DECL:
        case NODE_CONST_DECL:
            compile_var_decl(node);
            break;

        case NODE_DISPLAY:      compile_display(node);      break;
        case NODE_IF:           compile_if(node);           break;
        case NODE_MATCH:        compile_match(node);        break;
        case NODE_WHILE:        compile_while(node);        break;
        case NODE_FOR_RANGE:    compile_for_range(node);    break;
        case NODE_FOR_EACH:     compile_for_each(node);     break;
        case NODE_REPEAT:       compile_repeat(node);       break;
        case NODE_FN_DECL:      compile_fn_decl(node);      break;
        case NODE_STRUCT_DECL:  compile_struct_decl(node);  break;
        case NODE_ATTEMPT:      compile_attempt(node);      break;
        case NODE_USE:          compile_use(node);          break;
        case NODE_FROM_USE:     compile_from_use(node);     break;
        case NODE_BREAK:        compile_break(node);        break;
        case NODE_SKIP:         compile_skip(node);         break;

        case NODE_TRAIT:
        case NODE_IMPL:
        case NODE_OPERATOR_OVERLOAD:
        case NODE_DECORATOR:
        case NODE_UNSAFE:
            /* These require more complex VM support — emit warning */
            fprintf(stderr, "[Compiler Warning] Feature not yet supported in VM mode (line %d)\n",
                    node->line);
            break;

        case NODE_PASS:
            /* No-op — intentionally empty */
            break;

        case NODE_SPAWN:
            compile_spawn(node);
            break;
        
        case NODE_DEFER:
            compile_defer(node);
            break;

        case NODE_GIVE_BACK: {
            /* Tail-call position: give back f(args) can reuse current frame. */
            if (node->as.give_back.value &&
                node->as.give_back.value->type == NODE_CALL &&
                current->enclosing != NULL) {
                compile_tail_call(node->as.give_back.value);
                emit_byte(OP_RETURN, node->line);
            } else if (node->as.give_back.value) {
                compile_expression(node->as.give_back.value);
                emit_byte(OP_RETURN, node->line);
            } else {
                /* No return value - push nothing */
                emit_constant(val_nothing_v(), node->line);
                emit_byte(OP_RETURN, node->line);
            }
            break;
        }

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

        case NODE_WITH: {
            /* Scoped context binding: with expr as name { body } */
            begin_scope();
            compile_expression(node->as.with_stmt.expr);
            if (node->as.with_stmt.name) {
                add_local(node->as.with_stmt.name);
            } else {
                add_local("__with__");
            }
            compile_node(node->as.with_stmt.body);
            end_scope(node->line);
            break;
        }

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
    /* Enforce type correctness before bytecode generation. */
    int type_errors = vex_type_check(program);
    if (type_errors != 0) {
        fprintf(stderr, "[Compiler] Type checking failed with %d error(s).\n", type_errors);
        return NULL;
    }

    Compiler compiler;
    ObjFunction* fn = obj_function_new("<script>");
    fn->arity = 0;

    compiler.function = fn;
    compiler.enclosing = NULL;
    compiler.local_count = 0;
    compiler.scope_depth = 0;
    compiler.upvalue_count = 0;
    compiler.had_error = false;
    compiler.loop_start = -1;
    compiler.loop_exit_count = 0;
    compiler.loop_continue_count = 0;
    compiler.loop_scope_depth = 0;
    compiler.is_generator = false;
    compiler.yield_slot = -1;
    current = &compiler;

     /* vm_run() pushes the script closure onto vm->stack[0] before executing,
         so slot 0 is already occupied — reserve it here (same as fn declarations). */
     add_local("");

     compile_node(program);
    emit_byte(OP_HALT, 0);

    current = NULL;

    if (compiler.had_error) {
        fprintf(stderr, "[Compiler] Compilation finished with errors.\n");
        return NULL;
    }

    return fn;
}
