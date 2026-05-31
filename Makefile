CC      := gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -Iinclude
LDFLAGS := -lncurses

# 빌드 출력 디렉토리
BUILD_DIR := build

SRC     := src/main.c \
           $(wildcard src/core/*.c) \
           $(wildcard src/net/*.c) \
           $(wildcard src/ui/*.c) \
           $(wildcard src/augment/*.c) \
           $(wildcard src/scene/*.c)

# 오브젝트 파일을 build/ 디렉토리에 생성
OBJ     := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC))
TARGET  := tetris

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 빌드 디렉토리 구조 자동 생성
$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
