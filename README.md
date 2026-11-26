<h1 align="center">B--</h1>

<p align="center">
  A small, fully defined programming language.<br>
  Strict C99 bytecode interpreter. Python reference implementation. No dependencies.
</p>

<p align="center">
  <a href="https://github.com/erluza/bmm-lang/actions"><img alt="CI" src="https://github.com/erluza/bmm-lang/actions/workflows/ci.yml/badge.svg"></a>
  <img alt="Release" src="https://img.shields.io/badge/release-17.14-blue">
  <img alt="Language specification" src="https://img.shields.io/badge/spec-1.0.0%20frozen-informational">
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-green"></a>
</p>

<p align="center">
  <a href="docs/LANGUAGE_REFERENCE.md">Reference</a> ·
  <a href="docs/TUTORIAL.md">Tutorial</a> ·
  <a href="docs/BUILTINS.md">Built-ins</a> ·
  <a href="docs/FAQ.md">FAQ</a> ·
  <a href="docs/ARCHITECTURE.md">Architecture</a> ·
  <a href="CHANGELOG.md">Changelog</a> ·
  <a href="README.es.md">Español</a>
</p>

---

B-- (pronounced "be minus minus") is a small programming language with three
types, twelve keywords, and a closed set of built-in functions. Source is
compiled to bytecode for a stack-based virtual machine. A mark-and-sweep
garbage collector manages heap objects.

The language specification is frozen at version 1.0.0. Release 17.14 is a
documentation and discoverability release; it does not change language
behavior.

## Contents

