#include "common.h"
#include "token.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "interpreter.h"
#include "compiler.h"
#include "vm.h"
#include <time.h>

/* ══════════════════════════════════════════════════════════════
 *  FILE READING
 * ══════════════════════════════════════════════════════════════ */

static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Error: Could not open file '%s'\n", path);
        return NULL;
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(file_size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Error: Not enough memory to read '%s'\n", path);
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    buffer[bytes_read] = '\0';

    fclose(file);
    return buffer;
}

/* ══════════════════════════════════════════════════════════════
 *  RUN FILE — Parse and EXECUTE
 * ══════════════════════════════════════════════════════════════ */

static int run_file(const char* path) {
    char* source = read_file(path);
    if (source == NULL) return 1;

    Parser parser;
    parser_init(&parser, source);
    ASTNode* program = parser_parse(&parser);

    if (parser.had_error) {
        fprintf(stderr, "\nParsing failed with errors.\n");
        ast_free(program);
        free(source);
        return 1;
    }

    Interpreter interp;
    interpreter_init(&interp);
    interpret_program(&interp, program);

    int exit_code = interp.had_error ? 1 : 0;
    interpreter_free(&interp);
    ast_free(program);
    free(source);
    return exit_code;
}

/* ══════════════════════════════════════════════════════════════
 *  RUN VM — Parse, Compile, and execute via bytecode VM
 * ══════════════════════════════════════════════════════════════ */

static int run_vm_file(const char* path) {
    char* source = read_file(path);
    if (source == NULL) return 1;

    Parser parser;
    parser_init(&parser, source);
    ASTNode* program = parser_parse(&parser);

    if (parser.had_error) {
        fprintf(stderr, "\nParsing failed with errors.\n");
        ast_free(program);
        free(source);
        return 1;
    }

    ObjFunction* fn = compile(program);
    if (fn == NULL) {
        fprintf(stderr, "Compilation failed.\n");
        ast_free(program);
        free(source);
        return 1;
    }

    VM vm;
    vm_init(&vm);
    VMResult result = vm_run(&vm, fn);
    vm_free(&vm);

    ast_free(program);
    free(source);
    return (result == VM_OK) ? 0 : 1;
}

/* ══════════════════════════════════════════════════════════════
 *  DISASSEMBLE — Show compiled bytecode
 * ══════════════════════════════════════════════════════════════ */

static void disassemble_function(ObjFunction* fn, int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
    printf("=== Function: %s (arity %d, %d bytes) ===\n",
           fn->name ? fn->name : "<script>", fn->arity, fn->chunk_count);

    int offset = 0;
    while (offset < fn->chunk_count) {
        for (int i = 0; i < indent; i++) printf("  ");
        printf("%04d  ", offset);

        uint8_t op = fn->code[offset++];
        printf("%-20s", opcode_name(op));

        switch (op) {
            case OP_CONST:
            case OP_DEFINE_GLOBAL:
            case OP_GET_GLOBAL:
            case OP_SET_GLOBAL: {
                uint16_t idx = fn->code[offset] | (fn->code[offset + 1] << 8);
                offset += 2;
                printf(" %d  (", idx);
                if (idx < (uint16_t)fn->const_count) {
                    value_print(fn->constants[idx]);
                }
                printf(")");
                break;
            }
            case OP_GET_LOCAL:
            case OP_SET_LOCAL:
            case OP_CALL:
                printf(" %d", fn->code[offset++]);
                break;
            case OP_JMP:
            case OP_JMP_IF_FALSE: {
                uint16_t jmp = fn->code[offset] | (fn->code[offset + 1] << 8);
                offset += 2;
                printf(" +%d -> %d", jmp, offset + jmp);
                break;
            }
            case OP_LOOP: {
                uint16_t jmp = fn->code[offset] | (fn->code[offset + 1] << 8);
                offset += 2;
                printf(" -%d -> %d", jmp, offset - jmp);
                break;
            }
            case OP_ARRAY:
            case OP_MAP: {
                uint16_t count = fn->code[offset] | (fn->code[offset + 1] << 8);
                offset += 2;
                printf(" count=%d", count);
                break;
            }
            default:
                break;
        }
        printf("\n");
    }

    /* Disassemble nested functions */
    for (int i = 0; i < fn->const_count; i++) {
        Value c = fn->constants[i];
        if (is_obj(c)) {
            Obj* obj = (Obj*)as_obj(c);
            if (obj->type == OBJ_FUNCTION) {
                ObjFunction* nested = (ObjFunction*)obj;
                if (nested->code != NULL && nested->chunk_count > 0) {
                    printf("\n");
                    disassemble_function(nested, indent + 1);
                }
            }
        }
    }
}

