#include "compiler.h"

static void compile_node(ASTNode* node, Chunk* chunk);

static int emit_jump(Chunk* chunk, uint8_t instruction, int line) {
    chunk_write(chunk, instruction, line);
    chunk_write(chunk, 0xff, line);
    chunk_write(chunk, 0xff, line);
    return chunk->count - 2;
}

static void patch_jump(Chunk* chunk, int offset) {
    int jump = chunk->count - offset - 2;
    if (jump > 65535) {
        fprintf(stderr, "Error: Too much code to jump over.\n");
    }
    chunk->code[offset] = (jump >> 8) & 0xff;
    chunk->code[offset + 1] = jump & 0xff;
}

static void emit_loop(Chunk* chunk, int loop_start, int line) {
    chunk_write(chunk, OP_LOOP, line);
    int offset = chunk->count - loop_start + 2;
    if (offset > 65535) {
        fprintf(stderr, "Error: Loop body too large.\n");
    }
    chunk_write(chunk, (offset >> 8) & 0xff, line);
    chunk_write(chunk, offset & 0xff, line);
}

static void compile_node(ASTNode* node, Chunk* chunk) {
    if (node == NULL) return;

    switch (node->type) {
        case NODE_INT_LITERAL: {
            int constant = chunk_add_constant(chunk, int_to_value(node->as.int_literal.value));
            chunk_write(chunk, OP_CONSTANT, node->line);
            chunk_write(chunk, (uint8_t)constant, node->line);
            break;
        }
        case NODE_FLOAT_LITERAL: {
            int constant = chunk_add_constant(chunk, double_to_value(node->as.float_literal.value));
            chunk_write(chunk, OP_CONSTANT, node->line);
            chunk_write(chunk, (uint8_t)constant, node->line);
            break;
        }
        case NODE_BOOL_LITERAL: {
            chunk_write(chunk, node->as.bool_literal.value ? OP_TRUE : OP_FALSE, node->line);
            break;
        }
        case NODE_NOTHING_LITERAL: {
            chunk_write(chunk, OP_NIL, node->line);
            break;
        }
        case NODE_STRING_LITERAL: {
            ObjString* str = allocate_string(node->as.string_literal.value, node->as.string_literal.length);
            int constant = chunk_add_constant(chunk, obj_to_value((Obj*)str));
            chunk_write(chunk, OP_CONSTANT, node->line);
            chunk_write(chunk, (uint8_t)constant, node->line);
            break;
        }
        case NODE_IDENTIFIER: {
            ObjString* name = allocate_string(node->as.identifier.name, (int)strlen(node->as.identifier.name));
            int constant = chunk_add_constant(chunk, obj_to_value((Obj*)name));
            chunk_write(chunk, OP_GET_GLOBAL, node->line);
            chunk_write(chunk, (uint8_t)constant, node->line);
            break;
        }
        case NODE_LET_DECL:
        case NODE_CONST_DECL: {
            if (node->as.var_decl.value) {
                compile_node(node->as.var_decl.value, chunk);
            } else {
                chunk_write(chunk, OP_NIL, node->line);
            }
            ObjString* name = allocate_string(node->as.var_decl.name, (int)strlen(node->as.var_decl.name));
            int constant = chunk_add_constant(chunk, obj_to_value((Obj*)name));
            chunk_write(chunk, OP_SET_GLOBAL, node->line);
            chunk_write(chunk, (uint8_t)constant, node->line);
            break;
        }
        case NODE_ASSIGN: {
            compile_node(node->as.assign.value, chunk);
            if (node->as.assign.target->type == NODE_IDENTIFIER) {
                ObjString* name = allocate_string(node->as.assign.target->as.identifier.name, (int)strlen(node->as.assign.target->as.identifier.name));
                int constant = chunk_add_constant(chunk, obj_to_value((Obj*)name));
                chunk_write(chunk, OP_SET_GLOBAL, node->line);
                chunk_write(chunk, (uint8_t)constant, node->line);
            }
            break;
        }
        case NODE_BINARY_OP: {
            compile_node(node->as.binary.left, chunk);
            compile_node(node->as.binary.right, chunk);
            switch (node->as.binary.op) {
                case TOKEN_PLUS:     chunk_write(chunk, OP_ADD, node->line); break;
                case TOKEN_MINUS:    chunk_write(chunk, OP_SUBTRACT, node->line); break;
                case TOKEN_STAR:     chunk_write(chunk, OP_MULTIPLY, node->line); break;
                case TOKEN_SLASH:    chunk_write(chunk, OP_DIVIDE, node->line); break;
                case TOKEN_EQ:       chunk_write(chunk, OP_EQUAL, node->line); break;
                case TOKEN_GT:       chunk_write(chunk, OP_GREATER, node->line); break;
                case TOKEN_LT:       chunk_write(chunk, OP_LESS, node->line); break;
                default: break;
            }
            break;
        }
        case NODE_UNARY_OP: {
            compile_node(node->as.unary.operand, chunk);
            if (node->as.unary.op == TOKEN_NOT) chunk_write(chunk, OP_NOT, node->line);
            else if (node->as.unary.op == TOKEN_MINUS) chunk_write(chunk, OP_NEGATE, node->line);
            break;
        }
        case NODE_IF: {
            compile_node(node->as.if_stmt.condition, chunk);
            int then_jump = emit_jump(chunk, OP_JUMP_IF_FALSE, node->line);
            chunk_write(chunk, OP_POP, node->line);

            compile_node(node->as.if_stmt.then_block, chunk);

            int else_jump = emit_jump(chunk, OP_JUMP, node->line);
            patch_jump(chunk, then_jump);
            chunk_write(chunk, OP_POP, node->line);

            if (node->as.if_stmt.else_block) {
                compile_node(node->as.if_stmt.else_block, chunk);
            }
            patch_jump(chunk, else_jump);
            break;
        }
        case NODE_WHILE: {
            int loop_start = chunk->count;
            compile_node(node->as.while_stmt.condition, chunk);

            int exit_jump = emit_jump(chunk, OP_JUMP_IF_FALSE, node->line);
            chunk_write(chunk, OP_POP, node->line);

            compile_node(node->as.while_stmt.body, chunk);
            emit_loop(chunk, loop_start, node->line);

            patch_jump(chunk, exit_jump);
            chunk_write(chunk, OP_POP, node->line);
            break;
        }
        case NODE_REPEAT: {
            /* Compile as count loop */
            int loop_start = chunk->count;
            compile_node(node->as.repeat.count, chunk);
            int exit_jump = emit_jump(chunk, OP_JUMP_IF_FALSE, node->line);
            chunk_write(chunk, OP_POP, node->line);

            compile_node(node->as.repeat.body, chunk);
            emit_loop(chunk, loop_start, node->line);

            patch_jump(chunk, exit_jump);
            chunk_write(chunk, OP_POP, node->line);
            break;
        }
        case NODE_DISPLAY: {
            compile_node(node->as.display.value, chunk);
            chunk_write(chunk, OP_PRINT, node->line);
            break;
        }
        case NODE_BLOCK: {
            for (int i = 0; i < node->as.block.statements.count; i++) {
                compile_node(node->as.block.statements.items[i], chunk);
            }
            break;
        }
        case NODE_EXPR_STMT: {
            compile_node(node->as.expr_stmt.expr, chunk);
            chunk_write(chunk, OP_POP, node->line);
            break;
        }
        case NODE_PROGRAM: {
            for (int i = 0; i < node->as.program.statements.count; i++) {
                compile_node(node->as.program.statements.items[i], chunk);
            }
            chunk_write(chunk, OP_RETURN, 0);
            break;
        }
        default:
            break;
    }
}

bool compile_ast_to_chunk(ASTNode* program, Chunk* chunk) {
    chunk_init(chunk);
    compile_node(program, chunk);
    return true;
}
