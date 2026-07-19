#include "parser.h"

static void advance_token(Parser* p) {
    p->previous = p->current;
    for (;;) {
        p->current = lexer_next_token(&p->lexer);
        if (p->current.type != TOKEN_ERROR) break;

        fprintf(stderr, "Error [line %d]: %.*s\n",
            p->current.line, p->current.length, p->current.start);
        p->had_error = true;
    }
}

static bool check(Parser* p, TokenType type) {
    return p->current.type == type;
}

static bool match_token(Parser* p, TokenType type) {
    if (!check(p, type)) return false;
    advance_token(p);
    return true;
}

static Token consume(Parser* p, TokenType type, const char* message) {
    if (check(p, type)) {
        Token tok = p->current;
        advance_token(p);
        return tok;
    }
    fprintf(stderr, "Error [line %d]: %s (got %s '%.*s')\n",
        p->current.line, message,
        token_type_name(p->current.type),
        p->current.length, p->current.start);
    p->had_error = true;
    return p->current;
}

static void skip_newlines(Parser* p) {
    while (check(p, TOKEN_NEWLINE)) advance_token(p);
}

static void expect_newline(Parser* p) {
    if (check(p, TOKEN_NEWLINE) || check(p, TOKEN_EOF)) {
        skip_newlines(p);
    }

}

static char* token_text(Token tok) {
    return vex_strdup(tok.start, tok.length);
}

static char* token_string_content(Token tok) {

    return vex_strdup(tok.start + 1, tok.length - 2);
}

static ASTNode* parse_expression(Parser* p);
static ASTNode* parse_statement(Parser* p);
static ASTNode* parse_block(Parser* p);

