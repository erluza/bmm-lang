CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -Werror -D_GNU_SOURCE -Isrc -Isrc/pal
LDFLAGS ?=

SRCS = src/lexer.c src/parser.c src/ast.c src/value.c src/gc.c src/vm.c \
       src/compiler.c src/bytecode_io.c src/disasm.c src/fmt.c src/repl.c \
       src/out.c src/main.c src/pal/pal_posix.c

OBJS = $(SRCS:.c=.o)

all: bmm

bmm: $(OBJS)
	$(CC) $(CFLAGS) -o bmm $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test:
	tests/portability_check.sh
	tests/run_tests.sh

install:
	mkdir -p $(PREFIX)/bin
	cp bmm $(PREFIX)/bin/bmm

clean:
	rm -f $(OBJS) bmm
	rm -rf build

.PHONY: all test install clean
