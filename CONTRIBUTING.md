# Contributing

1. Build with `./build.sh` or `make`.
2. Run `make test` before submitting a change.
3. Keep the language surface minimal. New features require a specification
   change and a decision entry in `docs/DECISIONS.md`.
4. Do not add external dependencies. The interpreter must build with a C99
   compiler and the Python reference must use only the standard library.
5. Do not introduce libc I/O outside `src/pal/`. `tests/portability_check.sh`
   enforces this rule.
6. Add a conformance or error test for every behavior change.