static int disasm_file(const char* path) {
    char* source = read_file(path);
    if (source == NULL) return 1;

    Parser parser;
    parser_init(&parser, source);
    ASTNode* program = parser_parse(&parser);

    if (parser.had_error) {
        fprintf(stderr, "\nParsing failed with errors.\n");
        ast_free(program);
        free(source);
        return 1;
    }

    ObjFunction* fn = compile(program);
    if (fn == NULL) {
        fprintf(stderr, "Compilation failed.\n");
        ast_free(program);
        free(source);
        return 1;
    }

    disassemble_function(fn, 0);

    ast_free(program);
    free(source);
    return 0;
}

/* ══════════════════════════════════════════════════════════════
 *  BENCH — Run both backends and compare
 * ══════════════════════════════════════════════════════════════ */

static int bench_file(const char* path) {
    char* source = read_file(path);
    if (source == NULL) return 1;

    /* Parse once */
    Parser parser;
    parser_init(&parser, source);
    ASTNode* program = parser_parse(&parser);
    if (parser.had_error) {
        fprintf(stderr, "\nParsing failed with errors.\n");
        ast_free(program);
        free(source);
        return 1;
    }

    /* ── Tree-walk interpreter benchmark ── */
    printf("\n═══ Benchmark: %s ═══\n\n", path);
    printf("── Tree-Walk Interpreter ──\n");
    clock_t tw_start = clock();
    {
        Interpreter interp;
        interpreter_init(&interp);
        interpret_program(&interp, program);
        interpreter_free(&interp);
    }
    clock_t tw_end = clock();
    double tw_time = (double)(tw_end - tw_start) / CLOCKS_PER_SEC;
    printf("  Time: %.6f seconds\n\n", tw_time);

    /* ── Bytecode VM benchmark ── */
    printf("── Bytecode VM ──\n");
    clock_t vm_start = clock();
    {
        ObjFunction* fn = compile(program);
        if (fn) {
            VM vm;
            vm_init(&vm);
            vm_run(&vm, fn);
            vm_free(&vm);
        } else {
            printf("  (compilation failed)\n");
        }
    }
    clock_t vm_end = clock();
    double vm_time = (double)(vm_end - vm_start) / CLOCKS_PER_SEC;
    printf("  Time: %.6f seconds\n\n", vm_time);

    /* ── Comparison ── */
    if (vm_time > 0 && tw_time > 0) {
        double speedup = tw_time / vm_time;
        printf("══ Result: VM is %.2fx %s ══\n\n",
               speedup > 1 ? speedup : 1.0 / speedup,
               speedup > 1 ? "faster" : "slower");
    }

    ast_free(program);
    free(source);
    return 0;
}

/* ══════════════════════════════════════════════════════════════
 *  AST DEBUG — show AST tree only
 * ══════════════════════════════════════════════════════════════ */

static int ast_file(const char* path) {
    char* source = read_file(path);
    if (source == NULL) return 1;

    printf("Parsing: %s\n\n", path);
    Parser parser;
    parser_init(&parser, source);
    ASTNode* program = parser_parse(&parser);

    if (parser.had_error) {
        fprintf(stderr, "\nParsing failed with errors.\n");
        ast_free(program);
        free(source);
        return 1;
    }

    printf("=== AST ===\n");
    ast_print(program, 0);
    printf("===========\n\n");

    ast_free(program);
    free(source);
    return 0;
}

/* ══════════════════════════════════════════════════════════════
 *  LEX FILE — Token debug mode
 * ══════════════════════════════════════════════════════════════ */

static int lex_file(const char* path) {
    char* source = read_file(path);
    if (source == NULL) return 1;

    printf("Lexing: %s\n\n", path);
    Lexer lexer;
    lexer_init(&lexer, source);

    printf("-- Tokens --\n");
    int token_count = 0;
    for (;;) {
        Token token = lexer_next_token(&lexer);
        token_print(token);
        token_count++;
        if (token.type == TOKEN_EOF) break;
        if (token.type == TOKEN_ERROR) break;
    }
    printf("------------\n");
    printf("  Total: %d tokens\n\n", token_count);

    free(source);
    return 0;
}

