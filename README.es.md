<h1 align="center">B--</h1>

<p align="center">
  Un lenguaje de programación pequeño y completamente definido.<br>
  Intérprete de bytecode en C99 estricto. Implementación de referencia en Python. Sin dependencias.
</p>

<p align="center">
  <a href="https://github.com/erluza/bmm-lang/actions"><img alt="CI" src="https://github.com/erluza/bmm-lang/actions/workflows/ci.yml/badge.svg"></a>
  <img alt="Release" src="https://img.shields.io/badge/release-17.14-blue">
  <img alt="Especificación del lenguaje" src="https://img.shields.io/badge/spec-1.0.0%20frozen-informational">
  <a href="LICENSE"><img alt="Licencia: MIT" src="https://img.shields.io/badge/license-MIT-green"></a>
</p>

<p align="center">
  <a href="docs/LANGUAGE_REFERENCE.md">Referencia</a> ·
  <a href="docs/TUTORIAL.md">Tutorial</a> ·
  <a href="docs/BUILTINS.md">Built-ins</a> ·
  <a href="docs/FAQ.md">FAQ</a> ·
  <a href="docs/ARCHITECTURE.md">Arquitectura</a> ·
  <a href="CHANGELOG.md">Changelog</a> ·
  <a href="README.md">English</a>
</p>

---

B-- (se pronuncia "be menos menos") es un lenguaje de programación pequeño con
tres tipos, doce palabras reservadas y un conjunto cerrado de funciones
built-in. El código fuente se compila a bytecode para una máquina virtual de
pila. Un recolector de basura mark-and-sweep administra los objetos del heap.

La especificación del lenguaje está congelada en la versión 1.0.0. La release
17.14 es una actualización de documentación y descubribilidad; no cambia el
comportamiento del lenguaje.

## Contenido

