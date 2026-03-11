#include "ast.h"

/* ══════════════════════════════════════════════════════════════
 *  NODE CREATION
 * ══════════════════════════════════════════════════════════════ */

ASTNode* ast_new_node(NodeType type, int line, int column) {
    ASTNode* node = (ASTNode*)calloc(1, sizeof(ASTNode));
    node->type = type;
    node->line = line;
    node->column = column;
    return node;
}

/* ══════════════════════════════════════════════════════════════
 *  NODELIST
 * ══════════════════════════════════════════════════════════════ */

void nodelist_init(NodeList* list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void nodelist_add(NodeList* list, ASTNode* node) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = (ASTNode**)realloc(list->items,
            sizeof(ASTNode*) * list->capacity);
    }
    list->items[list->count++] = node;
}

/* ══════════════════════════════════════════════════════════════
 *  PARAMLIST
 * ══════════════════════════════════════════════════════════════ */

void paramlist_init(ParamList* list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void paramlist_add(ParamList* list, const char* name, const char* type_name) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = (Param*)realloc(list->items,
            sizeof(Param) * list->capacity);
    }
    Param* p = &list->items[list->count++];
    p->name = vex_strdup(name, (int)strlen(name));
    p->type_name = type_name ? vex_strdup(type_name, (int)strlen(type_name)) : NULL;
    p->default_value = NULL;
}

/* ══════════════════════════════════════════════════════════════
 *  AST PRINTING (debug)
 * ══════════════════════════════════════════════════════════════ */

static void print_indent(int level) {
    for (int i = 0; i < level; i++) printf("  ");
}

static const char* node_type_name(NodeType type) {
    switch (type) {
        case NODE_INT_LITERAL:     return "IntLiteral";
        case NODE_FLOAT_LITERAL:   return "FloatLiteral";
        case NODE_STRING_LITERAL:  return "StringLiteral";
        case NODE_BOOL_LITERAL:    return "BoolLiteral";
        case NODE_NOTHING_LITERAL: return "Nothing";
        case NODE_IDENTIFIER:      return "Identifier";
        case NODE_BINARY_OP:       return "BinaryOp";
        case NODE_UNARY_OP:        return "UnaryOp";
        case NODE_CALL:            return "Call";
        case NODE_INDEX:           return "Index";
        case NODE_FIELD_ACCESS:    return "FieldAccess";
        case NODE_ARRAY_LITERAL:   return "ArrayLiteral";
        case NODE_MAP_LITERAL:     return "MapLiteral";
        case NODE_ASSIGN:          return "Assign";
        case NODE_LET_DECL:        return "LetDecl";
        case NODE_CONST_DECL:      return "ConstDecl";
        case NODE_EXPR_STMT:       return "ExprStmt";
        case NODE_DISPLAY:         return "Display";
        case NODE_IF:              return "If";
        case NODE_WHILE:           return "While";
        case NODE_FOR_RANGE:       return "ForRange";
        case NODE_FOR_EACH:        return "ForEach";
        case NODE_REPEAT:          return "Repeat";
        case NODE_FN_DECL:         return "FnDecl";
        case NODE_GIVE_BACK:       return "GiveBack";
        case NODE_BREAK:           return "Break";
        case NODE_SKIP:            return "Skip";
        case NODE_STRUCT_DECL:     return "StructDecl";
        case NODE_MATCH:           return "Match";
        case NODE_USE:             return "Use";
        case NODE_FROM_USE:        return "FromUse";
        case NODE_ATTEMPT:         return "Attempt";
        case NODE_PASS:            return "Pass";
        case NODE_WITH:            return "With";
        case NODE_BLOCK:           return "Block";
        case NODE_PROGRAM:         return "Program";
        case NODE_LAMBDA:          return "Lambda";
        case NODE_LIST_COMP:       return "ListComp";
    }
    return "Unknown";
}