/* ══════════════════════════════════════════════════════════════
 *  REPL — Interactive interpreter
 * ══════════════════════════════════════════════════════════════ */

static void run_repl(void) {
    char line[4096];

    printf(" __   __          _\n");
    printf(" \\ \\ / /_____ __ (_)_  _ _ __ \n");
    printf("  \\ V / -_) \\ / || | || | '  \\\n");
    printf("   \\_/\\___/_\\_\\ |_|\\_,_|_|_|_|\n\n");
    printf("  Vexium REPL v%s\n", VEXIUM_VERSION);
    printf("  Type code. Ctrl+C to exit.\n\n");

    Interpreter interp;
    interpreter_init(&interp);

    for (;;) {
        printf("vxm> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        /* Skip empty lines */
        if (line[0] == '\n' || line[0] == '\r') continue;

        Parser parser;
        parser_init(&parser, line);
        ASTNode* program = parser_parse(&parser);

        if (!parser.had_error) {
            VexValue result = interpret(&interp, program, interp.global_env);
            /* Show expression results in REPL (if not nothing) */
            if (result.type != VAL_NOTHING &&
                program->as.program.statements.count == 1 &&
                program->as.program.statements.items[0]->type == NODE_EXPR_STMT) {
                printf("=> ");
                vex_print_value(result);
                printf("\n");
            }
        }

        interp.had_error = false;
        interp.signal.type = SIGNAL_NONE;
        ast_free(program);
    }

    interpreter_free(&interp);
}

/* ══════════════════════════════════════════════════════════════
 *  MAIN
 * ══════════════════════════════════════════════════════════════ */

static void print_usage(void) {
    printf("Vexium Programming Language v%s\n\n", VEXIUM_VERSION);
    printf("Usage:\n");
    printf("  vexium run <file.vxm>      Execute via tree-walk interpreter\n");
    printf("  vexium run-vm <file.vxm>   Execute via bytecode VM (fast!)\n");
    printf("  vexium bench <file.vxm>    Benchmark: interpreter vs VM\n");
    printf("  vexium disasm <file.vxm>   Disassemble bytecode\n");
    printf("  vexium ast <file.vxm>      Show AST tree (debug)\n");
    printf("  vexium lex <file.vxm>      Show token stream (debug)\n");
    printf("  vexium repl                Interactive REPL\n");
    printf("  vexium --version           Show version\n");
    printf("  vexium --help              Show this help\n\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        run_repl();
        return 0;
    }

    if (strcmp(argv[1], "--version") == 0) {
        printf("Vexium v%s\n", VEXIUM_VERSION);
        return 0;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage();
        return 0;
    }

    if (strcmp(argv[1], "repl") == 0) {
        run_repl();
        return 0;
    }

    if (strcmp(argv[1], "lex") == 0) {
        if (argc < 3) { fprintf(stderr, "Error: 'vexium lex' requires a file path.\n"); return 1; }
        return lex_file(argv[2]);
    }

    if (strcmp(argv[1], "ast") == 0) {
        if (argc < 3) { fprintf(stderr, "Error: 'vexium ast' requires a file path.\n"); return 1; }
        return ast_file(argv[2]);
    }

    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) { fprintf(stderr, "Error: 'vexium run' requires a file path.\n"); return 1; }
        return run_file(argv[2]);
    }

    if (strcmp(argv[1], "run-vm") == 0) {
        if (argc < 3) { fprintf(stderr, "Error: 'vexium run-vm' requires a file path.\n"); return 1; }
        return run_vm_file(argv[2]);
    }

    if (strcmp(argv[1], "disasm") == 0) {
        if (argc < 3) { fprintf(stderr, "Error: 'vexium disasm' requires a file path.\n"); return 1; }
        return disasm_file(argv[2]);
    }

    if (strcmp(argv[1], "bench") == 0) {
        if (argc < 3) { fprintf(stderr, "Error: 'vexium bench' requires a file path.\n"); return 1; }
        return bench_file(argv[2]);
    }

    /* If just a filename, run it directly */
    if (argc == 2) {
        return run_file(argv[1]);
    }

    fprintf(stderr, "Unknown command: %s\n", argv[1]);
    print_usage();
    return 1;
}
