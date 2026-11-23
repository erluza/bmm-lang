# B--

B-- (pronounced "be minus minus") is a small, fully defined programming
language. It has three types, twelve keywords, and a closed set of built-in
functions. The reference interpreter is written in strict C99 with no
dependencies.

## Status

Version 1.0.0. The language specification is frozen.

## Overview

Types: `int` (32-bit signed, wraparound), `str` (immutable byte sequence),
`arr` (mutable, zero-indexed, bounds-checked).

Keywords: `let fn if else loop break ret use and or not end`.

Execution model: source is compiled to bytecode for a stack-based virtual
machine. A mark-and-sweep collector manages memory.

## Requirements

A C99 compiler and `make`. No other dependencies. No administrator
privileges are required.

## Building and Installation

```
make
make test
make install PREFIX="$HOME/.local"
```

Alternative without make:

```
./build.sh
```

## Quick Start

Hello world:

```
println("Hello, world")
```

Run:

```
bmm hello.bmm
```

REPL:

```
bmm
```

Compile to bytecode:

```
bmm build hello.bmm -o hello.bmc
bmm run hello.bmc
```

## Language at a Glance

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

Built-ins: `print println input len push pop str int chr ord sub arr exit argv clock`.

## Repository Layout

```
src/        interpreter sources and PAL
ref/        Python reference implementation
docs/       specification and guides
examples/   fifteen example programs
stdlib/     standard library written in B--
tests/      conformance, errors, fuzz, benchmarks
```

## Documentation

- `docs/MANIFESTO.md`
- `docs/LANGUAGE_REFERENCE.md`
- `docs/GRAMMAR.ebnf`
- `docs/TUTORIAL.md`
- `docs/BUILTINS.md`
- `docs/BYTECODE.md`
- `docs/BMC_FORMAT.md`
- `docs/EMBEDDING.md`
- `docs/PORTING.md`
- `docs/ERRORS.md`
- `docs/ARCHITECTURE.md`
- `docs/DECISIONS.md`

## Portability

All platform access is isolated in `src/pal/pal.h` and its implementations.
Porting requires implementing roughly ten functions and adjusting
`bmm_config.h`.

## Testing

```
make test
```

Runs conformance, error, fuzz, and benchmark tests. The C and Python
interpreters are compared byte for byte.

## Contributing

Contributions are welcome. Keep changes minimal and consistent with the
existing style. Run `make test` before submitting.

## License

MIT.

---

Resumen en español: B-- es un lenguaje mínimo y totalmente definido, con
intérprete en C99 e implementación de referencia en Python. Incluye
documentación, ejemplos, tests y capa de portabilidad. Se compila con
`make` y se instala con `make install PREFIX="$HOME/.local"`.