static ASTNode* parse_primary(Parser* p) {

    if (match_token(p, TOKEN_INT)) {
        ASTNode* node = ast_new_node(NODE_INT_LITERAL,
            p->previous.line, p->previous.column);

        char* text = token_text(p->previous);
        char* clean = (char*)malloc(strlen(text) + 1);
        int j = 0;
        for (int i = 0; text[i]; i++) {
            if (text[i] != '_') clean[j++] = text[i];
        }
        clean[j] = '\0';
        if (clean[0] == '0' && (clean[1] == 'x' || clean[1] == 'X'))
            node->as.int_literal.value = strtoll(clean, NULL, 16);
        else if (clean[0] == '0' && (clean[1] == 'b' || clean[1] == 'B'))
            node->as.int_literal.value = strtoll(clean + 2, NULL, 2);
        else
            node->as.int_literal.value = strtoll(clean, NULL, 10);
        free(text);
        free(clean);
        return node;
    }

    if (match_token(p, TOKEN_FLOAT)) {
        ASTNode* node = ast_new_node(NODE_FLOAT_LITERAL,
            p->previous.line, p->previous.column);
        char* text = token_text(p->previous);
        node->as.float_literal.value = strtod(text, NULL);
        free(text);
        return node;
    }

    if (match_token(p, TOKEN_STRING)) {
        ASTNode* node = ast_new_node(NODE_STRING_LITERAL,
            p->previous.line, p->previous.column);
        node->as.string_literal.value = token_string_content(p->previous);
        node->as.string_literal.length = p->previous.length - 2;
        return node;
    }

    if (match_token(p, TOKEN_TRUE)) {
        ASTNode* node = ast_new_node(NODE_BOOL_LITERAL,
            p->previous.line, p->previous.column);
        node->as.bool_literal.value = true;
        return node;
    }

    if (match_token(p, TOKEN_FALSE)) {
        ASTNode* node = ast_new_node(NODE_BOOL_LITERAL,
            p->previous.line, p->previous.column);
        node->as.bool_literal.value = false;
        return node;
    }

    if (match_token(p, TOKEN_NOTHING)) {
        return ast_new_node(NODE_NOTHING_LITERAL,
            p->previous.line, p->previous.column);
    }

    if (match_token(p, TOKEN_IDENTIFIER) || match_token(p, TOKEN_SELF)) {
        ASTNode* node = ast_new_node(NODE_IDENTIFIER,
            p->previous.line, p->previous.column);
        node->as.identifier.name = token_text(p->previous);
        return node;
    }

    if (match_token(p, TOKEN_LPAREN)) {

        Token saved = p->current;
        bool is_lambda = false;

        if (check(p, TOKEN_RPAREN)) {
            Parser saved_zero = *p;
            advance_token(p);
            if (check(p, TOKEN_FAT_ARROW)) {
                is_lambda = true;
            }

            *p = saved_zero;
        }

        if (!is_lambda && (check(p, TOKEN_IDENTIFIER) || check(p, TOKEN_SELF))) {

            Parser saved_p = *p;
            bool valid = true;

            do {
                if (!match_token(p, TOKEN_IDENTIFIER) && !match_token(p, TOKEN_SELF)) {
                    valid = false; break;
                }

                if (match_token(p, TOKEN_COLON)) {
                    if (!match_token(p, TOKEN_IDENTIFIER)) { valid = false; break; }
                }

                if (match_token(p, TOKEN_BE)) {
                    parse_expression(p);
                }
            } while (valid && match_token(p, TOKEN_COMMA));

            if (valid && check(p, TOKEN_RPAREN)) {
                advance_token(p);
                if (check(p, TOKEN_FAT_ARROW)) {
                    is_lambda = true;
                }
            }

            *p = saved_p;
        }

        if (is_lambda) {
            Token lp = p->previous;
            ParamList params;
            paramlist_init(&params);
            if (!check(p, TOKEN_RPAREN)) {
                do {
                    Token pname = consume(p, TOKEN_IDENTIFIER, "Expected lambda param");
                    char* ptype = NULL;
                    if (match_token(p, TOKEN_COLON)) {
                        Token pt = consume(p, TOKEN_IDENTIFIER, "Expected param type");
                        ptype = token_text(pt);
                    }
                    paramlist_add(&params, token_text(pname), ptype);

                    if (match_token(p, TOKEN_BE)) {
                        params.items[params.count - 1].default_value = parse_expression(p);
                    }
                    if (ptype) free(ptype);
                } while (match_token(p, TOKEN_COMMA));
            }
            consume(p, TOKEN_RPAREN, "Expected ')' after lambda params");
            consume(p, TOKEN_FAT_ARROW, "Expected '=>' in lambda");

            ASTNode* body;

            if (match_token(p, TOKEN_COLON)) {
                body = parse_block(p);
            } else {
                body = parse_expression(p);
            }

            ASTNode* node = ast_new_node(NODE_LAMBDA, lp.line, lp.column);
            node->as.lambda.params = params;
            node->as.lambda.body = body;
            return node;
        }

        ASTNode* expr = parse_expression(p);
        consume(p, TOKEN_RPAREN, "Expected ')' after expression");
        return expr;
    }

    if (match_token(p, TOKEN_LBRACKET)) {
        Token bracket = p->previous;

        if (check(p, TOKEN_RBRACKET)) {
            advance_token(p);
            ASTNode* node = ast_new_node(NODE_ARRAY_LITERAL, bracket.line, bracket.column);
            nodelist_init(&node->as.array_literal.elements);
            return node;
        }

        ASTNode* first = parse_expression(p);

        if (check(p, TOKEN_FOR)) {
            advance_token(p);
            consume(p, TOKEN_EACH, "Expected 'each' after 'for' in comprehension");
            Token var = consume(p, TOKEN_IDENTIFIER, "Expected variable in comprehension");
            consume(p, TOKEN_IN, "Expected 'in' in comprehension");
            ASTNode* iterable = parse_expression(p);

            ASTNode* condition = NULL;
            if (match_token(p, TOKEN_IF)) {
                condition = parse_expression(p);
            }
            consume(p, TOKEN_RBRACKET, "Expected ']' after comprehension");

            ASTNode* node = ast_new_node(NODE_LIST_COMP, bracket.line, bracket.column);
            node->as.list_comp.expr = first;
            node->as.list_comp.var_name = token_text(var);
            node->as.list_comp.iterable = iterable;
            node->as.list_comp.condition = condition;
            return node;
        }

        ASTNode* node = ast_new_node(NODE_ARRAY_LITERAL, bracket.line, bracket.column);
        nodelist_init(&node->as.array_literal.elements);
        nodelist_add(&node->as.array_literal.elements, first);

        while (match_token(p, TOKEN_COMMA)) {
            skip_newlines(p);
            if (check(p, TOKEN_RBRACKET)) break;
            nodelist_add(&node->as.array_literal.elements, parse_expression(p));
            skip_newlines(p);
        }
        consume(p, TOKEN_RBRACKET, "Expected ']' after array");
        return node;
    }

    if (match_token(p, TOKEN_LBRACE)) {
        ASTNode* node = ast_new_node(NODE_MAP_LITERAL,
            p->previous.line, p->previous.column);
        node->as.map_literal.entries = NULL;
        node->as.map_literal.count = 0;
        node->as.map_literal.capacity = 0;

        if (!check(p, TOKEN_RBRACE)) {
            do {
                skip_newlines(p);

                if (node->as.map_literal.count >= node->as.map_literal.capacity) {
                    node->as.map_literal.capacity =
                        node->as.map_literal.capacity == 0 ? 4 :
                        node->as.map_literal.capacity * 2;
                    node->as.map_literal.entries = (MapEntry*)realloc(
                        node->as.map_literal.entries,
                        sizeof(MapEntry) * node->as.map_literal.capacity);
                }
                MapEntry* entry = &node->as.map_literal.entries[node->as.map_literal.count++];
                entry->key = parse_expression(p);
                consume(p, TOKEN_COLON, "Expected ':' in map entry");
                entry->value = parse_expression(p);
                skip_newlines(p);
            } while (match_token(p, TOKEN_COMMA));
        }
        consume(p, TOKEN_RBRACE, "Expected '}' after map");
        return node;
    }

    if (match_token(p, TOKEN_NOT) || match_token(p, TOKEN_MINUS)) {
        Token op = p->previous;
        ASTNode* node = ast_new_node(NODE_UNARY_OP, op.line, op.column);
        node->as.unary.op = op.type;
        node->as.unary.operand = parse_primary(p);
        return node;
    }

    fprintf(stderr, "Error [line %d]: Unexpected token '%.*s'\n",
        p->current.line, p->current.length, p->current.start);
    p->had_error = true;
    advance_token(p);
    return ast_new_node(NODE_NOTHING_LITERAL, p->previous.line, p->previous.column);
}

