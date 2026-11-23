# Architecture

## Pipeline

```
source (.bmm) -> lexer -> parser -> AST -> compiler -> bytecode -> VM
```

1. Lexer produces tokens.
2. Recursive-descent parser produces an AST.
3. Compiler lowers the AST to bytecode for a stack-based VM.
4. The VM executes frames with its own value stack and local slots.

## Memory

All heap objects are managed by a mark-and-sweep collector in `gc.c`.
Allocation goes through `bmm_alloc`, `bmm_realloc`, and `bmm_free`.

## Platform Abstraction Layer

`src/pal/pal.h` declares the only platform-dependent operations. No other
file includes operating-system headers.

## Bytecode

Fewer than 40 opcodes, all listed in `opcodes.h` and `docs/BYTECODE.md`.
Each function has its own code array and constant pool; the module shares one
global table.