- [What B-- is](#what-b--is)
- [Language at a glance](#language-at-a-glance)
- [Install](#install)
- [Usage](#usage)
- [Documentation](#documentation)
- [Project status](#project-status)
- [Testing](#testing)
- [FAQ](#faq)
- [Machine-readable metadata](#machine-readable-metadata)
- [Repository layout](#repository-layout)
- [Portability](#portability)
- [Contributing](#contributing)
- [License](#license)

## What B-- is

B-- is an imperative language for small programs and for studying language
implementation. It has integers, strings, arrays, functions, block scope,
one loop form, and one conditional form. It has no floating-point numbers,
classes, exceptions, null values, or implicit conversions.

The interpreter in `src/` is written in C99 and builds with a C99 compiler and
`make`. The reference implementation in `ref/bmm.py` is a single Python file
that uses only the standard library. Both implementations are expected to
produce identical output for the test suite.

The C implementation compiles source to bytecode for a stack VM, runs a
mark-and-sweep collector, and keeps all operating-system access behind
`src/pal/pal.h`.

## Language at a glance

```bmm
fn fib(n)
  if n < 2
    ret n
  end
  ret fib(n - 1) + fib(n - 2)
end

let i = 0
loop i < 10
  println(str(fib(i)))
  i = i + 1
end
```

| Property | Value |
|---|---|
| Types | `int` (32-bit signed, wraparound), `str` (immutable byte sequence), `arr` (mutable, zero-indexed, bounds-checked) |
| Keywords | `let` `fn` `if` `else` `loop` `break` `ret` `use` `and` `or` `not` `end` |
| Built-ins | `print` `println` `input` `len` `push` `pop` `str` `int` `chr` `ord` `sub` `arr` `exit` `argv` `clock` |
| Execution | Bytecode, stack-based VM, mark-and-sweep GC |
| Specification | 1.0.0, frozen |
| License | MIT |

## Install

Requirements: a C99 compiler and `make`. No other dependencies and no
administrator privileges.

```text noexec: shell command
sh
make
make test
make install PREFIX="$HOME/.local"
```

Without `make`:

```text noexec: shell command
sh
./build.sh
```

## Usage

```text noexec: shell command
sh
bmm hello.bmm                      # run a source file
bmm                                # start the REPL
bmm build hello.bmm -o hello.bmc   # compile to bytecode
bmm run hello.bmc                  # run compiled bytecode
```

`hello.bmm`:

```bmm
println("Hello, world")
```

## Documentation

| Topic | Document |
|---|---|
| Language | [Manifesto](docs/MANIFESTO.md) · [Reference](docs/LANGUAGE_REFERENCE.md) · [Grammar](docs/GRAMMAR.ebnf) · [Built-ins](docs/BUILTINS.md) · [Errors](docs/ERRORS.md) |
| Learning | [Tutorial](docs/TUTORIAL.md) · [`examples/`](examples) |
| Internals | [Architecture](docs/ARCHITECTURE.md) · [Bytecode](docs/BYTECODE.md) · [BMC format](docs/BMC_FORMAT.md) · [Design decisions](docs/DECISIONS.md) |
| Integration | [Embedding](docs/EMBEDDING.md) · [Porting](docs/PORTING.md) |
| Reference questions | [FAQ](docs/FAQ.md) |
| Verification | [Gate output](docs/GATE_OUTPUT.txt) · [Verification report](docs/VERIFICATION_REPORT.md) |

## Project status

- Language specification: 1.0.0, frozen.
- Repository release: 17.14.
- CI: GitHub Actions, gcc and clang, strict C99 warnings as errors.
- Test gate: 18 stages.
- Conformance: 20 programs mapped to 25 numbered language rules.
- Examples: 15 programs with expected output.
- Fuzzing: 2,000 differential programs, 1,000 random or truncated sources,
  and 500 corrupt `.bmc` files.
- Mutation testing: 34 mutations, all detected.
- Sanitizers: AddressSanitizer and UndefinedBehaviorSanitizer over 3,550
  programs, zero findings.
- Dependencies: none beyond a C99 compiler, `make`, and Python 3 for the
  reference implementation and tests.

## Testing

```text noexec: shell command
make test
```

`make test` runs `tests/gate.sh`. The gate covers build, portability, examples,
conformance, errors, formatter behavior, REPL state, bytecode execution,
limits, documentation blocks, fuzzing, sanitizers, GC stress, mutations,
regressions, report generation, benchmarks, and a clean-room build.

The C and Python interpreters are compared byte for byte on stdout, stderr,
and exit status.

## FAQ

### What is B--?

B-- is a small, fully defined programming language with a C99 bytecode
interpreter and a Python reference implementation. It is designed for
readability, portability, and language implementation study.

### What types and statements does B-- have?

B-- has `int`, `str`, and `arr`. It has `let`, `fn`, `if`, `else`, `loop`,
`break`, `ret`, `use`, `and`, `or`, `not`, and `end`. It does not have
floating-point numbers, classes, exceptions, or implicit conversions.

### Does B-- require external libraries?

No. The C interpreter uses a small platform layer in `src/pal/` and the C
standard library. The Python reference uses only the Python standard library.

### How is B-- implemented?

The C implementation compiles source to bytecode and runs a stack-based VM
with a mark-and-sweep collector. The Python file `ref/bmm.py` is an executable
specification that interprets the same language.

### Is B-- intended for production use?

B-- is a small language and a reference implementation. The repository
documents its limits and its test results; it does not claim fitness for
production systems.

### How do I port B--?

Implement the functions in `src/pal/pal.h` and adjust `bmm_config.h`. See
[PORTING.md](docs/PORTING.md).

### How do I cite B--?

Use [CITATION.cff](CITATION.cff). The repository URL is
`https://github.com/erluza/bmm-lang`.

### Where is the language specification?

The specification is in [docs/LANGUAGE_REFERENCE.md](docs/LANGUAGE_REFERENCE.md),
with the grammar in [docs/GRAMMAR.ebnf](docs/GRAMMAR.ebnf).

## Machine-readable metadata

- [llms.txt](llms.txt): canonical summary for language models and retrieval systems.
- [CITATION.cff](CITATION.cff): citation metadata.
- [VERSION](VERSION): repository release.
- [docs/GATE_OUTPUT.txt](docs/GATE_OUTPUT.txt): latest gate stage results.
- [docs/VERIFICATION_REPORT.md](docs/VERIFICATION_REPORT.md): generated verification report.

## Repository layout

| Path | Contents |
|---|---|
| `src/` | Interpreter sources and platform abstraction layer (PAL) |
| `ref/` | Python reference implementation |
| `docs/` | Specification and guides |
| `examples/` | Fifteen example programs |
| `stdlib/` | Standard library, written in B-- |
| `tests/` | Conformance, error, fuzz and benchmark tests |

## Portability

All platform access is isolated in `src/pal/pal.h` and its implementations.
Porting requires implementing roughly ten functions and adjusting
`bmm_config.h`. See [PORTING.md](docs/PORTING.md).

## Contributing

Contributions are welcome. Keep changes minimal and consistent with the
existing style, and run `make test` before submitting. See
[CONTRIBUTING.md](CONTRIBUTING.md).

## License

Released under the [MIT License](LICENSE).

---

Resumen en español: B-- es un lenguaje de programación pequeño y totalmente
definido, con un intérprete de bytecode en C99 y una implementación de
referencia en Python. La especificación del lenguaje está congelada en 1.0.0.
La release 17.14 actualiza documentación y metadatos; no cambia el
comportamiento del lenguaje.
