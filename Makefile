CC ?= gcc
CFLAGS ?= -Wall -Wextra -g
CPPFLAGS ?= -Iinclude -Isrc/frontend -Isrc/backend
BUILD_DIR ?= build
TARGET ?= $(BUILD_DIR)/donkey
SRC = src/main.c src/frontend/lexer.c src/frontend/preprocess.c src/frontend/parser.c src/frontend/parser_decl.c src/frontend/parser_stmt.c src/frontend/parser_expr.c src/analysis/semantic.c src/analysis/type.c src/analysis/symbol.c src/backend/codegen.c src/backend/codegen_emit.c src/backend/codegen_data.c src/backend/codegen_stmt.c src/backend/codegen_expr.c src/support/mem.c src/support/file.c src/support/diag.c src/support/cli.c src/support/dump.c

.PHONY: all clean sample test

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(SRC) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $(TARGET) $(SRC)

sample: $(TARGET)
	$(TARGET) examples/sample.c build/sample.asm

test:
	sh scripts/test.sh

clean:
	rm -rf $(BUILD_DIR)