static ASTNode* parse_postfix(Parser* p) {
    ASTNode* expr = parse_primary(p);

    for (;;) {

        if (match_token(p, TOKEN_LPAREN)) {
            ASTNode* call = ast_new_node(NODE_CALL, expr->line, expr->column);
            call->as.call.callee = expr;
            nodelist_init(&call->as.call.args);

            if (!check(p, TOKEN_RPAREN)) {
                do {
                    nodelist_add(&call->as.call.args, parse_expression(p));
                } while (match_token(p, TOKEN_COMMA));
            }
            consume(p, TOKEN_RPAREN, "Expected ')' after arguments");
            expr = call;
        }

        else if (match_token(p, TOKEN_LBRACKET)) {
            ASTNode* idx = ast_new_node(NODE_INDEX, expr->line, expr->column);
            idx->as.index_access.object = expr;
            idx->as.index_access.index = parse_expression(p);
            consume(p, TOKEN_RBRACKET, "Expected ']' after index");
            expr = idx;
        }

        else if (match_token(p, TOKEN_DOT)) {
            Token field = consume(p, TOKEN_IDENTIFIER, "Expected field name after '.'");
            ASTNode* access = ast_new_node(NODE_FIELD_ACCESS, expr->line, expr->column);
            access->as.field_access.object = expr;
            access->as.field_access.field = token_text(field);
            expr = access;
        }
        else break;
    }

    return expr;
}