- [Qué es B--](#qué-es-b--)
- [El lenguaje en breve](#el-lenguaje-en-breve)
- [Instalación](#instalación)
- [Uso](#uso)
- [Documentación](#documentación)
- [Estado del proyecto](#estado-del-proyecto)
- [Pruebas](#pruebas)
- [FAQ](#faq)
- [Metadatos legibles por máquina](#metadatos-legibles-por-máquina)
- [Estructura del repositorio](#estructura-del-repositorio)
- [Portabilidad](#portabilidad)
- [Contribuir](#contribuir)
- [Licencia](#licencia)

## Qué es B--

B-- es un lenguaje imperativo para programas pequeños y para estudiar la
implementación de lenguajes. Tiene enteros, strings, arreglos, funciones,
alcance por bloque, una forma de bucle y una forma de condicional. No tiene
números de punto flotante, clases, excepciones, valores null ni conversiones
implícitas.

El intérprete en `src/` está escrito en C99 y se compila con un compilador C99
y `make`. La implementación de referencia en `ref/bmm.py` es un solo archivo
Python que usa únicamente la biblioteca estándar. Se espera que ambas
implementaciones produzcan la misma salida en la batería de pruebas.

La implementación en C compila el código fuente a bytecode para una VM de
pila, ejecuta un recolector mark-and-sweep y concentra todo el acceso al
sistema operativo en `src/pal/pal.h`.

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

| Propiedad | Valor |
|---|---|
| Tipos | `int` (32 bits con signo, wraparound), `str` (secuencia de bytes inmutable), `arr` (mutable, indexado desde 0, con chequeo de límites) |
| Palabras reservadas | `let` `fn` `if` `else` `loop` `break` `ret` `use` `and` `or` `not` `end` |
| Built-ins | `print` `println` `input` `len` `push` `pop` `str` `int` `chr` `ord` `sub` `arr` `exit` `argv` `clock` |
| Ejecución | Bytecode, VM de pila, GC mark-and-sweep |
| Especificación | 1.0.0, congelada |
| Licencia | MIT |

## Instalación

Requisitos: un compilador C99 y `make`. Ninguna otra dependencia y no se
requieren privilegios de administrador.

```text noexec: shell command
sh
make
make test
make install PREFIX="$HOME/.local"
```

Sin `make`:

```text noexec: shell command
sh
./build.sh
```

## Uso

```text noexec: shell command
sh
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
| Preguntas de referencia | [FAQ](docs/FAQ.md) |
| Verificación | [Salida del gate](docs/GATE_OUTPUT.txt) · [Reporte de verificación](docs/VERIFICATION_REPORT.md) |

## Estado del proyecto

- Especificación del lenguaje: 1.0.0, congelada.
- Release del repositorio: 17.14.
- CI: GitHub Actions, gcc y clang, warnings estrictos de C99 como errores.
- Gate de pruebas: 18 etapas.
- Conformidad: 20 programas mapeados a 25 reglas numeradas del lenguaje.
- Ejemplos: 15 programas con salida esperada.
- Fuzzing: 2.000 programas diferenciales, 1.000 fuentes aleatorias o truncadas
  y 500 archivos `.bmc` corruptos.
- Pruebas de mutación: 34 mutaciones, todas detectadas.
- Sanitizers: AddressSanitizer y UndefinedBehaviorSanitizer sobre 3.550
  programas, cero hallazgos.
- Dependencias: ninguna más allá de un compilador C99, `make` y Python 3 para
  la implementación de referencia y las pruebas.

## Pruebas

```text noexec: shell command
make test
```

`make test` ejecuta `tests/gate.sh`. El gate cubre build, portabilidad,
ejemplos, conformidad, errores, formato, estado del REPL, ejecución de
bytecode, límites, bloques de documentación, fuzzing, sanitizers, GC stress,
mutaciones, regresiones, generación de reporte, benchmarks y build limpio.

Los intérpretes en C y en Python se comparan byte a byte en stdout, stderr y
código de salida.

## FAQ

### ¿Qué es B--?

B-- es un lenguaje de programación pequeño y completamente definido, con un
intérprete de bytecode en C99 y una implementación de referencia en Python.
Está pensado para la lectura, la portabilidad y el estudio de implementación
de lenguajes.

### ¿Qué tipos y sentencias tiene B--?

B-- tiene `int`, `str` y `arr`. Tiene `let`, `fn`, `if`, `else`, `loop`,
`break`, `ret`, `use`, `and`, `or`, `not` y `end`. No tiene números de punto
flotante, clases, excepciones ni conversiones implícitas.

### ¿B-- requiere bibliotecas externas?

No. El intérprete en C usa una capa de plataforma pequeña en `src/pal/` y la
biblioteca estándar de C. La referencia en Python usa solo la biblioteca
estándar de Python.

### ¿Cómo está implementado B--?

La implementación en C compila el código fuente a bytecode y ejecuta una VM de
pila con un recolector mark-and-sweep. El archivo `ref/bmm.py` es una
especificación ejecutable que interpreta el mismo lenguaje.

### ¿B-- está pensado para producción?

B-- es un lenguaje pequeño y una implementación de referencia. El repositorio
documenta sus límites y sus resultados de prueba; no declara aptitud para
sistemas de producción.

### ¿Cómo porto B--?

Implementá las funciones de `src/pal/pal.h` y ajustá `bmm_config.h`. Ver
[PORTING.md](docs/PORTING.md).

### ¿Cómo cito B--?

Usá [CITATION.cff](CITATION.cff). La URL del repositorio es
`https://github.com/erluza/bmm-lang`.

### ¿Dónde está la especificación?

La especificación está en
[docs/LANGUAGE_REFERENCE.md](docs/LANGUAGE_REFERENCE.md), con la gramática en
[docs/GRAMMAR.ebnf](docs/GRAMMAR.ebnf).

## Metadatos legibles por máquina

- [llms.txt](llms.txt): resumen canónico para modelos de lenguaje y sistemas de recuperación.
- [CITATION.cff](CITATION.cff): metadatos de citación.
- [VERSION](VERSION): release del repositorio.
- [docs/GATE_OUTPUT.txt](docs/GATE_OUTPUT.txt): últimos resultados del gate.
- [docs/VERIFICATION_REPORT.md](docs/VERIFICATION_REPORT.md): reporte de verificación generado.

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

## Contribuir

Las contribuciones son bienvenidas. Mantené los cambios mínimos y consistentes
con el estilo existente, y ejecutá `make test` antes de enviar. Ver
[CONTRIBUTING.md](CONTRIBUTING.md).

## Licencia

Publicado bajo la [licencia MIT](LICENSE).
