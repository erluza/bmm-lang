# B-- Language Reference

## 1. Lexical structure

Source files use the `.bmm` extension and UTF-8 encoding. Identifiers are
ASCII: `[A-Za-z_][A-Za-z0-9_]*`. Keywords are reserved and cannot be used
as identifiers:

```
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

```
let name = expr
name = expr
```

`let` declares a variable in the current block scope. Assignment updates an
existing variable or array element:

```
arr[i] = expr
```

## 4. Functions

```
fn name(param, param)
  ...
  ret value
end
```

Functions may be recursive. Parameters are passed by value; arrays are shared
by reference.

## 5. Control flow

```
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

`argv` is a value, not a call.

## 8. Modules

```
use "file.bmm"
```

Executes the referenced file in the current global environment.

## 9. Array literals

```
let a = [1, 2, 3]
let b = []
```

## 10. Runtime errors

Division by zero, out-of-range index, type mismatch, stack overflow, and
invalid conversions terminate the program with a non-zero status.