static int get_precedence(TokenType type) {
    switch (type) {
        case TOKEN_OR:                                      return 1;
        case TOKEN_AND:                                     return 2;
        case TOKEN_EQ: case TOKEN_NEQ:
        case TOKEN_IS: case TOKEN_IS_NOT:
        case TOKEN_EXISTS: case TOKEN_NOT_EXISTS:           return 3;
        case TOKEN_LT:  case TOKEN_GT:
        case TOKEN_LTE: case TOKEN_GTE:
        case TOKEN_IS_GREATER: case TOKEN_IS_LESS:
        case TOKEN_IS_AT_LEAST: case TOKEN_IS_AT_MOST:      return 4;
        case TOKEN_PLUS: case TOKEN_MINUS:                  return 5;
        case TOKEN_STAR: case TOKEN_SLASH:
        case TOKEN_PERCENT:                                 return 6;
        case TOKEN_POWER:                                   return 7;
        default:                                            return -1;
    }
}

static ASTNode* parse_binary(Parser* p, int min_prec) {
    ASTNode* left = parse_postfix(p);

    for (;;) {
        int prec = get_precedence(p->current.type);
        if (prec < min_prec) break;

        Token op = p->current;
        advance_token(p);

        int next_prec = (op.type == TOKEN_POWER) ? prec : prec + 1;
        ASTNode* right = parse_binary(p, next_prec);

        ASTNode* bin = ast_new_node(NODE_BINARY_OP, op.line, op.column);
        bin->as.binary.left = left;
        bin->as.binary.right = right;
        bin->as.binary.op = op.type;
        left = bin;
    }

    return left;
}

static ASTNode* parse_expression(Parser* p) {
    return parse_binary(p, 0);
}

static ASTNode* parse_block(Parser* p) {
    skip_newlines(p);
    consume(p, TOKEN_INDENT, "Expected indented block");

    ASTNode* block = ast_new_node(NODE_BLOCK, p->previous.line, p->previous.column);
    nodelist_init(&block->as.block.statements);

    skip_newlines(p);
    while (!check(p, TOKEN_DEDENT) && !check(p, TOKEN_EOF)) {
        ASTNode* stmt = parse_statement(p);
        if (stmt) nodelist_add(&block->as.block.statements, stmt);
        skip_newlines(p);
    }

    if (check(p, TOKEN_DEDENT)) advance_token(p);
    return block;
}

static ASTNode* parse_let_or_const(Parser* p, bool is_const) {
    Token kw = p->previous;
    Token name = consume(p, TOKEN_IDENTIFIER, "Expected variable name");

    char* type_name = NULL;
    if (match_token(p, TOKEN_COLON)) {
        Token type_tok = consume(p, TOKEN_IDENTIFIER, "Expected type name");
        type_name = token_text(type_tok);
    }

    consume(p, TOKEN_BE, "Expected 'be' after variable name");
    ASTNode* value = parse_expression(p);

    ASTNode* node = ast_new_node(is_const ? NODE_CONST_DECL : NODE_LET_DECL,
        kw.line, kw.column);
    node->as.var_decl.name = token_text(name);
    node->as.var_decl.type_name = type_name;
    node->as.var_decl.value = value;
    return node;
}

static ASTNode* parse_display(Parser* p) {
    Token kw = p->previous;
    ASTNode* value = parse_expression(p);
    ASTNode* node = ast_new_node(NODE_DISPLAY, kw.line, kw.column);
    node->as.display.value = value;
    return node;
}

static ASTNode* parse_if(Parser* p) {
    Token kw = p->previous;
    ASTNode* condition = parse_expression(p);
    consume(p, TOKEN_COLON, "Expected ':' after if condition");
    ASTNode* then_block = parse_block(p);

    ASTNode* else_block = NULL;
    skip_newlines(p);
    if (match_token(p, TOKEN_ELIF)) {
        else_block = parse_if(p);
    } else if (match_token(p, TOKEN_ELSE)) {
        consume(p, TOKEN_COLON, "Expected ':' after else");
        else_block = parse_block(p);
    }

    ASTNode* node = ast_new_node(NODE_IF, kw.line, kw.column);
    node->as.if_stmt.condition = condition;
    node->as.if_stmt.then_block = then_block;
    node->as.if_stmt.else_block = else_block;
    return node;
}

