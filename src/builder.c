#include "builder.h"
#include "lexer.h"
#include "parser.h"
#include "ast.h"

/* ── Real C transpilation builder ──
 * Parses .vxm source → emits equivalent C code → invokes gcc to compile.
 */

static FILE* b_out;

static void b_emit_expr(ASTNode* node);

static void b_indent(int depth) {
    for (int i = 0; i < depth; i++) fprintf(b_out, "    ");
}

static void b_emit_expr(ASTNode* node) {
    if (!node) return;
    switch (node->type) {
        case NODE_INT_LITERAL:
            fprintf(b_out, "%lldLL", (long long)node->as.int_literal.value);
            break;
        case NODE_FLOAT_LITERAL:
            fprintf(b_out, "%g", node->as.float_literal.value);
            break;
        case NODE_STRING_LITERAL:
            fprintf(b_out, "\"%.*s\"", node->as.string_literal.length, node->as.string_literal.value);
            break;
        case NODE_BOOL_LITERAL:
            fprintf(b_out, "%d", node->as.bool_literal.value ? 1 : 0);
            break;
        case NODE_NOTHING_LITERAL:
            fprintf(b_out, "0");
            break;
        case NODE_IDENTIFIER:
            fprintf(b_out, "v_%s", node->as.identifier.name);
            break;
        case NODE_BINARY_OP:
            fprintf(b_out, "(");
            b_emit_expr(node->as.binary.left);
            switch (node->as.binary.op) {
                case TOKEN_PLUS:    fprintf(b_out, " + "); break;
                case TOKEN_MINUS:   fprintf(b_out, " - "); break;
                case TOKEN_STAR:    fprintf(b_out, " * "); break;
                case TOKEN_SLASH:   fprintf(b_out, " / "); break;
                case TOKEN_PERCENT: fprintf(b_out, " %% "); break;
                case TOKEN_EQ:      fprintf(b_out, " == "); break;
                case TOKEN_GT:      fprintf(b_out, " > "); break;
                case TOKEN_LT:      fprintf(b_out, " < "); break;
                case TOKEN_AND:     fprintf(b_out, " && "); break;
                case TOKEN_OR:      fprintf(b_out, " || "); break;
                default:            fprintf(b_out, " ? "); break;
            }
            b_emit_expr(node->as.binary.right);
            fprintf(b_out, ")");
            break;
        case NODE_UNARY_OP:
            if (node->as.unary.op == TOKEN_NOT) fprintf(b_out, "!(");
            else if (node->as.unary.op == TOKEN_MINUS) fprintf(b_out, "-(");
            b_emit_expr(node->as.unary.operand);
            fprintf(b_out, ")");
            break;
        case NODE_CALL:
            b_emit_expr(node->as.call.callee);
            fprintf(b_out, "(");
            for (int i = 0; i < node->as.call.args.count; i++) {
                if (i > 0) fprintf(b_out, ", ");
                b_emit_expr(node->as.call.args.items[i]);
            }
            fprintf(b_out, ")");
            break;
        default:
            fprintf(b_out, "0 /* unsupported expr */");
            break;
    }
}

static void b_emit_stmt(ASTNode* node, int depth);

static void b_emit_block(ASTNode* node, int depth) {
    if (!node) return;
    if (node->type == NODE_BLOCK) {
        for (int i = 0; i < node->as.block.statements.count; i++) {
            b_emit_stmt(node->as.block.statements.items[i], depth);
        }
    } else {
        b_emit_stmt(node, depth);
    }
}

