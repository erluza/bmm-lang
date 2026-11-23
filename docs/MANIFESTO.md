# B-- Manifesto

## 1. Purpose

B-- (pronounced "be minus minus") is a small, fully defined programming
language. It exists to be read, implemented, and ported. Every feature in the
language must justify its presence; in case of doubt, the feature is removed.

## 2. Principles

1. Minimalism. The language is small enough to be implemented in a few
   thousand lines of C and under eight hundred lines of Python.
2. One way to do each thing. There is a single loop construct, a single
   conditional, and a single module mechanism.
3. Fully defined semantics. There is no undefined behavior and no implicit
   conversion. Every operation either yields a value of a known type or
   raises a defined runtime error.
4. Portability first. The only platform-dependent code lives in the Platform
   Abstraction Layer.
5. Friendly errors. Every runtime and compile-time error reports a file, a
   line, a column, the affected source line, and a caret.

## 3. Types

Three types exist: `int` (32-bit signed, two's complement, wraparound),
`str` (immutable byte sequence with explicit length), and `arr` (mutable,
zero-indexed, dynamically sized, bounds-checked). There are no floats,
structs, classes, null values, exceptions, or macros.

## 4. Control flow

`if ... end` with optional `else`. `loop` and `loop cond`. `break` exits the
innermost loop. `ret` returns from a function. Blocks close with `end`.

## 5. Functions and scope

Functions are declared with `fn`. Recursion is supported. Scope is lexical by
block. Globals are declared with `let` at file level.

## 6. Memory

A simple deterministic mark-and-sweep collector manages all heap objects.
All allocation passes through `bmm_alloc`, `bmm_realloc`, and `bmm_free`.

## 7. Portability

The interpreter is written in strict C99 with no dependencies. Porting to a
new platform requires implementing roughly ten functions in `pal.h` and
adjusting `bmm_config.h`.