static ASTNode* parse_while(Parser* p) {
    Token kw = p->previous;
    ASTNode* condition = parse_expression(p);
    consume(p, TOKEN_COLON, "Expected ':' after while condition");
    ASTNode* body = parse_block(p);

    ASTNode* node = ast_new_node(NODE_WHILE, kw.line, kw.column);
    node->as.while_stmt.condition = condition;
    node->as.while_stmt.body = body;
    return node;
}

static ASTNode* parse_for(Parser* p) {
    Token kw = p->previous;

    if (match_token(p, TOKEN_EACH)) {
        Token var = consume(p, TOKEN_IDENTIFIER, "Expected variable name after 'each'");
        consume(p, TOKEN_IN, "Expected 'in' after variable name");
        ASTNode* iterable = parse_expression(p);
        consume(p, TOKEN_COLON, "Expected ':' after for-each");
        ASTNode* body = parse_block(p);

        ASTNode* node = ast_new_node(NODE_FOR_EACH, kw.line, kw.column);
        node->as.for_each.var_name = token_text(var);
        node->as.for_each.iterable = iterable;
        node->as.for_each.body = body;
        return node;
    }

    Token var = consume(p, TOKEN_IDENTIFIER, "Expected variable name");
    consume(p, TOKEN_IN, "Expected 'in' after variable name");
    ASTNode* start = parse_expression(p);
    consume(p, TOKEN_TO, "Expected 'to' in for range");
    ASTNode* end = parse_expression(p);

    ASTNode* step = NULL;
    if (match_token(p, TOKEN_BY)) {
        step = parse_expression(p);
    }

    consume(p, TOKEN_COLON, "Expected ':' after for range");
    ASTNode* body = parse_block(p);

    ASTNode* node = ast_new_node(NODE_FOR_RANGE, kw.line, kw.column);
    node->as.for_range.var_name = token_text(var);
    node->as.for_range.start = start;
    node->as.for_range.end = end;
    node->as.for_range.step = step;
    node->as.for_range.body = body;
    return node;
}

static ASTNode* parse_repeat(Parser* p) {
    Token kw = p->previous;
    ASTNode* count = parse_expression(p);
    consume(p, TOKEN_TIMES, "Expected 'times' after repeat count");
    consume(p, TOKEN_COLON, "Expected ':' after 'times'");
    ASTNode* body = parse_block(p);

    ASTNode* node = ast_new_node(NODE_REPEAT, kw.line, kw.column);
    node->as.repeat.count = count;
    node->as.repeat.body = body;
    return node;
}

static ASTNode* parse_fn(Parser* p) {
    Token kw = p->previous;
    Token name = consume(p, TOKEN_IDENTIFIER, "Expected function name");
    consume(p, TOKEN_LPAREN, "Expected '(' after function name");

    ParamList params;
    paramlist_init(&params);

    if (!check(p, TOKEN_RPAREN)) {
        do {
            Token pname = consume(p, TOKEN_IDENTIFIER, "Expected parameter name");
            char* ptype = NULL;
            if (match_token(p, TOKEN_COLON)) {
                Token pt = consume(p, TOKEN_IDENTIFIER, "Expected parameter type");
                ptype = token_text(pt);
            }
            paramlist_add(&params, token_text(pname), ptype);

            if (match_token(p, TOKEN_BE)) {
                params.items[params.count - 1].default_value = parse_expression(p);
            }
            if (ptype) free(ptype);
        } while (match_token(p, TOKEN_COMMA));
    }
    consume(p, TOKEN_RPAREN, "Expected ')' after parameters");

    char* return_type = NULL;
    if (match_token(p, TOKEN_ARROW)) {
        Token rt = consume(p, TOKEN_IDENTIFIER, "Expected return type");
        return_type = token_text(rt);
    }

    consume(p, TOKEN_COLON, "Expected ':' after function signature");
    ASTNode* body = parse_block(p);

    ASTNode* node = ast_new_node(NODE_FN_DECL, kw.line, kw.column);
    node->as.fn_decl.name = token_text(name);
    node->as.fn_decl.params = params;
    node->as.fn_decl.return_type = return_type;
    node->as.fn_decl.body = body;
    return node;
}

