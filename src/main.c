#include "common.h"
#include "token.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "interpreter.h"

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

        if (line[0] == '\n' || line[0] == '\r') continue;

        Parser parser;
        parser_init(&parser, line);
        ASTNode* program = parser_parse(&parser);

        if (!parser.had_error) {
            VexValue result = interpret(&interp, program, interp.global_env);

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
    printf("  vexium run <file.vxm>    Execute a Vexium program\n");
    printf("  vexium ast <file.vxm>    Show AST tree (debug)\n");
    printf("  vexium lex <file.vxm>    Show token stream (debug)\n");
    printf("  vexium repl              Interactive REPL\n");
    printf("  vexium --version         Show version\n");
    printf("  vexium --help            Show this help\n\n");
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

    if (argc == 2) {
        return run_file(argv[1]);
    }

    fprintf(stderr, "Unknown command: %s\n", argv[1]);
    print_usage();
    return 1;
}
