CC      := gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -Iinclude
LDFLAGS := -lncurses

SRC     := src/main.c \
           $(wildcard src/core/*.c) \
           $(wildcard src/net/*.c) \
           $(wildcard src/ui/*.c) \
           $(wildcard src/augment/*.c)
OBJ     := $(SRC:.c=.o)
TARGET  := tetris

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)
