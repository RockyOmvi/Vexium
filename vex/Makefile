# Vexium Language - Build Makefile
CC = gcc
CFLAGS = -std=c99 -O2
CUDA ?= 0
NVRTC ?= 0
SRC_DIR = src
SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/lexer.c $(SRC_DIR)/token.c $(SRC_DIR)/ast.c $(SRC_DIR)/parser.c $(SRC_DIR)/interpreter.c $(SRC_DIR)/stdlib.c $(SRC_DIR)/stdlib_extended.c $(SRC_DIR)/module_executor.c $(SRC_DIR)/compiler.c $(SRC_DIR)/vm.c $(SRC_DIR)/module_loader.c $(SRC_DIR)/json.c $(SRC_DIR)/time_module.c $(SRC_DIR)/http_module.c $(SRC_DIR)/http_server.c $(SRC_DIR)/gc.c $(SRC_DIR)/bytecode_cache.c $(SRC_DIR)/task.c $(SRC_DIR)/type_system.c $(SRC_DIR)/package.c $(SRC_DIR)/tensor.c
LDFLAGS = -lm -lws2_32
TARGET = vexium.exe

ifeq ($(CUDA),1)
CFLAGS += -DUSE_CUDA
LDFLAGS += -lcudart -lcuda
endif

ifeq ($(NVRTC),1)
CFLAGS += -DUSE_NVRTC
LDFLAGS += -lnvrtc
endif
# Pre-build cleanup to avoid permission issues on Windows (e.g., if the binary is still in use)
PREBUILD := @cmd /c "echo Pre-build cleanup: terminating vexium.exe if running & taskkill /IM vexium.exe /F 1>nul 2>nul & exit /b 0"

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

run: $(TARGET)
	@echo.
	@if not "$(FILE)"=="" ( .\$(TARGET) run $(FILE) ) else ( .\$(TARGET) run examples\hello.vxm )

run-vm: $(TARGET)
	@echo.
	@if not "$(FILE)"=="" ( .\$(TARGET) run-vm $(FILE) ) else ( .\$(TARGET) run-vm examples\test_vm.vxm )

gpu-test: $(TARGET)
	@echo.
	@echo === Running GPU smoke test (interpreter + VM) ===
	.\$(TARGET) run examples\test_gpu.vxm
	.\$(TARGET) run-vm examples\test_gpu.vxm

ai-test: $(TARGET)
	@echo.
	@echo === Running AI smoke test (interpreter + VM) ===
	.\$(TARGET) run examples\test_ai.vxm
	.\$(TARGET) run-vm examples\test_ai.vxm

data-test: $(TARGET)
	@echo.
	@echo === Running Data module test (interpreter + VM) ===
	.\$(TARGET) run examples\test_data.vxm
	.\$(TARGET) run-vm examples\test_data.vxm

transformer-test: $(TARGET)
	@echo.
	@echo === Running Transformer primitives test (interpreter + VM) ===
	.\$(TARGET) run examples\test_transformer.vxm
	.\$(TARGET) run-vm examples\test_transformer.vxm

onnx-test: $(TARGET)
	@echo.
	@echo === Running ONNX smoke test (interpreter + VM) ===
	.\$(TARGET) run examples\test_onnx.vxm
	.\$(TARGET) run-vm examples\test_onnx.vxm

bench: $(TARGET)
	@echo.
	.\$(TARGET) bench examples\bench_fib.vxm

ast: $(TARGET)
	@echo.
	.\$(TARGET) ast examples\test_lexer.vxm

lex: $(TARGET)
	.\$(TARGET) lex examples\hello.vxm

.PHONY: all clean test test-vm run run-vm gpu-test ai-test data-test transformer-test onnx-test bench debug debug-vm ast lex