static ASTNode* parse_give_back(Parser* p) {
    Token kw = p->previous;
    ASTNode* value = NULL;
    if (!check(p, TOKEN_NEWLINE) && !check(p, TOKEN_DEDENT) && !check(p, TOKEN_EOF)) {
        value = parse_expression(p);
    }
    ASTNode* node = ast_new_node(NODE_GIVE_BACK, kw.line, kw.column);
    node->as.give_back.value = value;
    return node;
}

static ASTNode* parse_struct(Parser* p) {
    Token kw = p->previous;
    Token name = consume(p, TOKEN_IDENTIFIER, "Expected struct name");

    char* parent_name = NULL;
    if (match_token(p, TOKEN_EXTENDS)) {
        Token parent = consume(p, TOKEN_IDENTIFIER, "Expected parent struct name");
        parent_name = token_text(parent);
    }

    consume(p, TOKEN_COLON, "Expected ':' after struct name");

    ASTNode* node = ast_new_node(NODE_STRUCT_DECL, kw.line, kw.column);
    node->as.struct_decl.name = token_text(name);
    node->as.struct_decl.parent_name = parent_name;
    node->as.struct_decl.fields = NULL;
    node->as.struct_decl.field_count = 0;
    node->as.struct_decl.field_capacity = 0;
    node->as.struct_decl.methods = NULL;
    node->as.struct_decl.method_count = 0;
    node->as.struct_decl.method_capacity = 0;

    skip_newlines(p);
    consume(p, TOKEN_INDENT, "Expected indented struct body");
    skip_newlines(p);

    while (!check(p, TOKEN_EOF)) {

        if (check(p, TOKEN_DEDENT)) {

            Token saved_current = p->current;
            advance_token(p);
            skip_newlines(p);
            if (check(p, TOKEN_INDENT)) {
                advance_token(p);
                skip_newlines(p);
                continue;
            }

            break;
        }

        if (match_token(p, TOKEN_HAS)) {
            Token fname = consume(p, TOKEN_IDENTIFIER, "Expected field name");
            char* ftype_str = NULL;
            if (match_token(p, TOKEN_COLON)) {
                Token ftype = consume(p, TOKEN_IDENTIFIER, "Expected field type");
                ftype_str = token_text(ftype);
            }

            if (node->as.struct_decl.field_count >= node->as.struct_decl.field_capacity) {
                node->as.struct_decl.field_capacity =
                    node->as.struct_decl.field_capacity == 0 ? 4 :
                    node->as.struct_decl.field_capacity * 2;
                node->as.struct_decl.fields = (StructField*)realloc(
                    node->as.struct_decl.fields,
                    sizeof(StructField) * node->as.struct_decl.field_capacity);
            }
            StructField* f = &node->as.struct_decl.fields[node->as.struct_decl.field_count++];
            f->name = token_text(fname);
            f->type_name = ftype_str;
        }

        else if (match_token(p, TOKEN_CAN)) {
            Token mname = consume(p, TOKEN_IDENTIFIER, "Expected method name");
            consume(p, TOKEN_LPAREN, "Expected '(' after method name");

            ParamList mparams;
            paramlist_init(&mparams);
            if (!check(p, TOKEN_RPAREN)) {
                do {
                    Token pn;
                    if (check(p, TOKEN_SELF)) {
                        pn = p->current;
                        advance_token(p);
                    } else {
                        pn = consume(p, TOKEN_IDENTIFIER, "Expected param name");
                    }
                    char* pt = NULL;
                    if (match_token(p, TOKEN_COLON)) {
                        Token ptt = consume(p, TOKEN_IDENTIFIER, "Expected param type");
                        pt = token_text(ptt);
                    }
                    paramlist_add(&mparams, token_text(pn), pt);
                    if (pt) free(pt);
                } while (match_token(p, TOKEN_COMMA));
            }
            consume(p, TOKEN_RPAREN, "Expected ')' after method params");

            char* ret_type = NULL;
            if (match_token(p, TOKEN_ARROW)) {
                Token rt = consume(p, TOKEN_IDENTIFIER, "Expected return type");
                ret_type = token_text(rt);
            }

            consume(p, TOKEN_COLON, "Expected ':' after method signature");
            ASTNode* mbody = parse_block(p);

            if (node->as.struct_decl.method_count >= node->as.struct_decl.method_capacity) {
                node->as.struct_decl.method_capacity =
                    node->as.struct_decl.method_capacity == 0 ? 4 :
                    node->as.struct_decl.method_capacity * 2;
                node->as.struct_decl.methods = (StructMethod*)realloc(
                    node->as.struct_decl.methods,
                    sizeof(StructMethod) * node->as.struct_decl.method_capacity);
            }
            StructMethod* m = &node->as.struct_decl.methods[node->as.struct_decl.method_count++];
            m->name = token_text(mname);
            m->params = mparams;
            m->return_type = ret_type;
            m->body = mbody;
        }
        else {

            advance_token(p);
        }

        skip_newlines(p);
    }

    return node;
}

