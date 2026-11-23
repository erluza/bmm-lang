# .bmc Format

Magic: `BMC\0` (4 bytes). Version: u16 (1). Then u16 nglobals, u16 nfuncs,
u16 entry index. Then each function:

- null-terminated name
- u8 arity
- u16 nlocals
- u32 code length
- code bytes
- u16 constant count; each constant: u8 type (0 int, 1 str); int is i32
  little-endian; str is u16 length followed by bytes.

Trailer: u32 CRC-32 over all preceding bytes. All multi-byte integers are
little-endian.
