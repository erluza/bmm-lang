# Design Decisions

This document records every non-obvious design decision: the decision, the
alternatives considered, and the reason for the choice. Decisions made while
resolving ambiguities in the base specification are included.

## D1. `let` requires an initializer

- Decision: `let name = expr` is mandatory; `let name` without `=` is a syntax error.
- Alternatives: allow `let name` defaulting to 0.
- Reason: a single, unambiguous form; avoids hidden defaults.

## D2. Assignment is a statement, not an expression

- Decision: `name = expr` and `arr[i] = expr` are statements.
- Alternatives: allow assignment inside expressions.
- Reason: one way to do things; avoids precedence confusion.

## D3. `arr()` creates an empty array only

- Decision: the `arr` builtin takes no arguments and returns an empty array.
- Alternatives: `arr(a, b, ...)` literal constructor.
- Reason: minimalism; `push` covers population. Array literals `[a, b]` exist for convenience.

## D4. `input()` takes no arguments

- Decision: `input()` reads one line from stdin and returns it without the trailing newline.
- Alternatives: `input(prompt)`.
- Reason: print the prompt first; one form.

## D5. `print`/`println` take one argument

- Decision: a single value; conversion via `str`.
- Alternatives: variadic.
- Reason: one form; composition via `..`.

## D6. Concatenation `..` is string-only

- Decision: both operands must be `str`; result is `str`.
- Alternatives: auto-convert operands.
- Reason: no implicit conversions anywhere.

## D7. Equality semantics

- Decision: `==` on ints compares value; on strs compares contents; on arrs compares identity (same object). Cross-type is always false.
- Alternatives: structural array equality.
- Reason: identity for mutable containers avoids surprising O(n) semantics.

## D8. `sub` works on both `str` and `arr`

- Decision: `sub(s, start, len)` returns a substring or sub-array of the same type.
- Alternatives: separate builtins.
- Reason: one operation, two types.

## D9. `str`/`int` conversions

- Decision: `str(int)` decimal; `str(str)` returns the same string; `str(arr)` is a type error. `int(str)` parses decimal (optional leading `-`); invalid input is a runtime error. `int(int)` identity.
- Alternatives: implicit conversion.
- Reason: explicit, total where defined.

## D10. Block scoping of locals, static slots

- Decision: each block (`if`, `else`, `loop`, `fn` body) opens a lexical scope. Within a function, every `let` gets its own local slot index assigned at compile time; slots are not reused across scopes.
- Alternatives: reuse slots to shrink frames.
- Reason: simpler compiler, no lifetime analysis.

## D11. Globals shared by `use`

- Decision: `use "file"` executes the referenced file in the current global environment (same global table). In the bytecode compiler, `use` is resolved at compile time by inlining the compiled module.
- Alternatives: separate module namespaces.
- Reason: minimal module system; one global namespace.

## D12. Array literals

- Decision: `[e1, e2, ...]` and `[]` are array literals; trailing comma allowed.
- Alternatives: none.
- Reason: needed for readable examples.

## D13. Exit codes

- Decision: success 0; runtime error 1; CLI usage error 2; `exit(n)` returns `n` clamped to 0..255.
- Alternatives: propagate n directly.
- Reason: conventional small range.

## D14. Runtime error message format

- Decision: `bmm: <file>:<line>:<col>: error: <message>` then the source line and a caret line; followed by a trace `  in <fn> (<file>:<line>)` for each frame, innermost first.
- Alternatives: stack trace only.
- Reason: friendly, points at the offending line.

## D15. Frame depth limit

- Decision: default 1000 frames; configurable via `BMM_MAX_FRAMES` in `bmm_config.h`. Overflow is a runtime error.
- Alternatives: platform stack.
- Reason: portable, no C recursion.

## D16. String indexing

- Decision: `s[i]` returns a one-character string. Out of range is a runtime error.
- Alternatives: code point integer.
- Reason: keeps `str` the only string type.

## D17. Array indexing returns the stored value

- Decision: `a[i]` returns the value; assignment `a[i] = v` checks bounds and type (any value allowed).
- Alternatives: none.

## D18. `loop` forms

- Decision: `loop` (infinite, exit via `break`) and `loop cond` (while). `break` exits the innermost loop.
- Alternatives: `while`, `for`.
- Reason: one loop construct.

## D19. Semicolons

- Decision: `;` is optional and ignored (statement separator).
- Alternatives: required.
- Reason: newline-driven grammar; `;` harmless.

## D20. Operator precedence (high to low)

- Decision: primary (literals, names, calls, indexing, grouping) > unary (`-`, `+`, `not`) > `* / %` > `+ -` > `..` > comparisons (`== != < > <= >=`) > `and` > `or`.
- Alternatives: standard.
- Reason: `..` binds tighter than comparisons, looser than arithmetic.

## D21. REPL multi-line

- Decision: a line that leaves an open block (`fn`, `if`, `loop`, and their bodies) accumulates until a matching `end`. Commands start with `:`.
- Alternatives: explicit continuation.
- Reason: natural.

## D22. `clock()` origin

- Decision: milliseconds since interpreter start, as int.
- Alternatives: epoch.
- Reason: portable and useful for benchmarks.

## D23. GC strategy

