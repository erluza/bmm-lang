# B-- Language Reference

## 1. Lexical structure

Source files use the `.bmm` extension and UTF-8 encoding. Identifiers are
ASCII: `[A-Za-z_][A-Za-z0-9_]*`. Keywords are reserved and cannot be used
as identifiers:

```text noexec: example output or prose
let fn if else loop break ret use and or not end
```

Whitespace (space, tab, carriage return, newline) separates tokens. A newline
ends a statement. Semicolons are optional and ignored. Comments begin with
`--` and run to the end of the line.

Number literals are non-negative decimal integers. String literals use double
quotes with the escapes `\n`, `\t`, `\r`, `\\`, and `\"`.

## 2. Types

### 2.1 int

A 32-bit signed integer in two's complement. Arithmetic wraps on overflow.
Zero is false; any other value is true.

### 2.2 str

An immutable sequence of bytes with an explicit length. Strings are valid
UTF-8; `len` returns the byte length.

### 2.3 arr

A mutable, zero-indexed, dynamically sized array of values. Any value type
may be stored. Bounds are checked on every access.

## 3. Variables

```text noexec: example output or prose
let name = expr
name = expr
```

`let` declares a variable in the current block scope. Assignment updates an
existing variable or array element:

```text noexec: example output or prose
arr[i] = expr
```

## 4. Functions

```text noexec: example output or prose
fn name(param, param)
  ...
  ret value
end
```

Functions may be recursive. Parameters are passed by value; arrays are shared
by reference.

## 5. Control flow

```text noexec: example output or prose
if cond
  ...
else
  ...
end

loop
  ...
  break
end

loop cond
  ...
end
```

## 6. Operators

Precedence, highest to lowest:

1. primary: literals, names, calls, indexing, grouping, array literals
2. unary: `-`, `+`, `not`
3. `*`, `/`, `%`
4. `+`, `-`
5. `..` (concatenation, strings only)
6. `==`, `!=`, `<`, `>`, `<=`, `>=`
7. `and`
8. `or`

`and` and `or` short-circuit. `not` is prefix.

## 7. Built-in functions

| Name | Signature | Description |
|------|-----------|-------------|
| print | print(x) | Print x without a trailing newline. |
| println | println(x) | Print x with a trailing newline. |
| input | input() | Read one line from stdin. |
| len | len(x) | Length of a str or arr. |
| push | push(a, v) | Append v to array a. |
| pop | pop(a) | Remove and return the last element of a. |
| str | str(x) | Convert int or str to str. |
| int | int(x) | Convert str or int to int. |
| chr | chr(n) | Code point to UTF-8 string. |
| ord | ord(s) | Single character to code point. |
| sub | sub(x, start, len) | Substring or sub-array. |
| arr | arr() | New empty array. |
| exit | exit(n) | Terminate with status n. |
| argv | argv | Array of program arguments. |
| clock | clock() | Milliseconds since start. |

`argv` is a value, not a call. Use `len(argv)` and `argv[i]`. Writing
`argv()` is a compile-time error: `'argv' is not a function`.

## 8. Modules

```text noexec: example output or prose
use "file.bmm"
```

Executes the referenced file in the current global environment.

## 9. Array literals

```text noexec: example output or prose
let a = [1, 2, 3]
let b = []
```

## 10. Runtime errors

Division by zero, out-of-range index, type mismatch, stack overflow, and
invalid conversions terminate the program with a non-zero status.

## 11. Numbered rules

The following rules are normative and are mapped to tests in
`tests/conformance/INDEX.md`.

R001 `int` is 32-bit signed and arithmetic wraps on overflow.
R002 Integer division truncates toward zero.
R003 The sign of `%` follows the dividend.
R004 Division or modulo by zero is a runtime error.
R005 `str` is immutable and `len` returns its byte length.
R006 `arr` is mutable, zero-indexed and bounds-checked.
R007 Out-of-range string or array indexing is a runtime error.
R008 `let` declares a variable in the current block and requires an initializer.
R009 Assignment updates an existing variable or array element.
R010 `if` selects a branch by the zero/non-zero value of an int.
R011 `loop` and `loop cond` repeat; `break` exits the innermost loop.
R012 Functions are declared with `fn`, may recurse, and return with `ret`.
R013 Scope is lexical by block; globals are declared at file level.
R014 `and` and `or` short-circuit; `not` is prefix; all return 0 or 1.
R015 Operator precedence is primary, unary, `* / %`, `+ -`, `..`, comparisons, `and`, `or`.
R016 `..` concatenates strings only.
R017 Comparisons return 0 or 1; `==` compares array identity.
R018 `use "file"` executes another source file in the global environment.
R019 Comments start with `--` and run to the end of the line.
R020 Semicolons are optional statement separators.
R021 String escapes are `\n`, `\t`, `\r`, `\\` and `\"`.
R022 Built-ins are the closed set listed in section 7.
R023 `argv` is an array value, not a function.
R024 `clock()` is monotonic and non-negative.
R025 Runtime and compile-time errors exit with status 1.
