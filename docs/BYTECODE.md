# Bytecode

The VM is stack-based. Each instruction may take operands as shown.

| Opcode | Operands | Effect |
|--------|----------|--------|
| CONST | u16 | Push constant. |
| POP | | Pop top. |
| DUP | | Duplicate top. |
| LOAD_LOCAL | u16 | Push local slot. |
| STORE_LOCAL | u16 | Pop and store in local slot. |
| LOAD_GLOBAL | u16 | Push global. |
| STORE_GLOBAL | u16 | Pop and store in global (requires defined). |
| DECLARE_GLOBAL | u16 | Pop and declare global. |
| ADD/SUB/MUL/DIV/MOD | | Arithmetic on two ints. |
| NEG | | Negate int. |
| CONCAT | | Concatenate two strings. |
| EQ/NE | | Equality. |
| LT/GT/LE/GE | | Ordered comparison. |
| NOT | | Logical not. |
| JUMP | i16 | Unconditional jump. |
| JUMP_IF_FALSE | i16 | Pop; jump if false. |
| CALL | u16 u8 | Call function with argc. |
| RET | | Return. |
| RET_VAL | | Pop and return value. |
| ARRAY_NEW | u16 | Pop count values into a new array. |
| INDEX_GET | | Index into string or array. |
| INDEX_SET | | Pop value, index, array; assign. |
| CALL_BUILTIN | u8 u8 | Call builtin. |
| HALT | | Stop execution. |
