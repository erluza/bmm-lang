# .bmc Format

Version 2. Magic: `BMC\0` (4 bytes). All multi-byte integers are
little-endian.

Header:

- u16 version (2)
- u32 filename length, then filename bytes
- u16 nglobals
- u16 nfuncs
- u16 entry function index
- nglobals bytes of global-defined flags
- for each global: u16 name length followed by name bytes

Global slot 0 is reserved for `argv`.

Each function:

- null-terminated name
- u8 arity
- u16 nlocals
- u32 code length
- code bytes
- code-length i32 line numbers
- code-length i32 column numbers
- u16 constant count; each constant: u8 type (0 int, 1 str); int is i32;
  str is u16 length followed by bytes.

Trailer: u32 source length, source bytes, then u32 CRC-32 over all preceding
bytes.

Version 1 files are rejected. The source text and line information are stored
so runtime errors from `.bmc` files report the same file, line, column and
source line as direct execution.
