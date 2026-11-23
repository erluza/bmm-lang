#ifndef OPCODES_H
#define OPCODES_H

typedef enum {
    OP_CONST = 0,        /* u16 const index */
    OP_POP,
    OP_DUP,
    OP_LOAD_LOCAL,       /* u16 slot */
    OP_STORE_LOCAL,      /* u16 slot */
    OP_LOAD_GLOBAL,      /* u16 index */
    OP_STORE_GLOBAL,     /* u16 index (assignment, requires defined) */
    OP_DECLARE_GLOBAL,   /* u16 index (let) */
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_NEG,
    OP_CONCAT,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_GT,
    OP_LE,
    OP_GE,
    OP_AND,
    OP_OR,
    OP_NOT,
    OP_JUMP,             /* i16 offset */
    OP_JUMP_IF_FALSE,    /* i16 offset, pops condition */
    OP_CALL,             /* u16 func index, u8 argc */
    OP_RET,
    OP_RET_VAL,
    OP_ARRAY_NEW,        /* u16 count */
    OP_INDEX_GET,
    OP_INDEX_SET,
    OP_PUSH_VAL,         /* builtin push handled as OP_CALL_BUILTIN? use builtin op */
    OP_CALL_BUILTIN,     /* u8 builtin id, u8 argc */
    OP_EXIT,
    OP_HALT,
    OP_PRINT,            /* (unused, via builtin) */
    OP_MAX
} OpCode;

/* builtin ids */
#define BI_PRINT 0
#define BI_PRINTLN 1
#define BI_INPUT 2
#define BI_LEN 3
#define BI_PUSH 4
#define BI_POP 5
#define BI_STR 6
#define BI_INT 7
#define BI_CHR 8
#define BI_ORD 9
#define BI_SUB 10
#define BI_ARR 11
#define BI_EXIT 12
#define BI_ARGV 13  /* not a call; handled as global */
#define BI_CLOCK 14

#endif
