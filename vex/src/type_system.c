/* ═══════════════════════════════════════════════════════════════════════════════
 *  VEXIUM TYPE SYSTEM - Implementation
 *  Gradual typing with inference and generics
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define _POSIX_C_SOURCE 200809L  /* For strdup on POSIX systems */

#include "type_system.h"
#include "ast.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>  /* For isalnum, isspace */

/* strdup is not C99, provide fallback for Windows */
#ifdef _WIN32
static char* type_strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* copy = (char*)malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}
#define strdup type_strdup
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 *  BUILT-IN TYPE SINGLETONS
 * ═══════════════════════════════════════════════════════════════════════════════ */

Type* TYPE_NOTHING_SINGLETON = NULL;
Type* TYPE_DYNAMIC_SINGLETON = NULL;
Type* TYPE_INT_SINGLETON = NULL;
Type* TYPE_FLOAT_SINGLETON = NULL;
Type* TYPE_BOOL_SINGLETON = NULL;
Type* TYPE_STRING_SINGLETON = NULL;

void type_system_init(void) {
    TYPE_NOTHING_SINGLETON = type_create_nothing();
    TYPE_DYNAMIC_SINGLETON = type_create_dynamic();
    TYPE_INT_SINGLETON = type_create_int();
    TYPE_FLOAT_SINGLETON = type_create_float();
    TYPE_BOOL_SINGLETON = type_create_bool();
    TYPE_STRING_SINGLETON = type_create_string();
}