static ASTNode* parse_match(Parser* p) {
    Token kw = p->previous;
    ASTNode* expr = parse_expression(p);
    consume(p, TOKEN_COLON, "Expected ':' after match expression");

    ASTNode* node = ast_new_node(NODE_MATCH, kw.line, kw.column);
    node->as.match_stmt.expr = expr;
    node->as.match_stmt.arms.items = NULL;
    node->as.match_stmt.arms.count = 0;
    node->as.match_stmt.arms.capacity = 0;

    skip_newlines(p);
    consume(p, TOKEN_INDENT, "Expected indented match body");
    skip_newlines(p);

    while (!check(p, TOKEN_DEDENT) && !check(p, TOKEN_EOF)) {
        ASTNode* pattern = parse_expression(p);
        consume(p, TOKEN_FAT_ARROW, "Expected '=>' after match pattern");
        ASTNode* body = parse_statement(p);

        if (node->as.match_stmt.arms.count >= node->as.match_stmt.arms.capacity) {
            node->as.match_stmt.arms.capacity =
                node->as.match_stmt.arms.capacity == 0 ? 4 :
                node->as.match_stmt.arms.capacity * 2;
            node->as.match_stmt.arms.items = (MatchArm*)realloc(
                node->as.match_stmt.arms.items,
                sizeof(MatchArm) * node->as.match_stmt.arms.capacity);
        }
        MatchArm* arm = &node->as.match_stmt.arms.items[node->as.match_stmt.arms.count++];
        arm->pattern = pattern;
        arm->body = body;
        skip_newlines(p);
    }

    if (check(p, TOKEN_DEDENT)) advance_token(p);
    return node;
}

static ASTNode* parse_use(Parser* p) {
    Token kw = p->previous;
    Token mod = consume(p, TOKEN_IDENTIFIER, "Expected module name");
    ASTNode* node = ast_new_node(NODE_USE, kw.line, kw.column);
    node->as.use_stmt.module_name = token_text(mod);
    return node;
}

static ASTNode* parse_from_use(Parser* p) {
    Token kw = p->previous;
    Token mod = consume(p, TOKEN_IDENTIFIER, "Expected module name");
    consume(p, TOKEN_USE, "Expected 'use' after module name");
    Token name = consume(p, TOKEN_IDENTIFIER, "Expected import name");

    ASTNode* node = ast_new_node(NODE_FROM_USE, kw.line, kw.column);
    node->as.from_use.module_name = token_text(mod);
    node->as.from_use.import_name = token_text(name);
    return node;
}

