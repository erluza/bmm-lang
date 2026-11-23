<h1 align="center">B--</h1>

<p align="center">
  Un lenguaje de programación pequeño y completamente definido.<br>
  Intérprete de bytecode en C99 estricto. Implementación de referencia en Python. Sin dependencias.
</p>

<p align="center">
  <a href="https://github.com/erluza/bmm-lang/actions"><img alt="CI" src="https://github.com/erluza/bmm-lang/actions/workflows/ci.yml/badge.svg"></a>
  <img alt="Versión" src="https://img.shields.io/badge/version-1.0.0-blue">
  <img alt="Especificación" src="https://img.shields.io/badge/spec-frozen-informational">
  <a href="LICENSE"><img alt="Licencia: MIT" src="https://img.shields.io/badge/license-MIT-green"></a>
</p>

<p align="center">
  <a href="docs/LANGUAGE_REFERENCE.md">Referencia</a> ·
  <a href="docs/TUTORIAL.md">Tutorial</a> ·
  <a href="docs/BUILTINS.md">Built-ins</a> ·
  <a href="docs/ARCHITECTURE.md">Arquitectura</a> ·
  <a href="CHANGELOG.md">Changelog</a> ·
  <a href="README.md">English</a>
</p>

---

B-- (se pronuncia "be menos menos") tiene tres tipos, doce palabras reservadas
y un conjunto cerrado de funciones built-in. El código fuente se compila a
bytecode para una máquina virtual de pila, con un recolector de basura
mark-and-sweep. La especificación del lenguaje está congelada en la versión
1.0.0.

## Contenido

- [El lenguaje en breve](#el-lenguaje-en-breve)
- [Instalación](#instalación)
- [Uso](#uso)
- [Documentación](#documentación)
- [Estructura del repositorio](#estructura-del-repositorio)
- [Portabilidad](#portabilidad)
- [Pruebas](#pruebas)
- [Contribuir](#contribuir)
- [Licencia](#licencia)

## El lenguaje en breve

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
| **Tipos** | `int` (32 bits con signo, wraparound), `str` (secuencia de bytes inmutable), `arr` (mutable, indexado desde 0, con chequeo de límites) |
| **Palabras reservadas** | `let` `fn` `if` `else` `loop` `break` `ret` `use` `and` `or` `not` `end` |
| **Built-ins** | `print` `println` `input` `len` `push` `pop` `str` `int` `chr` `ord` `sub` `arr` `exit` `argv` `clock` |
| **Ejecución** | Bytecode, VM de pila, GC mark-and-sweep |

## Instalación

Requisitos: un compilador C99 y `make`. Ninguna otra dependencia y no se
requieren privilegios de administrador.

```sh
sh
make
make test
make install PREFIX="$HOME/.local"
```

Sin `make`:

```sh
./build.sh
```

## Uso

```sh
bmm hello.bmm                      # ejecutar un archivo fuente
bmm                                # iniciar el REPL
bmm build hello.bmm -o hello.bmc   # compilar a bytecode
bmm run hello.bmc                  # ejecutar bytecode compilado
```

`hello.bmm`:

```bmm
println("Hello, world")
```

## Documentación

| Tema | Documento |
|---|---|
| Lenguaje | [Manifiesto](docs/MANIFESTO.md) · [Referencia](docs/LANGUAGE_REFERENCE.md) · [Gramática](docs/GRAMMAR.ebnf) · [Built-ins](docs/BUILTINS.md) · [Errores](docs/ERRORS.md) |
| Aprendizaje | [Tutorial](docs/TUTORIAL.md) · [`examples/`](examples) |
| Internals | [Arquitectura](docs/ARCHITECTURE.md) · [Bytecode](docs/BYTECODE.md) · [Formato BMC](docs/BMC_FORMAT.md) · [Decisiones de diseño](docs/DECISIONS.md) |
| Integración | [Embedding](docs/EMBEDDING.md) · [Portabilidad](docs/PORTING.md) |

## Estructura del repositorio

| Ruta | Contenido |
|---|---|
| `src/` | Fuentes del intérprete y capa de abstracción de plataforma (PAL) |
| `ref/` | Implementación de referencia en Python |
| `docs/` | Especificación y guías |
| `examples/` | Quince programas de ejemplo |
| `stdlib/` | Biblioteca estándar, escrita en B-- |
| `tests/` | Pruebas de conformidad, errores, fuzzing y benchmarks |

## Portabilidad

Todo el acceso a plataforma está aislado en `src/pal/pal.h` y sus
implementaciones. Portar requiere implementar alrededor de diez funciones y
ajustar `bmm_config.h`. Ver [PORTING.md](docs/PORTING.md).

## Pruebas

```sh
make test
```

Ejecuta las suites de conformidad, errores, fuzzing y benchmarks. Los
intérpretes en C y en Python se comparan byte a byte.

## Contribuir

Las contribuciones son bienvenidas. Mantené los cambios mínimos y consistentes
con el estilo existente, y ejecutá `make test` antes de enviar. Ver
[CONTRIBUTING.md](CONTRIBUTING.md).

## Licencia

Publicado bajo la [licencia MIT](LICENSE).
