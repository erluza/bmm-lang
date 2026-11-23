# Errors

## Compile-time errors

Reported as:

```
bmm: <file>:<line>:<col>: error: <message>
  <source line>
          ^
```

Examples: unexpected character, expected `end`, bad escape.

## Runtime errors

Same format, then a non-zero exit status. The following conditions are
runtime errors:

- division or modulo by zero
- index out of range
- type mismatch
- undefined variable
- arity mismatch
- stack overflow (frame depth exceeded)
- invalid integer conversion
- empty array pop
- step limit exceeded (`--max-steps`)

## Exit codes

| Code | Meaning |
|------|---------|
| 0 | success |
| 1 | runtime or compile-time error |
| 2 | command-line usage error |

`exit(n)` terminates with `n` reduced modulo 256 to the range 0..255.