void type_system_shutdown(void) {
    /* Singletons are freed at program exit */
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TYPE CREATION
 * ═══════════════════════════════════════════════════════════════════════════════ */

static Type* type_alloc(void) {
    Type* t = (Type*)calloc(1, sizeof(Type));
    if (!t) {
        fprintf(stderr, "[Type System] Out of memory\n");
        exit(1);
    }
    return t;
}

Type* type_create_nothing(void) {
    Type* t = type_alloc();
    t->kind = TYPE_NOTHING;
    return t;
}

Type* type_create_dynamic(void) {
    Type* t = type_alloc();
    t->kind = TYPE_DYNAMIC;
    return t;
}

Type* type_create_int(void) {
    Type* t = type_alloc();
    t->kind = TYPE_INT;
    return t;
}

Type* type_create_float(void) {
    Type* t = type_alloc();
    t->kind = TYPE_FLOAT;
    return t;
}

Type* type_create_bool(void) {
    Type* t = type_alloc();
    t->kind = TYPE_BOOL;
    return t;
}

Type* type_create_string(void) {
    Type* t = type_alloc();
    t->kind = TYPE_STRING;
    return t;
}

Type* type_create_array(Type* element) {
    Type* t = type_alloc();
    t->kind = TYPE_ARRAY;
    t->element_type = element;
    return t;
}

Type* type_create_map(Type* key, Type* value) {
    Type* t = type_alloc();
    t->kind = TYPE_MAP;
    t->key_type = key;
    t->value_type = value;
    return t;
}

Type* type_create_option(Type* inner) {
    Type* t = type_alloc();
    t->kind = TYPE_OPTION;
    t->element_type = inner;
    return t;
}

Type* type_create_function(Type** params, int count, Type* ret) {
    Type* t = type_alloc();
    t->kind = TYPE_FUNCTION;
    t->param_count = count;
    t->param_types = params;
    t->return_type = ret;
    return t;
}

Type* type_create_struct(const char* name) {
    Type* t = type_alloc();
    t->kind = TYPE_STRUCT;
    t->struct_name = strdup(name);
    if (!t->struct_name) {
        fprintf(stderr, "[Type System] Out of memory in type_create_struct\n");
        free(t);
        exit(1);
    }
    return t;
}

Type* type_create_tuple(Type** types, int count) {
    Type* t = type_alloc();
    t->kind = TYPE_TUPLE;
    t->tuple_types = types;
    t->tuple_count = count;
    return t;
}

Type* type_create_union(Type** types, int count) {
    Type* t = type_alloc();
    t->kind = TYPE_UNION;
    t->union_types = types;
    t->union_count = count;
    return t;
}

Type* type_create_variable(const char* name, int id) {
    Type* t = type_alloc();
    t->kind = TYPE_VARIABLE;
    t->var_name = strdup(name);
    if (!t->var_name) {
        fprintf(stderr, "[Type System] Out of memory in type_create_variable\n");
        free(t);
        exit(1);
    }
    t->id = id;
    return t;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TYPE CONTEXT
 * ═══════════════════════════════════════════════════════════════════════════════ */

void type_context_init(TypeContext* ctx, bool strict) {
    memset(ctx, 0, sizeof(TypeContext));
    ctx->strict_mode = strict;
    ctx->next_type_var_id = 1;
}

void type_context_free(TypeContext* ctx) {
    /* Free bindings */
    for (int i = 0; i < ctx->bindings.count; i++) {
        free(ctx->bindings.names[i]);
    }
    for (int i = 0; i < ctx->generic_bindings.count; i++) {
        free(ctx->generic_bindings.names[i]);
    }
}

void type_context_bind(TypeContext* ctx, const char* name, Type* type) {
    if (ctx->bindings.count >= TYPE_CONTEXT_MAX_BINDINGS) {
        fprintf(stderr, "[Type System] Too many bindings\n");
        return;
    }
    char* name_copy = strdup(name);
    if (!name_copy) {
        fprintf(stderr, "[Type System] Out of memory in type_context_bind\n");
        ctx->had_error = true;
        return;
    }
    Type* stored_type = type_apply(type, ctx);
    if (!stored_type) {
        free(name_copy);
        fprintf(stderr, "[Type System] Out of memory cloning bound type\n");
        ctx->had_error = true;
        return;
    }
    ctx->bindings.names[ctx->bindings.count] = name_copy;
    ctx->bindings.types[ctx->bindings.count] = stored_type;
    ctx->bindings.count++;
}

Type* type_context_lookup(TypeContext* ctx, const char* name) {
    for (int i = ctx->bindings.count - 1; i >= 0; i--) {
        if (strcmp(ctx->bindings.names[i], name) == 0) {
            return ctx->bindings.types[i];
        }
    }
    return NULL;
}

Type* type_fresh_variable(TypeContext* ctx, const char* prefix) {
    char name[64];
    snprintf(name, sizeof(name), "%s%d", prefix, ctx->next_type_var_id);
    Type* var = type_create_variable(name, ctx->next_type_var_id++);
    return var;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TYPE EQUALITY
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool type_equals(Type* a, Type* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;
    
    switch (a->kind) {
        case TYPE_DYNAMIC:
        case TYPE_NOTHING:
        case TYPE_INT:
        case TYPE_FLOAT:
        case TYPE_BOOL:
        case TYPE_STRING:
            return true;
            
        case TYPE_ARRAY:
        case TYPE_OPTION:
            return type_equals(a->element_type, b->element_type);
            
        case TYPE_MAP:
            return type_equals(a->key_type, b->key_type) &&
                   type_equals(a->value_type, b->value_type);
            
        case TYPE_FUNCTION:
            if (a->param_count != b->param_count) return false;
            for (int i = 0; i < a->param_count; i++) {
                if (!type_equals(a->param_types[i], b->param_types[i]))
                    return false;
            }
            return type_equals(a->return_type, b->return_type);
            
        case TYPE_STRUCT:
            return strcmp(a->struct_name, b->struct_name) == 0;
            
        case TYPE_TUPLE:
            if (a->tuple_count != b->tuple_count) return false;
            for (int i = 0; i < a->tuple_count; i++) {
                if (!type_equals(a->tuple_types[i], b->tuple_types[i]))
                    return false;
            }
            return true;
            
        case TYPE_UNION:
            if (a->union_count != b->union_count) return false;
            for (int i = 0; i < a->union_count; i++) {
                if (!type_equals(a->union_types[i], b->union_types[i]))
                    return false;
            }
            return true;
            
        case TYPE_VARIABLE:
            return a->id == b->id;
            
        default:
            return false;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TYPE INFERENCE FROM AST
 * ═══════════════════════════════════════════════════════════════════════════════ */

Type* infer_literal(ASTNode* node) {
    switch (node->type) {
        case NODE_INT_LITERAL:
            return TYPE_INT_SINGLETON;
        case NODE_FLOAT_LITERAL:
            return TYPE_FLOAT_SINGLETON;
        case NODE_BOOL_LITERAL:
            return TYPE_BOOL_SINGLETON;
        case NODE_STRING_LITERAL:
            return TYPE_STRING_SINGLETON;
        case NODE_NOTHING_LITERAL:
            return TYPE_NOTHING_SINGLETON;
        default:
            return TYPE_DYNAMIC_SINGLETON;
    }
}

Type* infer_array(ASTNode* node, TypeContext* ctx) {
    int count = node->as.array_literal.elements.count;
    if (count == 0) {
        return type_create_array(TYPE_DYNAMIC_SINGLETON);
    }
    
    /* Infer from first element */
    Type* elem_type = infer_from_node(node->as.array_literal.elements.items[0], ctx);
    return type_create_array(elem_type);
}

Type* infer_map(ASTNode* node, TypeContext* ctx) {
    int count = node->as.map_literal.count;
    if (count == 0) {
        return type_create_map(TYPE_DYNAMIC_SINGLETON, TYPE_DYNAMIC_SINGLETON);
    }
    
    /* Infer from first entry */
    Type* key_type = infer_from_node(node->as.map_literal.entries[0].key, ctx);
    Type* val_type = infer_from_node(node->as.map_literal.entries[0].value, ctx);
    return type_create_map(key_type, val_type);
}

Type* infer_binary(ASTNode* node, TypeContext* ctx) {
    VexiumTokenType op = node->as.binary.op;
    Type* left = infer_from_node(node->as.binary.left, ctx);
    Type* right = infer_from_node(node->as.binary.right, ctx);

    if (!left || !right) return TYPE_DYNAMIC_SINGLETON;

    if (left->kind == TYPE_DYNAMIC || right->kind == TYPE_DYNAMIC) {
        return TYPE_DYNAMIC_SINGLETON;
    }
    
    switch (op) {
        /* Arithmetic */
        case TOKEN_PLUS:
            if (left->kind == TYPE_STRING && right->kind == TYPE_STRING) {
                return TYPE_STRING_SINGLETON;
            }
            if (left->kind == TYPE_ARRAY && right->kind == TYPE_ARRAY) {
                if (type_is_assignable(left, right)) return left;
                if (type_is_assignable(right, left)) return right;
                return type_create_array(TYPE_DYNAMIC_SINGLETON);
            }
            if ((left->kind == TYPE_INT || left->kind == TYPE_FLOAT) &&
                (right->kind == TYPE_INT || right->kind == TYPE_FLOAT)) {
                if (left->kind == TYPE_INT && right->kind == TYPE_INT) {
                    return TYPE_INT_SINGLETON;
                }
                return TYPE_FLOAT_SINGLETON;
            }
            ctx->had_error = true;
            snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                     "Unsupported '+' operands: %s and %s",
                     type_to_string(left), type_to_string(right));
            return TYPE_DYNAMIC_SINGLETON;

        case TOKEN_MINUS:
            if ((left->kind == TYPE_INT || left->kind == TYPE_FLOAT) &&
                (right->kind == TYPE_INT || right->kind == TYPE_FLOAT)) {
                if (left->kind == TYPE_INT && right->kind == TYPE_INT) {
                    return TYPE_INT_SINGLETON;
                }
                return TYPE_FLOAT_SINGLETON;
            }
            ctx->had_error = true;
            snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                     "Unsupported arithmetic operands: %s and %s",
                     type_to_string(left), type_to_string(right));
            return TYPE_DYNAMIC_SINGLETON;

        case TOKEN_STAR:
            if ((left->kind == TYPE_INT || left->kind == TYPE_FLOAT) &&
                (right->kind == TYPE_INT || right->kind == TYPE_FLOAT)) {
                if (left->kind == TYPE_INT && right->kind == TYPE_INT) {
                    return TYPE_INT_SINGLETON;
                }
                return TYPE_FLOAT_SINGLETON;
            }
            if ((left->kind == TYPE_STRING && right->kind == TYPE_INT) ||
                (left->kind == TYPE_INT && right->kind == TYPE_STRING)) {
                return TYPE_STRING_SINGLETON;
            }
            if ((left->kind == TYPE_ARRAY && right->kind == TYPE_INT) ||
                (left->kind == TYPE_INT && right->kind == TYPE_ARRAY)) {
                return left->kind == TYPE_ARRAY ? left : right;
            }
            ctx->had_error = true;
            snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                     "Unsupported arithmetic operands: %s and %s",
                     type_to_string(left), type_to_string(right));
            return TYPE_DYNAMIC_SINGLETON;
            
        case TOKEN_SLASH:
        case TOKEN_POWER:
            if (!((left->kind == TYPE_INT || left->kind == TYPE_FLOAT) &&
                  (right->kind == TYPE_INT || right->kind == TYPE_FLOAT))) {
                ctx->had_error = true;
                snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                         "Unsupported arithmetic operands: %s and %s",
                         type_to_string(left), type_to_string(right));
                return TYPE_DYNAMIC_SINGLETON;
            }
            return TYPE_FLOAT_SINGLETON;
            
        /* Comparison: always bool */
        case TOKEN_LT:
        case TOKEN_GT:
        case TOKEN_LTE:
        case TOKEN_GTE:
        case TOKEN_EQ:
        case TOKEN_NEQ:
        case TOKEN_IS:
        case TOKEN_IS_NOT:
        case TOKEN_IS_LESS:
        case TOKEN_IS_GREATER:
        case TOKEN_IS_AT_MOST:
        case TOKEN_IS_AT_LEAST:
            return TYPE_BOOL_SINGLETON;
            
        /* Logic: bool */
        case TOKEN_AND:
        case TOKEN_OR:
            return TYPE_BOOL_SINGLETON;
            
        default:
            return TYPE_DYNAMIC_SINGLETON;
    }
}

Type* infer_unary(ASTNode* node, TypeContext* ctx) {
    VexiumTokenType op = node->as.unary.op;
    Type* operand = infer_from_node(node->as.unary.operand, ctx);
    
    switch (op) {
        case TOKEN_MINUS:
            if (operand->kind == TYPE_INT) return TYPE_INT_SINGLETON;
            if (operand->kind == TYPE_FLOAT) return TYPE_FLOAT_SINGLETON;
            return TYPE_FLOAT_SINGLETON;
            
        case TOKEN_NOT:
            return TYPE_BOOL_SINGLETON;
            
        default:
            return TYPE_DYNAMIC_SINGLETON;
    }
}

Type* infer_identifier(ASTNode* node, TypeContext* ctx) {
    const char* name = node->as.identifier.name;
    Type* bound = type_context_lookup(ctx, name);
    if (bound) return bound;

    /* Unknown names may be builtins/module symbols resolved at runtime. */
    return TYPE_DYNAMIC_SINGLETON;
}

Type* infer_call(ASTNode* node, TypeContext* ctx) {
    Type* callee_type = infer_from_node(node->as.call.callee, ctx);

    if (!callee_type || callee_type->kind == TYPE_DYNAMIC) {
        return TYPE_DYNAMIC_SINGLETON;
    }
    
    /* Create fresh type variable for result */
    Type* result_type = type_fresh_variable(ctx, "call");
    
    /* Build expected function type from arguments */
    int arg_count = node->as.call.args.count;
    Type** arg_types = NULL;
    if (arg_count > 0) {
        arg_types = malloc(sizeof(Type*) * arg_count);
        for (int i = 0; i < arg_count; i++) {
            arg_types[i] = infer_from_node(node->as.call.args.items[i], ctx);
        }
    }
    
    /* Expected function type: (arg_types) -> result_type */
    Type* expected = type_create_function(arg_types, arg_count, result_type);
    
    /* Unify callee with expected function type */
    if (!type_unify(callee_type, expected)) {
        ctx->had_error = true;
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Type mismatch in function call");
    }
    
    /* Return the unified result type */
    return type_apply(result_type, ctx);
}

Type* infer_function(ASTNode* node, TypeContext* ctx) {
    /* Create fresh type variables for parameters */
    Type** param_types = NULL;
    int param_count = node->as.fn_decl.params.count;
    
    if (param_count > 0) {
        param_types = malloc(sizeof(Type*) * param_count);
        for (int i = 0; i < param_count; i++) {
            Param* p = &node->as.fn_decl.params.items[i];
            if (p->type_name) {
                /* Use explicit annotation */
                param_types[i] = type_parse_string(p->type_name, ctx);
            } else {
                /* Create fresh type variable */
                param_types[i] = type_fresh_variable(ctx, "t");
            }
            /* Bind parameter name to its type in context */
            type_context_bind(ctx, p->name, param_types[i]);
        }
    }
    
    /* Infer or use explicit return type */
    Type* return_type;
    if (node->as.fn_decl.return_type) {
        return_type = type_parse_string(node->as.fn_decl.return_type, ctx);
    } else {
        return_type = type_fresh_variable(ctx, "r");
    }
    
    /* Infer body type and unify with return type */
    Type* body_type = infer_from_node(node->as.fn_decl.body, ctx);
    type_unify(body_type, return_type);
    
    return type_create_function(param_types, param_count, 
                                 type_apply(return_type, ctx));
}

Type* infer_from_node(ASTNode* node, TypeContext* ctx) {
    if (!node) return TYPE_NOTHING_SINGLETON;
    
    switch (node->type) {
        case NODE_INT_LITERAL:
        case NODE_FLOAT_LITERAL:
        case NODE_BOOL_LITERAL:
        case NODE_STRING_LITERAL:
        case NODE_NOTHING_LITERAL:
            return infer_literal(node);
            
        case NODE_ARRAY_LITERAL:
            return infer_array(node, ctx);
            
        case NODE_MAP_LITERAL:
            return infer_map(node, ctx);
            
        case NODE_BINARY_OP:
            return infer_binary(node, ctx);
            
        case NODE_UNARY_OP:
            return infer_unary(node, ctx);
            
        case NODE_IDENTIFIER:
            return infer_identifier(node, ctx);
            
        case NODE_CALL:
            return infer_call(node, ctx);
            
        case NODE_FN_DECL:
            return infer_function(node, ctx);

        case NODE_LET_DECL:
        case NODE_CONST_DECL: {
            Type* value_type = infer_from_node(node->as.var_decl.value, ctx);
            Type* declared_type = NULL;
            if (node->as.var_decl.type_name) {
                declared_type = type_parse_string(node->as.var_decl.type_name, ctx);
                if (!type_is_assignable(declared_type, value_type)) {
                    ctx->had_error = true;
                    snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                             "Cannot assign %s to declared type %s for '%s'",
                             type_to_string(value_type), type_to_string(declared_type),
                             node->as.var_decl.name);
                }
                type_context_bind(ctx, node->as.var_decl.name, declared_type);
                return declared_type;
            }
            type_context_bind(ctx, node->as.var_decl.name, value_type);
            return value_type;
        }

        case NODE_ASSIGN: {
            Type* value_type = infer_from_node(node->as.assign.value, ctx);
            if (node->as.assign.target && node->as.assign.target->type == NODE_IDENTIFIER) {
                const char* name = node->as.assign.target->as.identifier.name;
                Type* target_type = type_context_lookup(ctx, name);
                if (!target_type) {
                    if (ctx->strict_mode) {
                        ctx->had_error = true;
                        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                                 "Assignment to undefined variable: %s", name);
                    }
                    return value_type;
                }

                if (node->as.assign.op == TOKEN_PLUS_ASSIGN) {
                    if (target_type->kind == TYPE_DYNAMIC || value_type->kind == TYPE_DYNAMIC) {
                        return target_type;
                    }
                    if (target_type->kind == TYPE_STRING && value_type->kind == TYPE_STRING) {
                        return TYPE_STRING_SINGLETON;
                    }
                    if (target_type->kind == TYPE_ARRAY && value_type->kind == TYPE_ARRAY) {
                        return target_type;
                    }
                    if ((target_type->kind == TYPE_INT || target_type->kind == TYPE_FLOAT) &&
                        (value_type->kind == TYPE_INT || value_type->kind == TYPE_FLOAT)) {
                        return (target_type->kind == TYPE_INT && value_type->kind == TYPE_INT)
                            ? TYPE_INT_SINGLETON : TYPE_FLOAT_SINGLETON;
                    }
                }

                if (node->as.assign.op == TOKEN_STAR_ASSIGN) {
                    if (target_type->kind == TYPE_DYNAMIC || value_type->kind == TYPE_DYNAMIC) {
                        return target_type;
                    }
                    if (target_type->kind == TYPE_STRING && value_type->kind == TYPE_INT) {
                        return TYPE_STRING_SINGLETON;
                    }
                    if (target_type->kind == TYPE_ARRAY && value_type->kind == TYPE_INT) {
                        return target_type;
                    }
                    if ((target_type->kind == TYPE_INT || target_type->kind == TYPE_FLOAT) &&
                        (value_type->kind == TYPE_INT || value_type->kind == TYPE_FLOAT)) {
                        return (target_type->kind == TYPE_INT && value_type->kind == TYPE_INT)
                            ? TYPE_INT_SINGLETON : TYPE_FLOAT_SINGLETON;
                    }
                }

                if (!type_is_assignable(target_type, value_type)) {
                    ctx->had_error = true;
                    snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                             "Type mismatch assigning %s to %s for '%s'",
                             type_to_string(value_type), type_to_string(target_type), name);
                }
                return target_type;
            }
            return value_type;
        }

        case NODE_EXPR_STMT:
            return infer_from_node(node->as.expr_stmt.expr, ctx);

        case NODE_BLOCK: {
            Type* last = TYPE_NOTHING_SINGLETON;
            for (int i = 0; i < node->as.block.statements.count; i++) {
                last = infer_from_node(node->as.block.statements.items[i], ctx);
            }
            return last;
        }

        case NODE_PROGRAM: {
            Type* last = TYPE_NOTHING_SINGLETON;
            for (int i = 0; i < node->as.program.statements.count; i++) {
                last = infer_from_node(node->as.program.statements.items[i], ctx);
            }
            return last;
        }
            
        default:
            return TYPE_DYNAMIC_SINGLETON;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TYPE STRING REPRESENTATION
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void type_to_string_impl(Type* type, char* buf, int size, int* pos) {
    if (*pos >= size - 1) return;
    
    switch (type->kind) {
        case TYPE_NOTHING:
            *pos += snprintf(buf + *pos, size - *pos, "nothing");
            break;
        case TYPE_DYNAMIC:
            *pos += snprintf(buf + *pos, size - *pos, "dynamic");
            break;
        case TYPE_INT:
            *pos += snprintf(buf + *pos, size - *pos, "int");
            break;
        case TYPE_FLOAT:
            *pos += snprintf(buf + *pos, size - *pos, "float");
            break;
        case TYPE_BOOL:
            *pos += snprintf(buf + *pos, size - *pos, "bool");
            break;
        case TYPE_STRING:
            *pos += snprintf(buf + *pos, size - *pos, "str");
            break;
        case TYPE_ARRAY:
            *pos += snprintf(buf + *pos, size - *pos, "[");
            type_to_string_impl(type->element_type, buf, size, pos);
            *pos += snprintf(buf + *pos, size - *pos, "]");
            break;
        case TYPE_MAP:
            *pos += snprintf(buf + *pos, size - *pos, "{");
            type_to_string_impl(type->key_type, buf, size, pos);
            *pos += snprintf(buf + *pos, size - *pos, ": ");
            type_to_string_impl(type->value_type, buf, size, pos);
            *pos += snprintf(buf + *pos, size - *pos, "}");
            break;
        case TYPE_OPTION:
            *pos += snprintf(buf + *pos, size - *pos, "?");
            type_to_string_impl(type->element_type, buf, size, pos);
            break;
        case TYPE_VARIABLE:
            *pos += snprintf(buf + *pos, size - *pos, "%s", type->var_name);
            break;
        case TYPE_STRUCT:
            *pos += snprintf(buf + *pos, size - *pos, "%s", type->struct_name);
            break;
        default:
            *pos += snprintf(buf + *pos, size - *pos, "<?>");
            break;
    }
}

