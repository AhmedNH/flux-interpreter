CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g -Isrc -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lm

SRC = src/lexer.c src/ast.c src/parser.c src/interpreter.c src/main.c
OBJ = $(SRC:.c=.o)
BIN = flux

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(BIN)
