CC ?= gcc
CFLAGS ?= -Wall -Wextra -g
CPPFLAGS ?= -Iinclude
BUILD_DIR ?= build
TARGET ?= $(BUILD_DIR)/donkey
SRC = src/main.c src/lexer.c src/parser.c src/codegen.c

.PHONY: all clean sample

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(SRC) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $(TARGET) $(SRC)

sample: $(TARGET)
	$(TARGET) examples/sample.c build/sample.asm

clean:
	rm -rf $(BUILD_DIR)
