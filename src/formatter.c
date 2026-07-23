#include "formatter.h"
#include "lexer.h"
#include "parser.h"
#include "ast.h"

/* ── Real AST-based code formatter ──
 * Parses the source file into an AST, then re-emits it
 * with consistent indentation and spacing.
 */

static FILE* fmt_out;
static int fmt_indent;

static void fmt_write_indent(void) {
    for (int i = 0; i < fmt_indent; i++) fprintf(fmt_out, "    ");
}

static void fmt_emit_node(ASTNode* node);

static void fmt_emit_expr(ASTNode* node) {
    if (!node) return;
    switch (node->type) {
        case NODE_INT_LITERAL:
            fprintf(fmt_out, "%lld", (long long)node->as.int_literal.value);
            break;
        case NODE_FLOAT_LITERAL:
            fprintf(fmt_out, "%g", node->as.float_literal.value);
            break;
        case NODE_STRING_LITERAL:
            fprintf(fmt_out, "\"%.*s\"", node->as.string_literal.length, node->as.string_literal.value);
            break;
        case NODE_BOOL_LITERAL:
            fprintf(fmt_out, "%s", node->as.bool_literal.value ? "true" : "false");
            break;
        case NODE_NOTHING_LITERAL:
            fprintf(fmt_out, "nothing");
            break;
        case NODE_IDENTIFIER:
            fprintf(fmt_out, "%s", node->as.identifier.name);
            break;
        case NODE_BINARY_OP: {
            fmt_emit_expr(node->as.binary.left);
            const char* op = "?";
            switch (node->as.binary.op) {
                case TOKEN_PLUS:  op = " + "; break;
                case TOKEN_MINUS: op = " - "; break;
                case TOKEN_STAR:  op = " * "; break;
                case TOKEN_SLASH: op = " / "; break;
                case TOKEN_PERCENT: op = " % "; break;
                case TOKEN_EQ:    op = " is "; break;
                case TOKEN_GT:    op = " > "; break;
                case TOKEN_LT:    op = " < "; break;
                case TOKEN_AND:   op = " and "; break;
                case TOKEN_OR:    op = " or "; break;
                default: op = " ?? "; break;
            }
            fprintf(fmt_out, "%s", op);
            fmt_emit_expr(node->as.binary.right);
            break;
        }
        case NODE_UNARY_OP:
            if (node->as.unary.op == TOKEN_NOT) fprintf(fmt_out, "not ");
            else if (node->as.unary.op == TOKEN_MINUS) fprintf(fmt_out, "-");
            fmt_emit_expr(node->as.unary.operand);
            break;
        case NODE_CALL: {
            fmt_emit_expr(node->as.call.callee);
            fprintf(fmt_out, "(");
            for (int i = 0; i < node->as.call.args.count; i++) {
                if (i > 0) fprintf(fmt_out, ", ");
                fmt_emit_expr(node->as.call.args.items[i]);
            }
            fprintf(fmt_out, ")");
            break;
        }
        case NODE_INDEX:
            fmt_emit_expr(node->as.index_access.object);
            fprintf(fmt_out, "[");
            fmt_emit_expr(node->as.index_access.index);
            fprintf(fmt_out, "]");
            break;
        case NODE_FIELD_ACCESS:
            fmt_emit_expr(node->as.field_access.object);
            fprintf(fmt_out, ".%s", node->as.field_access.field);
            break;
        case NODE_ARRAY_LITERAL: {
            fprintf(fmt_out, "[");
            for (int i = 0; i < node->as.array_literal.elements.count; i++) {
                if (i > 0) fprintf(fmt_out, ", ");
                fmt_emit_expr(node->as.array_literal.elements.items[i]);
            }
            fprintf(fmt_out, "]");
            break;
        }
        case NODE_MAP_LITERAL: {
            fprintf(fmt_out, "{");
            for (int i = 0; i < node->as.map_literal.count; i++) {
                if (i > 0) fprintf(fmt_out, ", ");
                fmt_emit_expr(node->as.map_literal.entries[i].key);
                fprintf(fmt_out, ": ");
                fmt_emit_expr(node->as.map_literal.entries[i].value);
            }
            fprintf(fmt_out, "}");
            break;
        }
        case NODE_LAMBDA: {
            fprintf(fmt_out, "(");
            for (int i = 0; i < node->as.lambda.params.count; i++) {
                if (i > 0) fprintf(fmt_out, ", ");
                fprintf(fmt_out, "%s", node->as.lambda.params.items[i].name);
            }
            fprintf(fmt_out, ") => ");
            fmt_emit_expr(node->as.lambda.body);
            break;
        }
        default:
            fmt_emit_node(node);
            break;
    }
}

