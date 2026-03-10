# Vexium Language - Build Makefile
CC = gcc
CFLAGS = -std=c99 -O2
SRC_DIR = src
SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/lexer.c $(SRC_DIR)/token.c $(SRC_DIR)/ast.c $(SRC_DIR)/parser.c $(SRC_DIR)/interpreter.c $(SRC_DIR)/stdlib.c $(SRC_DIR)/compiler.c $(SRC_DIR)/vm.c $(SRC_DIR)/module_loader.c $(SRC_DIR)/json.c $(SRC_DIR)/time_module.c $(SRC_DIR)/http_module.c $(SRC_DIR)/http_server.c $(SRC_DIR)/gc.c $(SRC_DIR)/bytecode_cache.c $(SRC_DIR)/task.c $(SRC_DIR)/type_system.c $(SRC_DIR)/package.c $(SRC_DIR)/tensor.c
LDFLAGS = -lm -lws2_32
TARGET = vexium.exe
# Pre-build cleanup to avoid permission issues on Windows (e.g., if the binary is still in use)
PREBUILD := @echo "Pre-build cleanup: terminating vexium.exe if running" && taskkill /IM vexium.exe /F 2>nul || true

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(PREBUILD)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

debug-vm: CFLAGS += -g -DDEBUG_VM
debug-vm: $(TARGET)

clean:
	del /Q $(TARGET) 2>nul

test: $(TARGET)
	@echo.
	@echo === Running test_interpreter.vxm ===
	.\$(TARGET) run examples\test_interpreter.vxm

test-vm: $(TARGET)
	@echo.
	@echo === Running test_vm.vxm via VM ===
	.\$(TARGET) run-vm examples\test_vm.vxm

bench: $(TARGET)
	@echo.
	.\$(TARGET) bench examples\bench_fib.vxm

ast: $(TARGET)
	@echo.
	.\$(TARGET) ast examples\test_lexer.vxm

lex: $(TARGET)
	.\$(TARGET) lex examples\hello.vxm

.PHONY: all clean test test-vm bench debug debug-vm ast lex