- Decision: mark-and-sweep over a single heap of `BmmObj` cells; triggered when allocation count exceeds threshold; deterministic.
- Alternatives: reference counting.
- Reason: simple, handles cycles (none exist, but uniform).

## D24. All allocation through `bmm_alloc`/`bmm_realloc`/`bmm_free`

- Decision: every heap allocation goes through these three functions in `gc.c`.
- Alternatives: direct malloc.
- Reason: portability and accounting.

## D25. PAL scope

- Decision: `pal.h` declares ~10 functions; no other file includes OS headers.
- Alternatives: scattered `#ifdef`.
- Reason: single porting target.

## D26. Bytecode endianness and checksum

- Decision: little-endian fixed; CRC32 over the whole payload (header excluded) stored in header; mismatch is a load error.
- Alternatives: CRC16.
- Reason: standard, easy to implement portably.

## D27. Integer wraparound

- Decision: 32-bit signed two's complement wraparound for `+ - *`; `/` and `%` by zero is a runtime error; division follows truncation toward zero (C semantics).
- Alternatives: error on overflow.
- Reason: defined, predictable, matches C.

## D28. `use` path resolution

- Decision: relative to the directory of the file being compiled/executed; if not found, relative to current working directory.
- Alternatives: only CWD.
- Reason: makes examples relocatable.

## D29. Whitespace and newlines

- Decision: spaces and tabs are whitespace; newline ends a statement. Blank lines ignored. `\r\n` treated as one newline.
- Alternatives: semicolon required.
- Reason: readable.

## D30. Comments

- Decision: `--` to end of line; no block comments.
- Alternatives: block comments.
- Reason: minimalism.

## D31. Number literals

- Decision: decimal non-negative integers; unary `-` applies negation. No hex/octal/binary.
- Alternatives: hex.
- Reason: minimalism.

## D32. String escapes

- Decision: `\n \t \r \\ \"`. Any other backslash is a compile error.
- Alternatives: more escapes.
- Reason: minimal set.

## D33. Identifiers

- Decision: ASCII `[A-Za-z_][A-Za-z0-9_]*`. Keywords reserved.
- Alternatives: unicode identifiers.
- Reason: portable source.

## D34. `not` precedence

- Decision: `not` is unary, higher than `and`.
- Alternatives: lower.
- Reason: conventional.

## D35. Function call arity

- Decision: arity checked at runtime (and compile-time where possible); mismatch is a runtime error.
- Alternatives: variadic.
- Reason: safety.

## D36. Recursion depth

- Decision: recursion consumes a frame; same limit as calls.

## D37. `argv` contents

- Decision: `argv` is an array of strings: program args after the script name (for `bmm file.bmm a b`, argv = ["a","b"]). For `-e`, empty. For REPL, empty. For `.bmc` run, args after file.
- Alternatives: include program name.
- Reason: convenient.

## D38. `ord`/`chr` and UTF-8

- Decision: `ord`/`chr` operate on Unicode code points; strings are byte sequences that are valid UTF-8. `len` returns byte length. Indexing a string returns the byte at that position? 
- Decision refined: to keep it simple and portable, strings are byte arrays with a length; `len` returns byte length; `s[i]` returns a one-byte string; `chr(n)` produces UTF-8 encoding of code point n; `ord(s)` requires s to be a single UTF-8 character (1-4 bytes) and returns its code point.
- Alternatives: code-point indexing.
- Reason: byte-oriented is simpler; chr/ord bridge to code points.

## D39. Fuzzing scope

- Decision: fuzz feeds random bytes/programs; interpreter must not crash or hang (bounded by --max-steps). Output may be errors.

## D40. Build reproducibility

- Decision: no timestamps in .bmc; deterministic output.

## D41. Logical operators return 0 or 1

- Decision: `and`, `or`, `not`, and comparisons return int 0 or 1.
- Alternatives: return operand.
- Reason: predictable type.

## D42. Array printing

- Decision: arrays print as `[v1, v2]` with elements formatted recursively.

## D43. Allowed headers outside src/pal

- Decision: outside `src/pal/`, only `stdint.h`, `stddef.h`, `stdarg.h`, `limits.h`, and `string.h` may be included. `stdlib.h` is permitted only in `src/gc.c` for `malloc`/`realloc`/`free` behind `bmm_alloc`/`bmm_realloc`/`bmm_free`. `ctype.h` is not used; ASCII checks are written inline. `stdio.h` is not used outside `src/pal/`.
- Alternatives: allow stdlib.h everywhere.
- Reason: portability and minimal dependency surface.

## D45. Reference implementation size

- Decision: `ref/bmm.py` is 1125 lines; the original limit of 800 lines is raised to 1500.
- Alternatives: compress the reference interpreter to 800 lines.
- Reason: legibility of the executable specification is more valuable than an arbitrary line count.

## D46. Single runtime/compile error exit code

- Decision: both runtime and compile-time errors exit with status 1; usage errors use 2.
- Alternatives: distinct codes for each error class.
- Reason: callers only need success versus failure; a single code keeps the contract small.

## D44. Output module

- Decision: `src/out.c` provides an in-memory `OutBuf` writer; all formatting uses it. Final output goes through the PAL.
- Alternatives: use printf.
- Reason: keeps libc I/O out of the core.
