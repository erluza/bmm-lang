<h1 align="center">B--</h1>

<p align="center">
  A small, fully defined programming language.<br>
  Strict C99 bytecode interpreter. Python reference implementation. No dependencies.
</p>

<p align="center">
  <a href="https://github.com/erluza/bmm-lang/actions"><img alt="CI" src="https://github.com/erluza/bmm-lang/actions/workflows/ci.yml/badge.svg"></a>
  <img alt="Version" src="https://img.shields.io/badge/version-1.0.0-blue">
  <img alt="Specification" src="https://img.shields.io/badge/spec-frozen-informational">
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-green"></a>
</p>

<p align="center">
  <a href="docs/LANGUAGE_REFERENCE.md">Reference</a> ·
  <a href="docs/TUTORIAL.md">Tutorial</a> ·
  <a href="docs/BUILTINS.md">Built-ins</a> ·
  <a href="docs/ARCHITECTURE.md">Architecture</a> ·
  <a href="CHANGELOG.md">Changelog</a> ·
  <a href="README.es.md">Español</a>
</p>

---

B-- (pronounced "be minus minus") has three types, twelve keywords and a
closed set of built-in functions. Source is compiled to bytecode for a
stack-based virtual machine, with a mark-and-sweep garbage collector.
The language specification is frozen at version 1.0.0.

## Contents

- [Language at a glance](#language-at-a-glance)
- [Install](#install)
- [Usage](#usage)
- [Documentation](#documentation)
- [Repository layout](#repository-layout)
- [Portability](#portability)
- [Testing](#testing)
- [Contributing](#contributing)
- [License](#license)

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

| | |
|---|---|
| **Types** | `int` (32-bit signed, wraparound), `str` (immutable byte sequence), `arr` (mutable, zero-indexed, bounds-checked) |
| **Keywords** | `let` `fn` `if` `else` `loop` `break` `ret` `use` `and` `or` `not` `end` |
| **Built-ins** | `print` `println` `input` `len` `push` `pop` `str` `int` `chr` `ord` `sub` `arr` `exit` `argv` `clock` |
| **Execution** | Bytecode, stack-based VM, mark-and-sweep GC |

## Install

Requirements: a C99 compiler and `make`. No other dependencies and no
administrator privileges.

```sh
sh
make
make test
make install PREFIX="$HOME/.local"
```

Without `make`:

```sh
./build.sh
```

## Usage

```sh
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

## Testing

```sh
make test
```

Runs the conformance, error, fuzz and benchmark suites. The C and Python
interpreters are compared byte for byte.

## Contributing

Contributions are welcome. Keep changes minimal and consistent with the
existing style, and run `make test` before submitting. See
[CONTRIBUTING.md](CONTRIBUTING.md).

## License

Released under the [MIT License](LICENSE).