static void b_emit_stmt(ASTNode* node, int depth) {
    if (!node) return;
    switch (node->type) {
        case NODE_LET_DECL:
        case NODE_CONST_DECL:
            b_indent(depth);
            fprintf(b_out, "long long v_%s = ", node->as.var_decl.name);
            if (node->as.var_decl.value) b_emit_expr(node->as.var_decl.value);
            else fprintf(b_out, "0");
            fprintf(b_out, ";\n");
            break;
        case NODE_DISPLAY:
            b_indent(depth);
            if (node->as.display.value && node->as.display.value->type == NODE_STRING_LITERAL) {
                fprintf(b_out, "printf(\"%%s\\n\", ");
                b_emit_expr(node->as.display.value);
                fprintf(b_out, ");\n");
            } else {
                fprintf(b_out, "printf(\"%%lld\\n\", (long long)");
                b_emit_expr(node->as.display.value);
                fprintf(b_out, ");\n");
            }
            break;
        case NODE_EXPR_STMT:
            b_indent(depth);
            b_emit_expr(node->as.expr_stmt.expr);
            fprintf(b_out, ";\n");
            break;
        case NODE_ASSIGN:
            b_indent(depth);
            b_emit_expr(node->as.assign.target);
            fprintf(b_out, " = ");
            b_emit_expr(node->as.assign.value);
            fprintf(b_out, ";\n");
            break;
        case NODE_IF:
            b_indent(depth);
            fprintf(b_out, "if (");
            b_emit_expr(node->as.if_stmt.condition);
            fprintf(b_out, ") {\n");
            b_emit_block(node->as.if_stmt.then_block, depth + 1);
            b_indent(depth);
            fprintf(b_out, "}");
            if (node->as.if_stmt.else_block) {
                fprintf(b_out, " else {\n");
                b_emit_block(node->as.if_stmt.else_block, depth + 1);
                b_indent(depth);
                fprintf(b_out, "}");
            }
            fprintf(b_out, "\n");
            break;
        case NODE_WHILE:
            b_indent(depth);
            fprintf(b_out, "while (");
            b_emit_expr(node->as.while_stmt.condition);
            fprintf(b_out, ") {\n");
            b_emit_block(node->as.while_stmt.body, depth + 1);
            b_indent(depth);
            fprintf(b_out, "}\n");
            break;
        case NODE_FOR_RANGE:
            b_indent(depth);
            fprintf(b_out, "for (long long v_%s = ", node->as.for_range.var_name);
            b_emit_expr(node->as.for_range.start);
            fprintf(b_out, "; v_%s < ", node->as.for_range.var_name);
            b_emit_expr(node->as.for_range.end);
            fprintf(b_out, "; v_%s++) {\n", node->as.for_range.var_name);
            b_emit_block(node->as.for_range.body, depth + 1);
            b_indent(depth);
            fprintf(b_out, "}\n");
            break;
        case NODE_FN_DECL:
            b_indent(depth);
            fprintf(b_out, "long long v_%s(", node->as.fn_decl.name);
            for (int i = 0; i < node->as.fn_decl.params.count; i++) {
                if (i > 0) fprintf(b_out, ", ");
                fprintf(b_out, "long long v_%s", node->as.fn_decl.params.items[i].name);
            }
            fprintf(b_out, ") {\n");
            b_emit_block(node->as.fn_decl.body, depth + 1);
            b_indent(depth);
            fprintf(b_out, "}\n");
            break;
        case NODE_GIVE_BACK:
            b_indent(depth);
            fprintf(b_out, "return ");
            b_emit_expr(node->as.give_back.value);
            fprintf(b_out, ";\n");
            break;
        case NODE_BREAK:
            b_indent(depth);
            fprintf(b_out, "break;\n");
            break;
        case NODE_SKIP:
            b_indent(depth);
            fprintf(b_out, "continue;\n");
            break;
        case NODE_REPEAT:
            b_indent(depth);
            fprintf(b_out, "for (long long _rep_i = 0; _rep_i < ");
            b_emit_expr(node->as.repeat.count);
            fprintf(b_out, "; _rep_i++) {\n");
            b_emit_block(node->as.repeat.body, depth + 1);
            b_indent(depth);
            fprintf(b_out, "}\n");
            break;
        case NODE_USE:
            b_indent(depth);
            fprintf(b_out, "/* use %s */\n", node->as.use_stmt.module_name);
            break;
        case NODE_BLOCK:
            b_emit_block(node, depth);
            break;
        case NODE_PROGRAM:
            for (int i = 0; i < node->as.program.statements.count; i++) {
                ASTNode* s = node->as.program.statements.items[i];
                /* Emit function declarations before main */
                if (s->type == NODE_FN_DECL) {
                    b_emit_stmt(s, depth);
                    fprintf(b_out, "\n");
                }
            }
            fprintf(b_out, "int main(void) {\n");
            for (int i = 0; i < node->as.program.statements.count; i++) {
                ASTNode* s = node->as.program.statements.items[i];
                if (s->type != NODE_FN_DECL && s->type != NODE_USE && s->type != NODE_STRUCT_DECL) {
                    b_emit_stmt(s, depth + 1);
                }
            }
            b_indent(depth + 1);
            fprintf(b_out, "return 0;\n");
            fprintf(b_out, "}\n");
            break;
        default:
            b_indent(depth);
            fprintf(b_out, "/* unsupported statement type %d */\n", node->type);
            break;
    }
}

