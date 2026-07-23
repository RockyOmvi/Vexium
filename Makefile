# Vexium Language - Build Makefile
CC = gcc
CFLAGS = -std=c99 -O2
SRC_DIR = src
SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/lexer.c $(SRC_DIR)/token.c $(SRC_DIR)/ast.c $(SRC_DIR)/parser.c $(SRC_DIR)/interpreter.c $(SRC_DIR)/stdlib.c $(SRC_DIR)/value.c $(SRC_DIR)/chunk.c $(SRC_DIR)/compiler.c $(SRC_DIR)/vm.c $(SRC_DIR)/typechecker.c $(SRC_DIR)/formatter.c $(SRC_DIR)/builder.c $(SRC_DIR)/thread.c $(SRC_DIR)/stdlib_ai.c $(SRC_DIR)/stdlib_net.c $(SRC_DIR)/stdlib_system.c $(SRC_DIR)/disasm.c $(SRC_DIR)/profiler.c $(SRC_DIR)/stdlib_json.c $(SRC_DIR)/stdlib_path.c $(SRC_DIR)/stdlib_db.c $(SRC_DIR)/stdlib_web.c $(SRC_DIR)/gc.c $(SRC_DIR)/cuda_driver.c $(SRC_DIR)/gpu_driver.c $(SRC_DIR)/jit.c $(SRC_DIR)/lsp.c $(SRC_DIR)/wasm_emitter.c
TARGET = vexium.exe

ifeq ($(OS),Windows_NT)
    LIBS = -lm -lws2_32
else
    LIBS = -lm -lpthread -ldl
endif

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

clean:
	del /Q $(TARGET) 2>nul

test: $(TARGET)
	@echo.
	@echo === Running test_interpreter.vxm ===
	.\$(TARGET) run examples\test_interpreter.vxm
	@echo.
	@echo === Running v2 Feature Suite ===
	.\$(TARGET) check examples\test_phase6.vxm
	.\$(TARGET) run examples\test_phase6.vxm

ast: $(TARGET)
	@echo.
	.\$(TARGET) ast examples\test_lexer.vxm

lex: $(TARGET)
	.\$(TARGET) lex examples\hello.vxm

.PHONY: all clean test debug ast lex
