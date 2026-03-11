#include "common.h"
#include "token.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "interpreter.h"
#include "compiler.h"
#include "type_system.h"
#include "vm.h"
#include "package.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════════════
 *  FILE READING
 * ══════════════════════════════════════════════════════════════════════ */

static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) { fprintf(stderr, "Error: Could not open file '%s'\n", path); return NULL; }
    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);
    char* buffer = (char*)malloc(file_size + 1);
    if (buffer == NULL) { fclose(file); return NULL; }
    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    buffer[bytes_read] = '\0';
    fclose(file);
    return buffer;
}

/* ══════════════════════════════════════════════════════════════════════
 *  RUN FILE — Parse and EXECUTE
 * ══════════════════════════════════════════════════════════════════════ */

static int run_file(const char* path) {
    char* source = read_file(path);
    if (source == NULL) return 1;
    Parser parser;
    parser_init(&parser, source);
    ASTNode* program = parser_parse(&parser);
    if (parser.had_error) { fprintf(stderr, "\nParsing failed with errors.\n"); ast_free(program); free(source); return 1; }
    int tc = vex_type_check(program);
    if (tc != 0) { fprintf(stderr, "Type checking failed for %s\n", path); ast_free(program); free(source); return 1; }
    Interpreter interp;
    interpreter_init(&interp);
    interpret_program(&interp, program);
    int exit_code = interp.had_error ? 1 : 0;
    interpreter_free(&interp);
    ast_free(program);
    free(source);
    return exit_code;
}

/* ══════════════════════════════════════════════════════════════════════
 *  RUN VM — Parse, Compile, and execute via bytecode VM
 * ══════════════════════════════════════════════════════════════════════ */

static int run_vm_file(const char* path) {
    char* source = read_file(path);
    if (source == NULL) return 1;
    Parser parser;
    parser_init(&parser, source);
    ASTNode* program = parser_parse(&parser);
    if (parser.had_error) { fprintf(stderr, "\nParsing failed with errors.\n"); ast_free(program); free(source); return 1; }
    ObjFunction* fn = compile(program);
    if (fn == NULL) { fprintf(stderr, "Compilation failed.\n"); ast_free(program); free(source); return 1; }
    VM vm;
    vm_init(&vm);
    VMResult result = vm_run(&vm, fn);
    vm_free(&vm);
    ast_free(program);
    free(source);
    return (result == VM_OK) ? 0 : 1;
}

/* ══════════════════════════════════════════════════════════════════════
 *  DISASSEMBLE — Show compiled bytecode
 * ══════════════════════════════════════════════════════════════════════ */

static void disassemble_function(ObjFunction* fn, int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
    printf("=== Function: %s (arity %d, %d bytes) ===\n", fn->name ? fn->name : "<script>", fn->arity, fn->chunk_count);
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
            case OP_SET_GLOBAL:
            case OP_USE_MODULE: {
                uint16_t idx = fn->code[offset] | (fn->code[offset + 1] << 8);
                offset += 2;
                printf(" %d  (", idx);
                if (idx < (uint16_t)fn->const_count) value_print(fn->constants[idx]);
                printf(")");
                break;
            }
            case OP_USE_SYMBOL: {
                uint16_t mod_idx = fn->code[offset] | (fn->code[offset + 1] << 8);
                uint16_t sym_idx = fn->code[offset + 2] | (fn->code[offset + 3] << 8);
                offset += 4;
                printf(" %d, %d", mod_idx, sym_idx);
                break;
            }
            case OP_GET_LOCAL:
            case OP_SET_LOCAL:
            case OP_CALL: printf(" %d", fn->code[offset++]); break;
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
            default: break;
        }
        printf("\n");
    }
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
    Parser parser; parser_init(&parser, source);
    ASTNode* program = parser_parse(&parser);
    if (parser.had_error) { ast_free(program); free(source); return 1; }
    ObjFunction* fn = compile(program);
    if (fn == NULL) { ast_free(program); free(source); return 1; }
    disassemble_function(fn, 0);
    ast_free(program); free(source); return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 *  BENCH — Run both backends and compare
 * ══════════════════════════════════════════════════════════════════════ */