static void fmt_emit_params(ParamList* params) {
    fprintf(fmt_out, "(");
    for (int i = 0; i < params->count; i++) {
        if (i > 0) fprintf(fmt_out, ", ");
        fprintf(fmt_out, "%s", params->items[i].name);
        if (params->items[i].type_name) {
            fprintf(fmt_out, ": %s", params->items[i].type_name);
        }
    }
    fprintf(fmt_out, ")");
}

static void fmt_emit_block(ASTNode* node) {
    if (!node) return;
    if (node->type == NODE_BLOCK) {
        fmt_indent++;
        for (int i = 0; i < node->as.block.statements.count; i++) {
            fmt_write_indent();
            fmt_emit_node(node->as.block.statements.items[i]);
            fprintf(fmt_out, "\n");
        }
        fmt_indent--;
    } else {
        fmt_indent++;
        fmt_write_indent();
        fmt_emit_node(node);
        fprintf(fmt_out, "\n");
        fmt_indent--;
    }
}

static void fmt_emit_node(ASTNode* node) {
    if (!node) return;
    switch (node->type) {
        case NODE_LET_DECL:
            fprintf(fmt_out, "let %s", node->as.var_decl.name);
            if (node->as.var_decl.type_name)
                fprintf(fmt_out, ": %s", node->as.var_decl.type_name);
            if (node->as.var_decl.value) {
                fprintf(fmt_out, " be ");
                fmt_emit_expr(node->as.var_decl.value);
            }
            break;
        case NODE_CONST_DECL:
            fprintf(fmt_out, "const %s", node->as.var_decl.name);
            if (node->as.var_decl.type_name)
                fprintf(fmt_out, ": %s", node->as.var_decl.type_name);
            if (node->as.var_decl.value) {
                fprintf(fmt_out, " be ");
                fmt_emit_expr(node->as.var_decl.value);
            }
            break;
        case NODE_DISPLAY:
            fprintf(fmt_out, "display ");
            fmt_emit_expr(node->as.display.value);
            break;
        case NODE_EXPR_STMT:
            fmt_emit_expr(node->as.expr_stmt.expr);
            break;
        case NODE_ASSIGN:
            fmt_emit_expr(node->as.assign.target);
            fprintf(fmt_out, " be ");
            fmt_emit_expr(node->as.assign.value);
            break;
        case NODE_IF:
            fprintf(fmt_out, "if ");
            fmt_emit_expr(node->as.if_stmt.condition);
            fprintf(fmt_out, ":\n");
            fmt_emit_block(node->as.if_stmt.then_block);
            if (node->as.if_stmt.else_block) {
                fmt_write_indent();
                if (node->as.if_stmt.else_block->type == NODE_IF) {
                    fprintf(fmt_out, "elif ");
                    fmt_emit_expr(node->as.if_stmt.else_block->as.if_stmt.condition);
                    fprintf(fmt_out, ":\n");
                    fmt_emit_block(node->as.if_stmt.else_block->as.if_stmt.then_block);
                    if (node->as.if_stmt.else_block->as.if_stmt.else_block) {
                        fmt_write_indent();
                        fprintf(fmt_out, "else:\n");
                        fmt_emit_block(node->as.if_stmt.else_block->as.if_stmt.else_block);
                    }
                } else {
                    fprintf(fmt_out, "else:\n");
                    fmt_emit_block(node->as.if_stmt.else_block);
                }
            }
            break;
        case NODE_WHILE:
            fprintf(fmt_out, "while ");
            fmt_emit_expr(node->as.while_stmt.condition);
            fprintf(fmt_out, ":\n");
            fmt_emit_block(node->as.while_stmt.body);
            break;
        case NODE_FOR_RANGE:
            fprintf(fmt_out, "for %s in ", node->as.for_range.var_name);
            fmt_emit_expr(node->as.for_range.start);
            fprintf(fmt_out, " to ");
            fmt_emit_expr(node->as.for_range.end);
            if (node->as.for_range.step) {
                fprintf(fmt_out, " by ");
                fmt_emit_expr(node->as.for_range.step);
            }
            fprintf(fmt_out, ":\n");
            fmt_emit_block(node->as.for_range.body);
            break;
        case NODE_FOR_EACH:
            fprintf(fmt_out, "for each %s in ", node->as.for_each.var_name);
            fmt_emit_expr(node->as.for_each.iterable);
            fprintf(fmt_out, ":\n");
            fmt_emit_block(node->as.for_each.body);
            break;
        case NODE_REPEAT:
            fprintf(fmt_out, "repeat ");
            fmt_emit_expr(node->as.repeat.count);
            fprintf(fmt_out, " times:\n");
            fmt_emit_block(node->as.repeat.body);
            break;
        case NODE_FN_DECL:
            fprintf(fmt_out, "fn %s", node->as.fn_decl.name);
            fmt_emit_params(&node->as.fn_decl.params);
            if (node->as.fn_decl.return_type)
                fprintf(fmt_out, " -> %s", node->as.fn_decl.return_type);
            fprintf(fmt_out, ":\n");
            fmt_emit_block(node->as.fn_decl.body);
            break;
        case NODE_GIVE_BACK:
            fprintf(fmt_out, "give back ");
            fmt_emit_expr(node->as.give_back.value);
            break;
        case NODE_BREAK:
            fprintf(fmt_out, "break");
            break;
        case NODE_SKIP:
            fprintf(fmt_out, "skip");
            break;
        case NODE_PASS:
            fprintf(fmt_out, "pass");
            break;
        case NODE_USE:
            fprintf(fmt_out, "use %s", node->as.use_stmt.module_name);
            break;
        case NODE_FROM_USE:
            fprintf(fmt_out, "from %s use %s", node->as.from_use.module_name, node->as.from_use.import_name);
            break;
        case NODE_STRUCT_DECL: {
            fprintf(fmt_out, "struct %s", node->as.struct_decl.name);
            if (node->as.struct_decl.parent_name)
                fprintf(fmt_out, " extends %s", node->as.struct_decl.parent_name);
            fprintf(fmt_out, ":\n");
            fmt_indent++;
            for (int i = 0; i < node->as.struct_decl.field_count; i++) {
                fmt_write_indent();
                fprintf(fmt_out, "has %s", node->as.struct_decl.fields[i].name);
                if (node->as.struct_decl.fields[i].type_name)
                    fprintf(fmt_out, ": %s", node->as.struct_decl.fields[i].type_name);
                fprintf(fmt_out, "\n");
            }
            for (int i = 0; i < node->as.struct_decl.method_count; i++) {
                fmt_write_indent();
                fprintf(fmt_out, "can %s", node->as.struct_decl.methods[i].name);
                fmt_emit_params(&node->as.struct_decl.methods[i].params);
                if (node->as.struct_decl.methods[i].return_type)
                    fprintf(fmt_out, " -> %s", node->as.struct_decl.methods[i].return_type);
                fprintf(fmt_out, ":\n");
                fmt_emit_block(node->as.struct_decl.methods[i].body);
            }
            fmt_indent--;
            break;
        }
        case NODE_MATCH: {
            fprintf(fmt_out, "match ");
            fmt_emit_expr(node->as.match_stmt.expr);
            fprintf(fmt_out, ":\n");
            fmt_indent++;
            for (int i = 0; i < node->as.match_stmt.arms.count; i++) {
                fmt_write_indent();
                fmt_emit_expr(node->as.match_stmt.arms.items[i].pattern);
                fprintf(fmt_out, " => ");
                fmt_emit_node(node->as.match_stmt.arms.items[i].body);
                fprintf(fmt_out, "\n");
            }
            fmt_indent--;
            break;
        }
        case NODE_ATTEMPT:
            fprintf(fmt_out, "attempt:\n");
            fmt_emit_block(node->as.attempt.try_block);
            fmt_write_indent();
            fprintf(fmt_out, "otherwise");
            if (node->as.attempt.error_name)
                fprintf(fmt_out, " %s", node->as.attempt.error_name);
            fprintf(fmt_out, ":\n");
            fmt_emit_block(node->as.attempt.catch_block);
            break;
        case NODE_BLOCK:
            for (int i = 0; i < node->as.block.statements.count; i++) {
                fmt_write_indent();
                fmt_emit_node(node->as.block.statements.items[i]);
                fprintf(fmt_out, "\n");
            }
            break;
        case NODE_PROGRAM:
            for (int i = 0; i < node->as.program.statements.count; i++) {
                fmt_emit_node(node->as.program.statements.items[i]);
                fprintf(fmt_out, "\n");
            }
            break;
        default:
            fmt_emit_expr(node);
            break;
    }
}

bool format_vexium_file(const char* file_path) {
    FILE* f = fopen(file_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open '%s' for formatting.\n", file_path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char* source = (char*)malloc(sz + 1);
    fread(source, 1, sz, f);
    source[sz] = '\0';
    fclose(f);

    Parser parser;
    parser_init(&parser, source);
    ASTNode* program = parser_parse(&parser);

    if (parser.had_error) {
        fprintf(stderr, "[vex fmt] Parse error in '%s'. Cannot format.\n", file_path);
        ast_free(program);
        free(source);
        return false;
    }

    /* Write formatted output to temp file, then rename */
    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "%s.fmt.tmp", file_path);
    fmt_out = fopen(tmp_path, "w");
    if (!fmt_out) {
        fprintf(stderr, "Error: Cannot create temp file for formatting.\n");
        ast_free(program);
        free(source);
        return false;
    }

    fmt_indent = 0;
    fmt_emit_node(program);

    fclose(fmt_out);
    ast_free(program);
    free(source);

    /* Replace original with formatted version */
    remove(file_path);
    rename(tmp_path, file_path);

    printf("[vex fmt] File '%s' formatted successfully.\n", file_path);
    return true;
}
