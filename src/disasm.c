#include "disasm.h"
#include "opcodes.h"
#include "out.h"
#include "gc.h"
#include "pal/pal.h"
#include <string.h>
#include <stdint.h>

void disasm_vm(BmmVM *vm) {
    OutBuf o; out_init(&o);
    for (size_t fi = 0; fi < vm->nfuncs; fi++) {
        Func *f = vm->funcs[fi];
        out_puts(&o, "func ");
        out_int(&o, (long)fi);
        out_putc(&o, ' ');
        out_puts(&o, f->name ? f->name : "?");
        out_puts(&o, " arity=");
        out_int(&o, f->arity);
        out_puts(&o, " locals=");
        out_int(&o, f->nlocals);
        out_putc(&o, '\n');
        size_t ip = 0;
        while (ip < f->code_len) {
            out_puts(&o, "  ");
            /* 4-digit ip */
            char buf[8];
            /* simple zero-pad */
            int v = (int)ip;
            buf[4] = '\0';
            for (int i = 3; i >= 0; i--) { buf[i] = '0' + (v % 10); v /= 10; }
            out_puts(&o, buf);
            out_putc(&o, ' ');
            unsigned char op = f->code[ip++];
            switch (op) {
                case OP_CONST: {
                    unsigned int idx = f->code[ip] | (f->code[ip+1]<<8); ip+=2;
                    out_puts(&o, "CONST ");
                    out_int(&o, idx);
                    if (idx < f->nconsts) {
                        Value v = f->consts[idx];
                        if (v.type == VAL_INT) { out_puts(&o, " (int "); out_int(&o, v.as.i); out_putc(&o, ')'); }
                        else if (v.type == VAL_STR) { out_puts(&o, " (str \""); out_puts(&o, ((ObjString*)v.as.o)->chars); out_puts(&o, "\")"); }
                    }
                    out_putc(&o, '\n');
                    break;
                }
                case OP_POP: out_puts(&o, "POP\n"); break;
                case OP_DUP: out_puts(&o, "DUP\n"); break;
                case OP_LOAD_LOCAL: { unsigned int s = f->code[ip] | (f->code[ip+1]<<8); ip+=2; out_puts(&o, "LOAD_LOCAL "); out_int(&o, s); out_putc(&o, '\n'); break; }
                case OP_STORE_LOCAL: { unsigned int s = f->code[ip] | (f->code[ip+1]<<8); ip+=2; out_puts(&o, "STORE_LOCAL "); out_int(&o, s); out_putc(&o, '\n'); break; }
                case OP_LOAD_GLOBAL: { unsigned int g = f->code[ip] | (f->code[ip+1]<<8); ip+=2; out_puts(&o, "LOAD_GLOBAL "); out_int(&o, g); out_putc(&o, '\n'); break; }
                case OP_STORE_GLOBAL: { unsigned int g = f->code[ip] | (f->code[ip+1]<<8); ip+=2; out_puts(&o, "STORE_GLOBAL "); out_int(&o, g); out_putc(&o, '\n'); break; }
                case OP_DECLARE_GLOBAL: { unsigned int g = f->code[ip] | (f->code[ip+1]<<8); ip+=2; out_puts(&o, "DECLARE_GLOBAL "); out_int(&o, g); out_putc(&o, '\n'); break; }
                case OP_ADD: out_puts(&o, "ADD\n"); break;
                case OP_SUB: out_puts(&o, "SUB\n"); break;
                case OP_MUL: out_puts(&o, "MUL\n"); break;
                case OP_DIV: out_puts(&o, "DIV\n"); break;
                case OP_MOD: out_puts(&o, "MOD\n"); break;
                case OP_NEG: out_puts(&o, "NEG\n"); break;
                case OP_CONCAT: out_puts(&o, "CONCAT\n"); break;
                case OP_EQ: out_puts(&o, "EQ\n"); break;
                case OP_NE: out_puts(&o, "NE\n"); break;
                case OP_LT: out_puts(&o, "LT\n"); break;
                case OP_GT: out_puts(&o, "GT\n"); break;
                case OP_LE: out_puts(&o, "LE\n"); break;
                case OP_GE: out_puts(&o, "GE\n"); break;
                case OP_AND: out_puts(&o, "AND\n"); break;
                case OP_OR: out_puts(&o, "OR\n"); break;
                case OP_NOT: out_puts(&o, "NOT\n"); break;
                case OP_JUMP: { unsigned int off = (unsigned int)(f->code[ip]) | ((unsigned int)(f->code[ip+1])<<8) | ((unsigned int)(f->code[ip+2])<<16) | ((unsigned int)(f->code[ip+3])<<24); ip+=4; out_puts(&o, "JUMP "); out_int(&o, (int)off); out_putc(&o, '\n'); break; }
                case OP_JUMP_IF_FALSE: { unsigned int off = (unsigned int)(f->code[ip]) | ((unsigned int)(f->code[ip+1])<<8) | ((unsigned int)(f->code[ip+2])<<16) | ((unsigned int)(f->code[ip+3])<<24); ip+=4; out_puts(&o, "JUMP_IF_FALSE "); out_int(&o, (int)off); out_putc(&o, '\n'); break; }
                case OP_CALL: { unsigned int fi2 = f->code[ip] | (f->code[ip+1]<<8); ip+=2; unsigned char argc = f->code[ip++]; out_puts(&o, "CALL "); out_int(&o, fi2); out_putc(&o, ' '); out_int(&o, argc); out_putc(&o, '\n'); break; }
                case OP_RET: out_puts(&o, "RET\n"); break;
                case OP_RET_VAL: out_puts(&o, "RET_VAL\n"); break;
                case OP_ARRAY_NEW: { unsigned int cnt = f->code[ip] | (f->code[ip+1]<<8); ip+=2; out_puts(&o, "ARRAY_NEW "); out_int(&o, cnt); out_putc(&o, '\n'); break; }
                case OP_INDEX_GET: out_puts(&o, "INDEX_GET\n"); break;
                case OP_INDEX_SET: out_puts(&o, "INDEX_SET\n"); break;
                case OP_CALL_BUILTIN: { unsigned char bi = f->code[ip++]; unsigned char argc = f->code[ip++]; out_puts(&o, "CALL_BUILTIN "); out_int(&o, bi); out_putc(&o, ' '); out_int(&o, argc); out_putc(&o, '\n'); break; }
                case OP_HALT: out_puts(&o, "HALT\n"); break;
                default: out_puts(&o, "UNKNOWN "); out_int(&o, op); out_putc(&o, '\n'); break;
            }
        }
    }
    out_flush_stdout(&o);
}