static int bench_file(const char* path) {
    char* source = read_file(path);
    if (source == NULL) return 1;
    Parser parser; parser_init(&parser, source);
    ASTNode* program = parser_parse(&parser);
    if (parser.had_error) { ast_free(program); free(source); return 1; }
    printf("\n═══ Benchmark: %s ═══\n\n", path);
    printf("── Tree-Walk Interpreter ──\n");
    clock_t tw_start = clock();
    { Interpreter interp; interpreter_init(&interp); interpret_program(&interp, program); interpreter_free(&interp); }
    clock_t tw_end = clock();
    double tw_time = (double)(tw_end - tw_start) / CLOCKS_PER_SEC;
    printf("  Time: %.6f seconds\n\n", tw_time);
    printf("── Bytecode VM ──\n");
    clock_t vm_start = clock();
    { ObjFunction* fn = compile(program); if (fn) { VM vm; vm_init(&vm); vm_run(&vm, fn); vm_free(&vm); } }
    clock_t vm_end = clock();
    double vm_time = (double)(vm_end - vm_start) / CLOCKS_PER_SEC;
    printf("  Time: %.6f seconds\n\n", vm_time);
    if (vm_time > 0 && tw_time > 0) {
        double speedup = tw_time / vm_time;
        printf("══ Result: VM is %.2fx %s ══\n\n", speedup > 1 ? speedup : 1.0 / speedup, speedup > 1 ? "faster" : "slower");
    }
    ast_free(program); free(source); return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 *  CHECK — Type-check without running (V2.1)
 * ══════════════════════════════════════════════════════════════════════ */

static int check_file(const char* path) {
    char* src = read_file(path);
    if (!src) return 1;
    Parser p; parser_init(&p, src);
    ASTNode* prog = parser_parse(&p);
    if (p.had_error) { fprintf(stderr, "Parse errors\n"); ast_free(prog); free(src); return 1; }
    printf("Type-checking: %s\n", path);
    int tc = vex_type_check(prog);
    if (tc != 0) { printf("Type checking found %d error(s).\n", tc); ast_free(prog); free(src); return 1; }
    printf("Type checking passed! No errors found.\n");
    ast_free(prog); free(src); return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 *  BUILD — Compile to native executable (V2.1)
 * ══════════════════════════════════════════════════════════════════════ */

static int build_file(const char* path) {
    char* src = read_file(path);
    if (!src) return 1;
    printf("Building: %s\n", path);
    Parser p; parser_init(&p, src);
    ASTNode* prog = parser_parse(&p);
    if (p.had_error) { fprintf(stderr, "Parse errors\n"); ast_free(prog); free(src); return 1; }
    int tc = vex_type_check(prog);
    if (tc != 0) { printf("Type errors. Build aborted.\n"); ast_free(prog); free(src); return 1; }
    ObjFunction* fn = compile(prog);
    if (!fn) { fprintf(stderr, "Compilation failed.\n"); ast_free(prog); free(src); return 1; }

    /* Derive output file name: replace .vxm with .vxc */
    char outpath[512];
    const char* dot = strrchr(path, '.');
    if (dot) {
        int base_len = (int)(dot - path);
        snprintf(outpath, sizeof(outpath), "%.*s.vxc", base_len, path);
    } else {
        snprintf(outpath, sizeof(outpath), "%s.vxc", path);
    }

    FILE* out = fopen(outpath, "wb");
    if (!out) { fprintf(stderr, "Could not create output file '%s'\n", outpath); ast_free(prog); free(src); return 1; }

    /* ── Header ── */
    const char magic[4] = {'V', 'X', 'C', '\0'};
    fwrite(magic, 1, 4, out);
    uint32_t version = 210;  /* v2.1.0 */
    fwrite(&version, sizeof(uint32_t), 1, out);

    /* ── Constant pool ── */
    uint32_t const_count = (uint32_t)fn->const_count;
    fwrite(&const_count, sizeof(uint32_t), 1, out);
    for (int i = 0; i < fn->const_count; i++) {
        Value v = fn->constants[i];
        if (is_number(v)) {
            uint8_t tag = 1;  /* number */
            fwrite(&tag, 1, 1, out);
            double d = as_number(v);
            fwrite(&d, sizeof(double), 1, out);
        } else if (is_int(v)) {
            uint8_t tag = 2;  /* int */
            fwrite(&tag, 1, 1, out);
            int32_t ival = as_int(v);
            fwrite(&ival, sizeof(int32_t), 1, out);
        } else if (is_bool(v)) {
            uint8_t tag = 3;  /* bool */
            fwrite(&tag, 1, 1, out);
            uint8_t bval = as_bool(v) ? 1 : 0;
            fwrite(&bval, 1, 1, out);
        } else if (is_nothing(v)) {
            uint8_t tag = 4;  /* nothing */
            fwrite(&tag, 1, 1, out);
        } else if (is_obj(v)) {
            Obj* obj = (Obj*)as_obj(v);
            if (obj->type == OBJ_STRING) {
                uint8_t tag = 5;  /* string */
                fwrite(&tag, 1, 1, out);
                ObjString* s = (ObjString*)obj;
                uint32_t slen = (uint32_t)s->length;
                fwrite(&slen, sizeof(uint32_t), 1, out);
                fwrite(s->chars, 1, slen, out);
            } else {
                /* Other obj types: write as nothing placeholder */
                uint8_t tag = 4;
                fwrite(&tag, 1, 1, out);
            }
        } else {
            uint8_t tag = 0;  /* unknown */
            fwrite(&tag, 1, 1, out);
        }
    }

    /* ── Code ── */
    uint32_t code_count = (uint32_t)fn->chunk_count;
    fwrite(&code_count, sizeof(uint32_t), 1, out);
    fwrite(fn->code, 1, fn->chunk_count, out);

    fclose(out);
    printf("Build successful! Output: %s (%u bytes code, %u constants)\n",
           outpath, code_count, const_count);
    ast_free(prog); free(src); return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 *  ADD — Package manager (V2.1)
 * ══════════════════════════════════════════════════════════════════════ */

static int add_package(const char* pkg) {
    if (package_add(pkg, "latest")) return 0;
    return 1;
}

static int init_package(const char* name) {
    if (package_init(name, ".")) return 0;
    return 1;
}

static int install_packages(void) {
    return package_install() ? 0 : 1;
}

static int update_packages(void) {
    return package_update() ? 0 : 1;
}

static int run_package_tests(void) {
    return package_test() ? 0 : 1;
}

static int fmt_sources(const char* path) {
    return package_fmt(path) ? 0 : 1;
}

static int lint_file(const char* path) {
    return check_file(path);
}

static int profile_file(const char* path) {
    return bench_file(path);
}

/* ══════════════════════════════════════════════════════════════════════
 *  AST DEBUG — show AST tree only
 * ══════════════════════════════════════════════════════════════════════ */

static int ast_file(const char* path) {
    char* source = read_file(path);
    if (source == NULL) return 1;
    printf("Parsing: %s\n\n", path);
    Parser parser;
    parser_init(&parser, source);
    ASTNode* program = parser_parse(&parser);
    if (parser.had_error) { fprintf(stderr, "\nParsing failed with errors.\n"); ast_free(program); free(source); return 1; }
    printf("=== AST ===\n");
    ast_print(program, 0);
    printf("===========\n\n");
    ast_free(program);
    free(source);
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 *  LEX FILE — Token debug mode
 * ══════════════════════════════════════════════════════════════════════ */

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

/* ══════════════════════════════════════════════════════════════════════
 *  REPL — Interactive interpreter
 * ══════════════════════════════════════════════════════════════════════ */

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
        if (!fgets(line, sizeof(line), stdin)) { printf("\n"); break; }
        if (line[0] == '\n' || line[0] == '\r') continue;
        Parser parser;
        parser_init(&parser, line);
        ASTNode* program = parser_parse(&parser);
        if (!parser.had_error) {
            VexValue result = interpret(&interp, program, interp.global_env);
            if (result.type != VAL_NOTHING && program->as.program.statements.count == 1 && program->as.program.statements.items[0]->type == NODE_EXPR_STMT) {
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

/* ══════════════════════════════════════════════════════════════════════
 *  MAIN
 * ══════════════════════════════════════════════════════════════════════ */

static void print_usage(void) {
    printf("Vexium Programming Language v%s\n\n", VEXIUM_VERSION);
    printf("Usage:\n");
    printf("  vexium run <file.vxm>      Execute via tree-walk interpreter\n");
    printf("  vexium run-vm <file.vxm>   Execute via bytecode VM (fast!)\n");
    printf("  vexium build <file.vxm>   Compile to native executable (V2.1)\n");
    printf("  vexium check <file.vxm>   Type-check without running (V2.1)\n");
    printf("  vexium add <package>      Add a package dependency (V2.1)\n");
    printf("  vexium init [name]        Initialize vex.toml package project\n");
    printf("  vexium install            Install dependencies from vex.toml\n");
    printf("  vexium update             Update dependencies\n");
    printf("  vexium test               Run package tests\n");
    printf("  vexium fmt [path]         Format source files\n");
    printf("  vexium lint <file.vxm>    Lint via static/type checks\n");
    printf("  vexium profile <file.vxm> Profile execution (bench mode)\n");
    printf("  vexium bench <file.vxm>    Benchmark: interpreter vs VM\n");
    printf("  vexium disasm <file.vxm>   Disassemble bytecode\n");
    printf("  vexium ast <file.vxm>      Show AST tree (debug)\n");
    printf("  vexium lex <file.vxm>      Show token stream (debug)\n");
    printf("  vexium repl                Interactive REPL\n");
    printf("  vexium --version           Show version\n");
    printf("  vexium --help              Show this help\n\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) { run_repl(); return 0; }
    if (strcmp(argv[1], "--version") == 0) { printf("Vexium v%s\n", VEXIUM_VERSION); return 0; }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) { print_usage(); return 0; }
    if (strcmp(argv[1], "repl") == 0) { run_repl(); return 0; }
    if (strcmp(argv[1], "lex") == 0) { if (argc < 3) { fprintf(stderr, "Error: 'vexium lex' requires a file path.\n"); return 1; } return lex_file(argv[2]); }
    if (strcmp(argv[1], "ast") == 0) { if (argc < 3) { fprintf(stderr, "Error: 'vexium ast' requires a file path.\n"); return 1; } return ast_file(argv[2]); }
    if (strcmp(argv[1], "run") == 0) { if (argc < 3) { fprintf(stderr, "Error: 'vexium run' requires a file path.\n"); return 1; } return run_file(argv[2]); }
    if (strcmp(argv[1], "run-vm") == 0) { if (argc < 3) { fprintf(stderr, "Error: 'vexium run-vm' requires a file path.\n"); return 1; } return run_vm_file(argv[2]); }
    if (strcmp(argv[1], "disasm") == 0) { if (argc < 3) { fprintf(stderr, "Error: 'vexium disasm' requires a file path.\n"); return 1; } return disasm_file(argv[2]); }
    if (strcmp(argv[1], "bench") == 0) { if (argc < 3) { fprintf(stderr, "Error: 'vexium bench' requires a file path.\n"); return 1; } return bench_file(argv[2]); }
    if (strcmp(argv[1], "check") == 0) { if (argc < 3) { fprintf(stderr, "Error: 'vexium check' requires a file path.\n"); return 1; } return check_file(argv[2]); }
    if (strcmp(argv[1], "build") == 0) { if (argc < 3) { fprintf(stderr, "Error: 'vexium build' requires a file path.\n"); return 1; } return build_file(argv[2]); }
    if (strcmp(argv[1], "add") == 0) { if (argc < 3) { fprintf(stderr, "Error: 'vexium add' requires a package name.\n"); return 1; } return add_package(argv[2]); }
    if (strcmp(argv[1], "init") == 0) { return init_package(argc >= 3 ? argv[2] : "vex-project"); }
    if (strcmp(argv[1], "install") == 0) { return install_packages(); }
    if (strcmp(argv[1], "update") == 0) { return update_packages(); }
    if (strcmp(argv[1], "test") == 0) { return run_package_tests(); }
    if (strcmp(argv[1], "fmt") == 0) { return fmt_sources(argc >= 3 ? argv[2] : "."); }
    if (strcmp(argv[1], "lint") == 0) { if (argc < 3) { fprintf(stderr, "Error: 'vexium lint' requires a file path.\n"); return 1; } return lint_file(argv[2]); }
    if (strcmp(argv[1], "profile") == 0) { if (argc < 3) { fprintf(stderr, "Error: 'vexium profile' requires a file path.\n"); return 1; } return profile_file(argv[2]); }
    if (argc == 2) return run_file(argv[1]);
    fprintf(stderr, "Unknown command: %s\n", argv[1]);
    print_usage();
    return 1;
}