void ast_print(ASTNode* node, int indent) {
    if (node == NULL) {
        print_indent(indent);
        printf("(null)\n");
        return;
    }

    print_indent(indent);

    switch (node->type) {
        case NODE_INT_LITERAL:
            printf("Int(%lld)\n", (long long)node->as.int_literal.value);
            break;

        case NODE_FLOAT_LITERAL:
            printf("Float(%g)\n", node->as.float_literal.value);
            break;

        case NODE_STRING_LITERAL:
            printf("String(\"%s\")\n", node->as.string_literal.value);
            break;

        case NODE_BOOL_LITERAL:
            printf("Bool(%s)\n", node->as.bool_literal.value ? "true" : "false");
            break;

        case NODE_NOTHING_LITERAL:
            printf("Nothing\n");
            break;

        case NODE_IDENTIFIER:
            printf("Id(%s)\n", node->as.identifier.name);
            break;

        case NODE_BINARY_OP:
            printf("BinaryOp(%s)\n", token_type_name(node->as.binary.op));
            ast_print(node->as.binary.left, indent + 1);
            ast_print(node->as.binary.right, indent + 1);
            break;

        case NODE_UNARY_OP:
            printf("UnaryOp(%s)\n", token_type_name(node->as.unary.op));
            ast_print(node->as.unary.operand, indent + 1);
            break;

        case NODE_CALL:
            printf("Call\n");
            print_indent(indent + 1); printf("callee:\n");
            ast_print(node->as.call.callee, indent + 2);
            print_indent(indent + 1); printf("args: (%d)\n", node->as.call.args.count);
            for (int i = 0; i < node->as.call.args.count; i++)
                ast_print(node->as.call.args.items[i], indent + 2);
            break;

        case NODE_INDEX:
            printf(node->as.index_access.is_slice ? "Slice\n" : "Index\n");
            ast_print(node->as.index_access.object, indent + 1);
            ast_print(node->as.index_access.index, indent + 1);
            if (node->as.index_access.is_slice) {
                ast_print(node->as.index_access.end, indent + 1);
                ast_print(node->as.index_access.step, indent + 1);
            }
            break;

        case NODE_FIELD_ACCESS:
            printf("Field(.%s)\n", node->as.field_access.field);
            ast_print(node->as.field_access.object, indent + 1);
            break;

        case NODE_ARRAY_LITERAL:
            printf("Array(%d items)\n", node->as.array_literal.elements.count);
            for (int i = 0; i < node->as.array_literal.elements.count; i++)
                ast_print(node->as.array_literal.elements.items[i], indent + 1);
            break;

        case NODE_MAP_LITERAL:
            printf("Map(%d entries)\n", node->as.map_literal.count);
            for (int i = 0; i < node->as.map_literal.count; i++) {
                print_indent(indent + 1); printf("key:\n");
                ast_print(node->as.map_literal.entries[i].key, indent + 2);
                print_indent(indent + 1); printf("val:\n");
                ast_print(node->as.map_literal.entries[i].value, indent + 2);
            }
            break;

        case NODE_ASSIGN:
            printf("Assign(%s)\n", token_type_name(node->as.assign.op));
            ast_print(node->as.assign.target, indent + 1);
            ast_print(node->as.assign.value, indent + 1);
            break;

        case NODE_LET_DECL:
        case NODE_CONST_DECL:
            printf("%s(%s", node->type == NODE_LET_DECL ? "Let" : "Const",
                   node->as.var_decl.name);
            if (node->as.var_decl.type_name)
                printf(": %s", node->as.var_decl.type_name);
            printf(")\n");
            if (node->as.var_decl.value)
                ast_print(node->as.var_decl.value, indent + 1);
            break;

        case NODE_DISPLAY:
            printf("Display\n");
            ast_print(node->as.display.value, indent + 1);
            break;

        case NODE_IF:
            printf("If\n");
            print_indent(indent + 1); printf("cond:\n");
            ast_print(node->as.if_stmt.condition, indent + 2);
            print_indent(indent + 1); printf("then:\n");
            ast_print(node->as.if_stmt.then_block, indent + 2);
            if (node->as.if_stmt.else_block) {
                print_indent(indent + 1); printf("else:\n");
                ast_print(node->as.if_stmt.else_block, indent + 2);
            }
            break;

        case NODE_WHILE:
            printf("While\n");
            print_indent(indent + 1); printf("cond:\n");
            ast_print(node->as.while_stmt.condition, indent + 2);
            print_indent(indent + 1); printf("body:\n");
            ast_print(node->as.while_stmt.body, indent + 2);
            break;

        case NODE_FOR_RANGE:
            printf("ForRange(%s)\n", node->as.for_range.var_name);
            print_indent(indent + 1); printf("start:\n");
            ast_print(node->as.for_range.start, indent + 2);
            print_indent(indent + 1); printf("end:\n");
            ast_print(node->as.for_range.end, indent + 2);
            if (node->as.for_range.step) {
                print_indent(indent + 1); printf("step:\n");
                ast_print(node->as.for_range.step, indent + 2);
            }
            print_indent(indent + 1); printf("body:\n");
            ast_print(node->as.for_range.body, indent + 2);
            break;

        case NODE_FOR_EACH:
            printf("ForEach(%s)\n", node->as.for_each.var_name);
            print_indent(indent + 1); printf("iterable:\n");
            ast_print(node->as.for_each.iterable, indent + 2);
            print_indent(indent + 1); printf("body:\n");
            ast_print(node->as.for_each.body, indent + 2);
            break;

        case NODE_REPEAT:
            printf("Repeat\n");
            print_indent(indent + 1); printf("count:\n");
            ast_print(node->as.repeat.count, indent + 2);
            print_indent(indent + 1); printf("body:\n");
            ast_print(node->as.repeat.body, indent + 2);
            break;

        case NODE_FN_DECL:
            printf("Fn(%s", node->as.fn_decl.name);
            if (node->as.fn_decl.return_type)
                printf(" -> %s", node->as.fn_decl.return_type);
            printf(")\n");
            print_indent(indent + 1); printf("params: (");
            for (int i = 0; i < node->as.fn_decl.params.count; i++) {
                if (i > 0) printf(", ");
                printf("%s", node->as.fn_decl.params.items[i].name);
                if (node->as.fn_decl.params.items[i].type_name)
                    printf(": %s", node->as.fn_decl.params.items[i].type_name);
            }
            printf(")\n");
            print_indent(indent + 1); printf("body:\n");
            ast_print(node->as.fn_decl.body, indent + 2);
            break;

        case NODE_GIVE_BACK:
            printf("GiveBack\n");
            if (node->as.give_back.value)
                ast_print(node->as.give_back.value, indent + 1);
            break;

        case NODE_BREAK:
            printf("Break\n");
            break;

        case NODE_SKIP:
            printf("Skip\n");
            break;

        case NODE_PASS:
            printf("Pass\n");
            break;

        case NODE_STRUCT_DECL:
            printf("Struct(%s)\n", node->as.struct_decl.name);
            for (int i = 0; i < node->as.struct_decl.field_count; i++) {
                print_indent(indent + 1);
                printf("has %s: %s\n",
                    node->as.struct_decl.fields[i].name,
                    node->as.struct_decl.fields[i].type_name);
            }
            for (int i = 0; i < node->as.struct_decl.method_count; i++) {
                print_indent(indent + 1);
                printf("can %s(", node->as.struct_decl.methods[i].name);
                for (int j = 0; j < node->as.struct_decl.methods[i].params.count; j++) {
                    if (j > 0) printf(", ");
                    printf("%s", node->as.struct_decl.methods[i].params.items[j].name);
                }
                printf(")\n");
                ast_print(node->as.struct_decl.methods[i].body, indent + 2);
            }
            break;

        case NODE_MATCH:
            printf("Match\n");
            ast_print(node->as.match_stmt.expr, indent + 1);
            for (int i = 0; i < node->as.match_stmt.arms.count; i++) {
                print_indent(indent + 1); printf("arm:\n");
                ast_print(node->as.match_stmt.arms.items[i].pattern, indent + 2);
                ast_print(node->as.match_stmt.arms.items[i].body, indent + 2);
            }
            break;

        case NODE_USE:
            printf("Use(%s)\n", node->as.use_stmt.module_name);
            break;

        case NODE_FROM_USE:
            printf("FromUse(%s.%s)\n",
                node->as.from_use.module_name,
                node->as.from_use.import_name);
            break;

        case NODE_ATTEMPT:
            printf("Attempt\n");
            print_indent(indent + 1); printf("try:\n");
            ast_print(node->as.attempt.try_block, indent + 2);
            if (node->as.attempt.error_name) {
                print_indent(indent + 1);
                printf("otherwise(%s):\n", node->as.attempt.error_name);
            } else {
                print_indent(indent + 1); printf("otherwise:\n");
            }
            ast_print(node->as.attempt.catch_block, indent + 2);
            break;

        case NODE_BLOCK:
            printf("Block(%d stmts)\n", node->as.block.statements.count);
            for (int i = 0; i < node->as.block.statements.count; i++)
                ast_print(node->as.block.statements.items[i], indent + 1);
            break;

        case NODE_PROGRAM:
            printf("Program(%d stmts)\n", node->as.program.statements.count);
            for (int i = 0; i < node->as.program.statements.count; i++)
                ast_print(node->as.program.statements.items[i], indent + 1);
            break;

        case NODE_EXPR_STMT:
            printf("ExprStmt\n");
            ast_print(node->as.expr_stmt.expr, indent + 1);
            break;

        case NODE_LAMBDA:
            printf("Lambda(");
            for (int i = 0; i < node->as.lambda.params.count; i++) {
                if (i > 0) printf(", ");
                printf("%s", node->as.lambda.params.items[i].name);
            }
            printf(")\n");
            ast_print(node->as.lambda.body, indent + 1);
            break;

        case NODE_LIST_COMP:
            printf("ListComp(%s)\n", node->as.list_comp.var_name);
            print_indent(indent + 1); printf("expr:\n");
            ast_print(node->as.list_comp.expr, indent + 2);
            print_indent(indent + 1); printf("iterable:\n");
            ast_print(node->as.list_comp.iterable, indent + 2);
            if (node->as.list_comp.condition) {
                print_indent(indent + 1); printf("if:\n");
                ast_print(node->as.list_comp.condition, indent + 2);
            }
            break;
    }
}

