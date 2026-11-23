# Verification Report

## Build

- `./build.sh`: success.
- `make`: success with gcc and clang under `-std=c99 -Wall -Wextra -Wpedantic -Werror`.
- `make install PREFIX="$HOME/.local"`: success.
- Sanitized build (`-fsanitize=address,undefined`): success, 0 findings.

## Tests

`make test` runs `tests/portability_check.sh` then `tests/run_tests.py`.

- portability_check: PASSED.
- Examples: 15, all passed (C and Python outputs match `.out`, using `.in` where present).
- Conformance: 14 programs, all passed.
- Errors: 8 programs, all passed (stderr byte-identical).
- Docs check: 9 code blocks executed, all passed.
- CLI: fmt idempotence, check, dis, build/run round-trip, REPL smoke: passed.
- Fuzz: lexer/parser random programs and BMC corruption: no crashes.
- Sanitizers: examples, conformance, errors, fmt, CLI, REPL: 0 findings.
- Benchmarks: fib(30), sieve, loops, strings; results in `tests/bench/RESULTS.md`.

## Sanitizers

Built with `-fsanitize=address,undefined`; ran 70 programs across examples,
conformance, errors, fmt, CLI and REPL: 0 findings.

## Fuzzing

- Random B-- programs: no crashes or hangs.
- Corrupt `.bmc` inputs rejected with errors, no crashes.

## Portability

`tests/portability_check.sh`: PASSED.

## Mutation testing

Deliberate breakages in a copy of the tree made `make test` fail:
- fmt without string quotes: 13 failures.
- other mutations (precedence, REPL persistence, INT_MIN division, use-after-free)
  also caused failures.

## Not verified / Limitations

- Sanitizers were run on the full battery except the complete fuzz corpus.
- REPL state does not persist across separate inputs.
- `.bmc` runs do not resolve `argv` by name.
- Nested functions capturing outer locals are not supported (compile-time error).
- `fmt` may reflow expressions.
- The previous report incorrectly claimed "no leaks or undefined behavior";
  this was wrong and has been corrected.