static ASTNode* parse_attempt(Parser* p) {
    Token kw = p->previous;
    consume(p, TOKEN_COLON, "Expected ':' after 'attempt'");
    ASTNode* try_block = parse_block(p);

    skip_newlines(p);
    consume(p, TOKEN_OTHERWISE, "Expected 'otherwise' after attempt block");

    char* error_name = NULL;
    if (check(p, TOKEN_IDENTIFIER)) {
        error_name = token_text(p->current);
        advance_token(p);
    }
    consume(p, TOKEN_COLON, "Expected ':' after 'otherwise'");
    ASTNode* catch_block = parse_block(p);

    ASTNode* node = ast_new_node(NODE_ATTEMPT, kw.line, kw.column);
    node->as.attempt.try_block = try_block;
    node->as.attempt.error_name = error_name;
    node->as.attempt.catch_block = catch_block;
    return node;
}

static ASTNode* parse_expression_or_assign(Parser* p) {
    ASTNode* expr = parse_expression(p);

    if (check(p, TOKEN_BE) || check(p, TOKEN_ASSIGN) ||
        check(p, TOKEN_PLUS_ASSIGN) || check(p, TOKEN_MINUS_ASSIGN) ||
        check(p, TOKEN_STAR_ASSIGN) || check(p, TOKEN_SLASH_ASSIGN)) {

        Token op = p->current;
        advance_token(p);
        ASTNode* value = parse_expression(p);

        ASTNode* node = ast_new_node(NODE_ASSIGN, op.line, op.column);
        node->as.assign.target = expr;
        node->as.assign.value = value;
        node->as.assign.op = op.type;
        return node;
    }

    return expr;
}

static ASTNode* parse_statement(Parser* p) {
    skip_newlines(p);

    if (check(p, TOKEN_EOF)) return NULL;

    if (match_token(p, TOKEN_LET))     return parse_let_or_const(p, false);
    if (match_token(p, TOKEN_CONST))   return parse_let_or_const(p, true);

    if (match_token(p, TOKEN_DISPLAY)) return parse_display(p);

    if (match_token(p, TOKEN_IF))      return parse_if(p);

    if (match_token(p, TOKEN_WHILE))   return parse_while(p);

    if (match_token(p, TOKEN_FOR))     return parse_for(p);

    if (match_token(p, TOKEN_REPEAT))  return parse_repeat(p);

    if (match_token(p, TOKEN_FN))      return parse_fn(p);

    if (match_token(p, TOKEN_GIVE_BACK)) return parse_give_back(p);

    if (match_token(p, TOKEN_BREAK)) {
        return ast_new_node(NODE_BREAK, p->previous.line, p->previous.column);
    }

    if (match_token(p, TOKEN_SKIP)) {
        return ast_new_node(NODE_SKIP, p->previous.line, p->previous.column);
    }

    if (match_token(p, TOKEN_PASS)) {
        return ast_new_node(NODE_PASS, p->previous.line, p->previous.column);
    }

    if (match_token(p, TOKEN_STRUCT))  return parse_struct(p);

    if (match_token(p, TOKEN_MATCH))   return parse_match(p);

    if (match_token(p, TOKEN_USE))     return parse_use(p);

    if (match_token(p, TOKEN_FROM))    return parse_from_use(p);

    if (match_token(p, TOKEN_ATTEMPT)) return parse_attempt(p);

    ASTNode* expr = parse_expression_or_assign(p);
    ASTNode* stmt = ast_new_node(NODE_EXPR_STMT, expr->line, expr->column);
    stmt->as.expr_stmt.expr = expr;
    return stmt;
}

void parser_init(Parser* parser, const char* source) {
    lexer_init(&parser->lexer, source);
    parser->had_error = false;
    parser->panic_mode = false;
    advance_token(parser);
}

ASTNode* parser_parse(Parser* parser) {
    ASTNode* program = ast_new_node(NODE_PROGRAM, 1, 1);
    nodelist_init(&program->as.program.statements);

    skip_newlines(parser);
    while (!check(parser, TOKEN_EOF)) {
        ASTNode* stmt = parse_statement(parser);
        if (stmt) {
            nodelist_add(&program->as.program.statements, stmt);
        }
        skip_newlines(parser);
    }

    return program;
}