/* ══════════════════════════════════════════════════════════════
 *  MEMORY MANAGEMENT
 * ══════════════════════════════════════════════════════════════ */

void ast_free(ASTNode* node) {
    if (node == NULL) return;

    switch (node->type) {
        case NODE_STRING_LITERAL:
            free(node->as.string_literal.value);
            break;
        case NODE_IDENTIFIER:
            free(node->as.identifier.name);
            break;
        case NODE_BINARY_OP:
            ast_free(node->as.binary.left);
            ast_free(node->as.binary.right);
            break;
        case NODE_UNARY_OP:
            ast_free(node->as.unary.operand);
            break;
        case NODE_CALL:
            ast_free(node->as.call.callee);
            for (int i = 0; i < node->as.call.args.count; i++)
                ast_free(node->as.call.args.items[i]);
            free(node->as.call.args.items);
            break;
        case NODE_INDEX:
            ast_free(node->as.index_access.object);
            ast_free(node->as.index_access.index);
            ast_free(node->as.index_access.end);
            ast_free(node->as.index_access.step);
            break;
        case NODE_FIELD_ACCESS:
            ast_free(node->as.field_access.object);
            free(node->as.field_access.field);
            break;
        case NODE_ARRAY_LITERAL:
            for (int i = 0; i < node->as.array_literal.elements.count; i++)
                ast_free(node->as.array_literal.elements.items[i]);
            free(node->as.array_literal.elements.items);
            break;
        case NODE_MAP_LITERAL:
            for (int i = 0; i < node->as.map_literal.count; i++) {
                ast_free(node->as.map_literal.entries[i].key);
                ast_free(node->as.map_literal.entries[i].value);
            }
            free(node->as.map_literal.entries);
            break;
        case NODE_ASSIGN:
            ast_free(node->as.assign.target);
            ast_free(node->as.assign.value);
            break;
        case NODE_LET_DECL:
        case NODE_CONST_DECL:
            free(node->as.var_decl.name);
            free(node->as.var_decl.type_name);
            ast_free(node->as.var_decl.value);
            break;
        case NODE_DISPLAY:
            ast_free(node->as.display.value);
            break;
        case NODE_IF:
            ast_free(node->as.if_stmt.condition);
            ast_free(node->as.if_stmt.then_block);
            ast_free(node->as.if_stmt.else_block);
            break;
        case NODE_WHILE:
            ast_free(node->as.while_stmt.condition);
            ast_free(node->as.while_stmt.body);
            break;
        case NODE_FOR_RANGE:
            free(node->as.for_range.var_name);
            ast_free(node->as.for_range.start);
            ast_free(node->as.for_range.end);
            ast_free(node->as.for_range.step);
            ast_free(node->as.for_range.body);
            break;
        case NODE_FOR_EACH:
            free(node->as.for_each.var_name);
            ast_free(node->as.for_each.iterable);
            ast_free(node->as.for_each.body);
            break;
        case NODE_REPEAT:
            ast_free(node->as.repeat.count);
            ast_free(node->as.repeat.body);
            break;
        case NODE_FN_DECL:
            free(node->as.fn_decl.name);
            free(node->as.fn_decl.return_type);
            for (int i = 0; i < node->as.fn_decl.params.count; i++) {
                free(node->as.fn_decl.params.items[i].name);
                free(node->as.fn_decl.params.items[i].type_name);
            }
            free(node->as.fn_decl.params.items);
            ast_free(node->as.fn_decl.body);
            break;
        case NODE_GIVE_BACK:
            ast_free(node->as.give_back.value);
            break;
        case NODE_STRUCT_DECL:
            free(node->as.struct_decl.name);
            for (int i = 0; i < node->as.struct_decl.field_count; i++) {
                free(node->as.struct_decl.fields[i].name);
                free(node->as.struct_decl.fields[i].type_name);
            }
            free(node->as.struct_decl.fields);
            for (int i = 0; i < node->as.struct_decl.method_count; i++) {
                free(node->as.struct_decl.methods[i].name);
                free(node->as.struct_decl.methods[i].return_type);
                for (int j = 0; j < node->as.struct_decl.methods[i].params.count; j++) {
                    free(node->as.struct_decl.methods[i].params.items[j].name);
                    free(node->as.struct_decl.methods[i].params.items[j].type_name);
                }
                free(node->as.struct_decl.methods[i].params.items);
                ast_free(node->as.struct_decl.methods[i].body);
            }
            free(node->as.struct_decl.methods);
            break;
        case NODE_MATCH:
            ast_free(node->as.match_stmt.expr);
            for (int i = 0; i < node->as.match_stmt.arms.count; i++) {
                ast_free(node->as.match_stmt.arms.items[i].pattern);
                ast_free(node->as.match_stmt.arms.items[i].body);
            }
            free(node->as.match_stmt.arms.items);
            break;
        case NODE_USE:
            free(node->as.use_stmt.module_name);
            break;
        case NODE_FROM_USE:
            free(node->as.from_use.module_name);
            free(node->as.from_use.import_name);
            break;
        case NODE_ATTEMPT:
            ast_free(node->as.attempt.try_block);
            free(node->as.attempt.error_name);
            ast_free(node->as.attempt.catch_block);
            break;
        case NODE_WITH:
            ast_free(node->as.with_stmt.expr);
            free(node->as.with_stmt.name);
            ast_free(node->as.with_stmt.body);
            break;
        case NODE_BLOCK:
            for (int i = 0; i < node->as.block.statements.count; i++)
                ast_free(node->as.block.statements.items[i]);
            free(node->as.block.statements.items);
            break;
        case NODE_PROGRAM:
            for (int i = 0; i < node->as.program.statements.count; i++)
                ast_free(node->as.program.statements.items[i]);
            free(node->as.program.statements.items);
            break;
        case NODE_EXPR_STMT:
            ast_free(node->as.expr_stmt.expr);
            break;
        case NODE_BREAK:
        case NODE_SKIP:
        case NODE_PASS:
        case NODE_INT_LITERAL:
        case NODE_FLOAT_LITERAL:
        case NODE_BOOL_LITERAL:
        case NODE_NOTHING_LITERAL:
            break;
        case NODE_LAMBDA:
            for (int i = 0; i < node->as.lambda.params.count; i++) {
                free(node->as.lambda.params.items[i].name);
                free(node->as.lambda.params.items[i].type_name);
            }
            free(node->as.lambda.params.items);
            ast_free(node->as.lambda.body);
            break;
        case NODE_LIST_COMP:
            ast_free(node->as.list_comp.expr);
            free(node->as.list_comp.var_name);
            ast_free(node->as.list_comp.iterable);
            ast_free(node->as.list_comp.condition);
            break;
    }

    free(node);
}
