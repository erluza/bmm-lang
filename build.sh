#!/bin/sh
set -e
cd "$(dirname "$0")"
cc -std=c99 -Wall -Wextra -Wpedantic -Werror -D_GNU_SOURCE -Isrc -Isrc/pal \
  src/lexer.c src/parser.c src/ast.c src/value.c src/gc.c src/vm.c \
  src/compiler.c src/bytecode_io.c src/disasm.c src/fmt.c src/repl.c \
  src/out.c src/main.c src/pal/pal_posix.c \
  -o bmm
echo "built ./bmm"
