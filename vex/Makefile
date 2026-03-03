# Vexium Language - Build Makefile
CC = gcc
CFLAGS = -std=c99 -O2
SRC_DIR = src
SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/lexer.c $(SRC_DIR)/token.c $(SRC_DIR)/ast.c $(SRC_DIR)/parser.c $(SRC_DIR)/interpreter.c $(SRC_DIR)/stdlib.c $(SRC_DIR)/compiler.c $(SRC_DIR)/vm.c
LDFLAGS = -lm
TARGET = vexium.exe

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

clean:
	del /Q $(TARGET) 2>nul

test: $(TARGET)
	@echo.
	@echo === Running test_interpreter.vxm ===
	.\$(TARGET) run examples\test_interpreter.vxm

ast: $(TARGET)
	@echo.
	.\$(TARGET) ast examples\test_lexer.vxm

lex: $(TARGET)
	.\$(TARGET) lex examples\hello.vxm

.PHONY: all clean test debug ast lex
