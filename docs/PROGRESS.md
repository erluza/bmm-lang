# Progress

## F0 baseline

- Date: 2026-11-23.
- The pre-hardening tree is recorded in git history.
- Regression probes keep their own fixtures.
- Known defects: argv in .bmc, .bmc diagnostic locations, nested function calls,
  fmt trivia, REPL persistence, and gate coverage.

## F1 gate

- Added `tests/gate.sh`, `tests/gate.py`, `tests/GATE_MANIFEST`.
- `make test` now runs the gate.

## F2 memory and arithmetic

- Fixed the use-after-free in `run_source`.
- uint32 arithmetic, INT_MIN division/modulo/negation defined.
- Sanitizer stage added.

## F3 fmt and REPL

- fmt emits strings with quotes and escapes, preserves comments and parentheses.
- REPL persists globals and functions, rolls back failed entries, no prompts on pipes.

## F4 argv, .bmc, diagnostics

- Global slot 0 reserved for argv; .bmc restores it.
- .bmc version 2 stores filename, line/column tables, global names and source.
- `argv()` reports `'argv' is not a function`.

## F5 limits and GC

- Parser nesting limit 1000.
- Large jump offsets are signed 32-bit.
- GC stress mode (`BMM_GC_STRESS`) and temporary-root fixes in concat/index/sub.

## F6-F7 conformance, fuzz, mutations, regressions

- 15 conformance tests with `.out` and `.code`.
- 2000 differential fuzz programs.
- 24 mutations, 20 regression probes.
- Report generator.

## F8-F9 documentation and cleanroom

- Generated `docs/VERIFICATION_REPORT.md`.
- Cleanroom stage builds and installs from `git ls-files`.

## Final

- Gate passes from a clean tree: 18 stages, all PASS.
- Sanitizers: zero findings on the fixed suites.
- GC stress: examples and conformance outputs identical.
- Mutations: 24 of 24 detected.
- Regressions: 20 of 20 pass.
- Cleanroom build and install: pass.