char* type_to_string(Type* type) {
    static char buf[256];
    int pos = 0;
    buf[0] = '\0';
    type_to_string_impl(type, buf, sizeof(buf), &pos);
    char* result = strdup(buf);
    if (!result) {
        fprintf(stderr, "[Type System] Out of memory in type_to_string\n");
        static char fallback[] = "<?>";
        return fallback;  /* Return static fallback to avoid another alloc */
    }
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TYPE FREE
 * ═══════════════════════════════════════════════════════════════════════════════ */

void type_free(Type* type) {
    if (!type) return;
    
    /* Don't free singletons */
    if (type == TYPE_NOTHING_SINGLETON ||
        type == TYPE_DYNAMIC_SINGLETON ||
        type == TYPE_INT_SINGLETON ||
        type == TYPE_FLOAT_SINGLETON ||
        type == TYPE_BOOL_SINGLETON ||
        type == TYPE_STRING_SINGLETON) {
        return;
    }
    
    /* Free child types */
    if (type->element_type) type_free(type->element_type);
    if (type->key_type) type_free(type->key_type);
    if (type->value_type) type_free(type->value_type);
    if (type->return_type) type_free(type->return_type);
    
    /* Free arrays */
    if (type->param_types) {
        for (int i = 0; i < type->param_count; i++) {
            type_free(type->param_types[i]);
        }
        free(type->param_types);
    }
    if (type->tuple_types) {
        for (int i = 0; i < type->tuple_count; i++) {
            type_free(type->tuple_types[i]);
        }
        free(type->tuple_types);
    }
    if (type->union_types) {
        for (int i = 0; i < type->union_count; i++) {
            type_free(type->union_types[i]);
        }
        free(type->union_types);
    }
    
    /* Free strings */
    if (type->struct_name) free(type->struct_name);
    if (type->var_name) free(type->var_name);
    
    free(type);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TYPE APPLICATION (Apply substitutions to resolve type variables)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Forward declaration for recursive application */
static Type* type_apply_impl(Type* type, TypeContext* ctx);

static Type* type_apply_impl(Type* type, TypeContext* ctx) {
    if (!type) return NULL;
    
    /* If it's a type variable, look up its binding */
    if (type->kind == TYPE_VARIABLE) {
        if (type->binding) {
            /* Apply recursively in case the binding is also a variable */
            return type_apply_impl(type->binding, ctx);
        }
        /* Unbound variable - return as-is */
        return type;
    }
    
    /* For other types, recursively apply to children */
    Type* result = type_alloc();
    result->kind = type->kind;
    result->line = type->line;
    result->column = type->column;
    
    switch (type->kind) {
        case TYPE_ARRAY:
        case TYPE_OPTION:
            result->element_type = type_apply_impl(type->element_type, ctx);
            break;
            
        case TYPE_MAP:
            result->key_type = type_apply_impl(type->key_type, ctx);
            result->value_type = type_apply_impl(type->value_type, ctx);
            break;
            
        case TYPE_FUNCTION:
            result->param_count = type->param_count;
            result->return_type = type_apply_impl(type->return_type, ctx);
            if (type->param_count > 0) {
                result->param_types = (Type**)malloc(sizeof(Type*) * type->param_count);
                for (int i = 0; i < type->param_count; i++) {
                    result->param_types[i] = type_apply_impl(type->param_types[i], ctx);
                }
            }
            break;
            
        case TYPE_TUPLE:
            result->tuple_count = type->tuple_count;
            if (type->tuple_count > 0) {
                result->tuple_types = (Type**)malloc(sizeof(Type*) * type->tuple_count);
                for (int i = 0; i < type->tuple_count; i++) {
                    result->tuple_types[i] = type_apply_impl(type->tuple_types[i], ctx);
                }
            }
            break;
            
        case TYPE_UNION:
            result->union_count = type->union_count;
            if (type->union_count > 0) {
                result->union_types = (Type**)malloc(sizeof(Type*) * type->union_count);
                for (int i = 0; i < type->union_count; i++) {
                    result->union_types[i] = type_apply_impl(type->union_types[i], ctx);
                }
            }
            break;
            
        case TYPE_STRUCT:
            result->struct_name = type->struct_name ? strdup(type->struct_name) : NULL;
            if (type->struct_name && !result->struct_name) {
                fprintf(stderr, "[Type System] Out of memory copying struct name\n");
                free(result);
                return NULL;
            }
            break;
            
        default:
            /* Primitive types - just return the singleton */
            free(result);
            return type;
    }
    
    return result;
}

Type* type_apply(Type* type, TypeContext* ctx) {
    (void)ctx;  /* Currently unused - reserved for future context-based lookup */
    return type_apply_impl(type, ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TYPE UNIFICATION (Hindley-Milner)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Check if type variable occurs in type (prevents infinite types) */
bool type_occurs_in(Type* var, Type* type) {
    if (!var || !type) return false;
    
    /* Dereference any bound type variables first */
    while (type->kind == TYPE_VARIABLE && type->binding) {
        type = type->binding;
    }
    
    /* Check if this is the same type variable */
    if (type->kind == TYPE_VARIABLE) {
        return type->id == var->id;
    }
    
    /* Recursively check composite types */
    switch (type->kind) {
        case TYPE_ARRAY:
        case TYPE_OPTION:
            return type_occurs_in(var, type->element_type);
            
        case TYPE_MAP:
            return type_occurs_in(var, type->key_type) || 
                   type_occurs_in(var, type->value_type);
                   
        case TYPE_FUNCTION:
            for (int i = 0; i < type->param_count; i++) {
                if (type_occurs_in(var, type->param_types[i])) return true;
            }
            return type_occurs_in(var, type->return_type);
            
        case TYPE_TUPLE:
            for (int i = 0; i < type->tuple_count; i++) {
                if (type_occurs_in(var, type->tuple_types[i])) return true;
            }
            return false;
            
        case TYPE_UNION:
            for (int i = 0; i < type->union_count; i++) {
                if (type_occurs_in(var, type->union_types[i])) return true;
            }
            return false;
            
        default:
            return false;
    }
}

/* Unify two types, updating type variable bindings.
 * Returns true if unification succeeds, false on conflict.
 */
bool type_unify(Type* a, Type* b) {
    if (!a || !b) return false;
    
    /* Dereference any bound type variables */
    while (a->kind == TYPE_VARIABLE && a->binding) a = a->binding;
    while (b->kind == TYPE_VARIABLE && b->binding) b = b->binding;
    
    /* Same reference or structurally equal */
    if (type_equals(a, b)) return true;
    
    /* Variable binding */
    if (a->kind == TYPE_VARIABLE) {
        if (type_occurs_in(a, b)) return false; /* Occurs check fails */
        a->binding = b;
        return true;
    }
    if (b->kind == TYPE_VARIABLE) {
        if (type_occurs_in(b, a)) return false; /* Occurs check fails */
        b->binding = a;
        return true;
    }
    
    /* Same kind required for unification */
    if (a->kind != b->kind) return false;
    
    /* Unify component types */
    switch (a->kind) {
        case TYPE_ARRAY:
        case TYPE_OPTION:
            return type_unify(a->element_type, b->element_type);
            
        case TYPE_MAP:
            return type_unify(a->key_type, b->key_type) &&
                   type_unify(a->value_type, b->value_type);
                   
        case TYPE_FUNCTION:
            if (a->param_count != b->param_count) return false;
            for (int i = 0; i < a->param_count; i++) {
                if (!type_unify(a->param_types[i], b->param_types[i])) 
                    return false;
            }
            return type_unify(a->return_type, b->return_type);
            
        case TYPE_STRUCT:
            return strcmp(a->struct_name, b->struct_name) == 0;
            
        case TYPE_TUPLE:
            if (a->tuple_count != b->tuple_count) return false;
            for (int i = 0; i < a->tuple_count; i++) {
                if (!type_unify(a->tuple_types[i], b->tuple_types[i]))
                    return false;
            }
            return true;
            
        case TYPE_UNION:
            if (a->union_count != b->union_count) return false;
            for (int i = 0; i < a->union_count; i++) {
                if (!type_unify(a->union_types[i], b->union_types[i]))
                    return false;
            }
            return true;
            
        default:
            return true;
    }
}

/* Check if a type is concrete (contains no unbound type variables) */
bool type_is_concrete(Type* type) {
    if (!type) return true;
    
    /* Dereference bound variables */
    while (type->kind == TYPE_VARIABLE && type->binding) {
        type = type->binding;
    }
    
    /* Unbound type variable is not concrete */
    if (type->kind == TYPE_VARIABLE) return false;
    
    /* Check composite types */
    switch (type->kind) {
        case TYPE_ARRAY:
        case TYPE_OPTION:
            return type_is_concrete(type->element_type);
            
        case TYPE_MAP:
            return type_is_concrete(type->key_type) && 
                   type_is_concrete(type->value_type);
                   
        case TYPE_FUNCTION:
            for (int i = 0; i < type->param_count; i++) {
                if (!type_is_concrete(type->param_types[i])) return false;
            }
            return type_is_concrete(type->return_type);
            
        case TYPE_TUPLE:
            for (int i = 0; i < type->tuple_count; i++) {
                if (!type_is_concrete(type->tuple_types[i])) return false;
            }
            return true;
            
        case TYPE_UNION:
            for (int i = 0; i < type->union_count; i++) {
                if (!type_is_concrete(type->union_types[i])) return false;
            }
            return true;
            
        default:
            return true;
    }
}

/* Check if sub is a subtype of super */
bool type_is_subtype(Type* sub, Type* super) {
    if (!sub || !super) return false;
    if (type_equals(sub, super)) return true;
    if (super->kind == TYPE_DYNAMIC) return true; /* Everything is subtype of dynamic */
    
    /* Optional<T> is subtype of T (with nothing as valid value) */
    if (super->kind == TYPE_OPTION) {
        return type_is_subtype(sub, super->element_type) ||
               sub->kind == TYPE_NOTHING; /* nothing is valid for optionals */
    }
    
    /* For unions: sub is subtype if it's subtype of any member */
    if (super->kind == TYPE_UNION) {
        for (int i = 0; i < super->union_count; i++) {
            if (type_is_subtype(sub, super->union_types[i])) return true;
        }
        return false;
    }
    
    /* For function types: contravariant in args, covariant in return */
    if (sub->kind == TYPE_FUNCTION && super->kind == TYPE_FUNCTION) {
        if (sub->param_count != super->param_count) return false;
        
        /* Parameters are contravariant: super's params must be subtype of sub's */
        for (int i = 0; i < sub->param_count; i++) {
            if (!type_is_subtype(super->param_types[i], sub->param_types[i]))
                return false;
        }
        
        /* Return type is covariant: sub's return must be subtype of super's */
        return type_is_subtype(sub->return_type, super->return_type);
    }
    
    return false;
}

/* Check if source type can be assigned to target type */
bool type_is_assignable(Type* target, Type* source) {
    return type_is_subtype(source, target);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  TYPE PARSER
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Parse a type name string into a Type structure.
 * Handles: int, str, bool, float, nothing, dynamic
 *           Array<T>, Map<K,V>, T?
 *           fn(T1, T2) -> R
 *           (T1, T2, T3) for tuples
 *           T1 | T2 for unions
 */
static Type* type_parse_internal(const char** str, TypeContext* ctx);

Type* type_parse_string(const char* type_str, TypeContext* ctx) {
    if (!type_str) return TYPE_DYNAMIC_SINGLETON;
    
    const char* p = type_str;
    
    /* Skip leading whitespace */
    while (*p == ' ' || *p == '\t') p++;
    
    return type_parse_internal(&p, ctx);
}

static const char* parse_identifier(const char** str) {
    const char* start = *str;
    while (isalnum(**str) || **str == '_') (*str)++;
    
    size_t len = *str - start;
    char* ident = (char*)malloc(len + 1);
    if (ident) {
        strncpy(ident, start, len);
        ident[len] = '\0';
    }
    return ident;
}

static Type* type_parse_internal(const char** str, TypeContext* ctx) {
    /* Skip whitespace */
    while (**str == ' ' || **str == '\t') (*str)++;
    
    if (**str == '\0') return TYPE_DYNAMIC_SINGLETON;
    
    /* Parse identifier */
    const char* start = *str;
    while (isalnum(**str) || **str == '_') (*str)++;
    
    size_t len = *str - start;
    if (len == 0) return TYPE_DYNAMIC_SINGLETON;
    
    /* Extract name */
    char name[64];
    if (len >= sizeof(name)) len = sizeof(name) - 1;
    strncpy(name, start, len);
    name[len] = '\0';
    
    /* Check for function type */
    if (strcmp(name, "fn") == 0) {
        /* Skip whitespace */
        while (**str == ' ' || **str == '\t') (*str)++;
        
        /* Expect '(' */
        if (**str != '(') return TYPE_DYNAMIC_SINGLETON;
        (*str)++;
        
        /* Parse parameter types */
        Type** params = NULL;
        int param_count = 0;
        int param_capacity = 0;
        
        while (**str && **str != ')') {
            Type* param = type_parse_internal(str, ctx);
            
            /* Grow params array */
            if (param_count >= param_capacity) {
                param_capacity = param_capacity == 0 ? 4 : param_capacity * 2;
                params = (Type**)realloc(params, sizeof(Type*) * param_capacity);
            }
            params[param_count++] = param;
            
            /* Skip whitespace */
            while (**str == ' ' || **str == '\t' || **str == ',') (*str)++;
        }
        
        /* Skip ')' */
        if (**str == ')') (*str)++;
        
        /* Parse return type if present */
        Type* ret = TYPE_NOTHING_SINGLETON;
        while (**str == ' ' || **str == '\t') (*str)++;
        
        if (**str == '-' && *(*str + 1) == '>') {
            *str += 2;
            ret = type_parse_internal(str, ctx);
        }
        
        return type_create_function(params, param_count, ret);
    }
    
    /* Check basic types */
    if (strcmp(name, "int") == 0) return TYPE_INT_SINGLETON;
    if (strcmp(name, "str") == 0) return TYPE_STRING_SINGLETON;
    if (strcmp(name, "bool") == 0) return TYPE_BOOL_SINGLETON;
    if (strcmp(name, "float") == 0) return TYPE_FLOAT_SINGLETON;
    if (strcmp(name, "nothing") == 0) return TYPE_NOTHING_SINGLETON;
    if (strcmp(name, "dynamic") == 0) return TYPE_DYNAMIC_SINGLETON;
    
    /* Check for generics */
    while (**str == ' ' || **str == '\t') (*str)++;
    
    if (**str == '<') {
        (*str)++;
        
        if (strcmp(name, "Array") == 0) {
            Type* elem = type_parse_internal(str, ctx);
            while (**str != '>' && **str) (*str)++;
            if (**str == '>') (*str)++;
            return type_create_array(elem);
        }
        
        if (strcmp(name, "Map") == 0) {
            Type* key = type_parse_internal(str, ctx);
            while (**str == ' ' || **str == '\t' || **str == ',') (*str)++;
            Type* value = type_parse_internal(str, ctx);
            while (**str != '>' && **str) (*str)++;
            if (**str == '>') (*str)++;
            return type_create_map(key, value);
        }
        
        if (strcmp(name, "Option") == 0) {
            Type* inner = type_parse_internal(str, ctx);
            while (**str != '>' && **str) (*str)++;
            if (**str == '>') (*str)++;
            return type_create_option(inner);
        }
    }
    
    /* Check for shorthand optional: int?, str? */
    if (**str == '?') {
        (*str)++;
        Type* inner;
        if (strcmp(name, "int") == 0) inner = TYPE_INT_SINGLETON;
        else if (strcmp(name, "str") == 0) inner = TYPE_STRING_SINGLETON;
        else if (strcmp(name, "bool") == 0) inner = TYPE_BOOL_SINGLETON;
        else if (strcmp(name, "float") == 0) inner = TYPE_FLOAT_SINGLETON;
        else {
            /* Generic type variable */
            inner = type_fresh_variable(ctx, name);
        }
        return type_create_option(inner);
    }
    
    /* Unknown name - create a type variable */
    return type_fresh_variable(ctx, name);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *  PUBLIC TYPE CHECKING API
 *  This function is called from main.c to type-check the entire program
 * ═══════════════════════════════════════════════════════════════════════════════ */

int vex_type_check(ASTNode* program) {
    if (!program) return 0;
    
    /* Initialize type system singletons */
    type_system_init();
    
    /* Create type context for inference */
    TypeContext ctx;
    type_context_init(&ctx, true);  /* Enforce strict checking in compiler gate */
    
    /* Check for --strict flag via environment or default to false */
    /* For now, use gradual typing - errors are warnings unless strict */
    
    int error_count = 0;
    
    /* Type-check each statement in the program */
    if (program->type == NODE_PROGRAM) {
        for (int i = 0; i < program->as.program.statements.count; i++) {
            ASTNode* stmt = program->as.program.statements.items[i];
            infer_from_node(stmt, &ctx);
            
            if (ctx.had_error) {
                fprintf(stderr, "[Type Error] %s at line %d\n", 
                        ctx.error_msg, stmt->line);
                error_count++;
                ctx.had_error = false;  /* Reset for next statement */
            }
        }
    }
    
    /* In strict mode, check for any unbound type variables */
    if (ctx.strict_mode) {
        /* Additional strict checks could go here */
    }
    
    type_context_free(&ctx);
    type_system_shutdown();
    
    return error_count;
}

/* Check a single function declaration */
int vex_type_check_function(ASTNode* fn_node, bool strict) {
    if (!fn_node || fn_node->type != NODE_FN_DECL) return 0;
    
    type_system_init();
    
    TypeContext ctx;
    type_context_init(&ctx, strict);
    
    /* Infer function type */
    Type* fn_type = infer_function(fn_node, &ctx);
    
    int result = 0;
    if (ctx.had_error) {
        fprintf(stderr, "[Type Error] In function '%s': %s\n",
                fn_node->as.fn_decl.name ? fn_node->as.fn_decl.name : "anonymous",
                ctx.error_msg);
        result = 1;
    }
    
    /* Check if return type is concrete in strict mode */
    if (strict && fn_type && !type_is_concrete(fn_type)) {
        fprintf(stderr, "[Type Error] Function '%s' has generic return type\n",
                fn_node->as.fn_decl.name ? fn_node->as.fn_decl.name : "anonymous");
        result = 1;
    }
    
    if (fn_type) type_free(fn_type);
    type_context_free(&ctx);
    type_system_shutdown();
    
    return result;
}
