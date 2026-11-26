# Frequently Asked Questions

## What is B--?

B-- is a small, fully defined programming language. It has three types
(`int`, `str`, `arr`), twelve keywords, and a closed set of built-in
functions. The C99 implementation compiles source to bytecode for a
stack-based virtual machine. The Python implementation in `ref/bmm.py` is an
executable specification.

## What does the name mean?

B-- is pronounced "be minus minus". The name is written with two hyphens.

## Which version is current?

The language specification is frozen at 17.14. Repository release 17.14 is a
documentation and discoverability release and does not change language
behavior.

## What types does B-- have?

`int` is a 32-bit signed integer with defined wraparound. `str` is an
immutable byte sequence. `arr` is a mutable, zero-indexed array with bounds
checks. B-- has no floating-point type, no structs, no classes, no null value,
and no exceptions.

## Does B-- support closures?

No. Functions are top-level declarations. A nested function that captures
outer locals is rejected at compile time.

## Does B-- require a runtime or external libraries?

No. The C interpreter builds with a C99 compiler and `make`. It uses a small
platform layer in `src/pal/` and the C standard library. The Python reference
uses only the Python standard library.

## Which platforms are supported?

The repository includes a POSIX platform implementation. Porting requires
implementing the functions declared in `src/pal/pal.h` and adjusting
`bmm_config.h`. See [PORTING.md](PORTING.md).

## How is the interpreter structured?

The C implementation has a lexer, recursive-descent parser, AST, bytecode
compiler, stack VM, mark-and-sweep collector, and a platform abstraction
layer. See [ARCHITECTURE.md](ARCHITECTURE.md) and [BYTECODE.md](BYTECODE.md).

## How do I run a program?

Build the interpreter with `make`, then run `bmm file.bmm`. The REPL starts
with `bmm`. Bytecode compilation uses `bmm build file.bmm -o file.bmc`, and
compiled files run with `bmm run file.bmc`.

## How do I run the tests?

Run `make test`. The gate covers build, portability, examples, conformance,
errors, formatter behavior, REPL state, bytecode execution, limits,
documentation blocks, fuzzing, sanitizers, GC stress, mutations, regressions,
report generation, benchmarks, and a clean-room build.

## What is the `.bmc` format?

`.bmc` is the bytecode container. Version 2 stores the source filename,
global metadata, functions, line and column tables, source text, and a CRC-32
checksum. See [BMC_FORMAT.md](BMC_FORMAT.md).

## How do I cite B--?

Use [CITATION.cff](../CITATION.cff). The repository URL is
`https://github.com/erluza/bmm-lang`.

## Where can I read the specification?

The language reference is [LANGUAGE_REFERENCE.md](LANGUAGE_REFERENCE.md). The
grammar is [GRAMMAR.ebnf](GRAMMAR.ebnf). The design rationale is
[DECISIONS.md](DECISIONS.md).