bool build_native_binary(const char* input_path, const char* target_platform) {
    /* Step 1: Read and parse source */
    FILE* src = fopen(input_path, "rb");
    if (!src) {
        fprintf(stderr, "[vex build] Error: Cannot open '%s'\n", input_path);
        return false;
    }
    fseek(src, 0, SEEK_END);
    long sz = ftell(src);
    rewind(src);
    char* source = (char*)malloc(sz + 1);
    fread(source, 1, sz, src);
    source[sz] = '\0';
    fclose(src);

    Parser parser;
    parser_init(&parser, source);
    ASTNode* program = parser_parse(&parser);
    if (parser.had_error) {
        fprintf(stderr, "[vex build] Parse error in '%s'\n", input_path);
        ast_free(program);
        free(source);
        return false;
    }

    printf("[vex build] Transpiling '%s' to C...\n", input_path);

    /* Step 2: Emit C source */
    const char* c_path = "_vex_build_tmp.c";
    b_out = fopen(c_path, "w");
    if (!b_out) {
        fprintf(stderr, "[vex build] Error: Cannot create temporary C file\n");
        ast_free(program);
        free(source);
        return false;
    }

    fprintf(b_out, "/* Generated by Vexium Builder from '%s' */\n", input_path);
    fprintf(b_out, "#include <stdio.h>\n");
    fprintf(b_out, "#include <stdlib.h>\n");
    fprintf(b_out, "#include <string.h>\n\n");

    b_emit_stmt(program, 0);

    fclose(b_out);
    ast_free(program);
    free(source);

    /* Step 3: Determine output name */
    char out_name[256];
    const char* base = strrchr(input_path, '\\');
    if (!base) base = strrchr(input_path, '/');
    base = base ? base + 1 : input_path;

    snprintf(out_name, sizeof(out_name), "%.*s",
        (int)(strrchr(base, '.') ? strrchr(base, '.') - base : (int)strlen(base)), base);

#ifdef _WIN32
    char out_path[512];
    snprintf(out_path, sizeof(out_path), "%s.exe", out_name);
#else
    char out_path[512];
    snprintf(out_path, sizeof(out_path), "%s", out_name);
#endif

    /* Step 4: Compile with gcc */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "gcc -std=c99 -O2 -o %s %s -lm", out_path, c_path);
    printf("[vex build] Compiling: %s\n", cmd);

    int result = system(cmd);
    remove(c_path); /* Clean up temp file */

    if (result == 0) {
        /* Get file size */
        FILE* built = fopen(out_path, "rb");
        if (built) {
            fseek(built, 0, SEEK_END);
            long bsz = ftell(built);
            fclose(built);
            printf("[vex build] Success: '%s' (%ld bytes) for target '%s'\n",
                out_path, bsz, target_platform ? target_platform : "host");
        }
        return true;
    } else {
        fprintf(stderr, "[vex build] Error: gcc compilation failed (exit code %d)\n", result);
        fprintf(stderr, "[vex build] Hint: Ensure gcc is installed and in PATH\n");
        return false;
    }
}
