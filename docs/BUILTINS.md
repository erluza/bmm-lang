# Built-in Functions

| Name | Signature | Description |
|------|-----------|-------------|
| print | print(x) | Print a value without a trailing newline. |
| println | println(x) | Print a value with a trailing newline. |
| input | input() | Read one line from standard input. |
| len | len(x) | Length of a string (bytes) or array. |
| push | push(a, v) | Append a value to an array. |
| pop | pop(a) | Remove and return the last element. |
| str | str(x) | Convert an int or str to a string. |
| int | int(x) | Convert a str or int to an int. |
| chr | chr(n) | Code point to a UTF-8 string. |
| ord | ord(s) | Single character to a code point. |
| sub | sub(x, start, length) | Substring or sub-array. |
| arr | arr() | A new empty array. |
| exit | exit(n) | Terminate with status n. |
| argv | argv | Array of program arguments (a value, not a call). |
| clock | clock() | Milliseconds since interpreter start. |

`argv` is a value rather than a call. Use `len(argv)` and `argv[i]`;
`argv()` is a compile-time error. `print` and `println` accept one argument;
convert with `str` or concatenate with `..`.
