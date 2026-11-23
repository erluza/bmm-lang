#include "vm.h"
#include "gc.h"
#include "pal/pal.h"
#include <string.h>
#include <stdint.h>

BmmVM *vm_new(void) {
    BmmVM *vm = bmm_alloc(sizeof(BmmVM));
    memset(vm, 0, sizeof(BmmVM));
    vm->stack_cap = BMM_STACK_SIZE;
    vm->stack = bmm_alloc(vm->stack_cap * sizeof(Value));
    vm->frame_cap = BMM_MAX_FRAMES;
    vm->frames = bmm_alloc(vm->frame_cap * sizeof(Frame));
    return vm;
}

void vm_free(BmmVM *vm) {
    if (!vm) return;
    for (size_t i = 0; i < vm->fp; i++) {
        if (vm->frames[i].locals) bmm_free(vm->frames[i].locals);
    }
    bmm_free(vm->stack);
    bmm_free(vm->frames);
    bmm_free(vm->globals);
    bmm_free(vm->global_defined);
    if (vm->owns_global_names) {
        for (size_t i = 0; i < vm->nglobals; i++) bmm_free(vm->global_names ? vm->global_names[i] : NULL);
        bmm_free(vm->global_names);
    }
    bmm_free(vm->error_msg);
    bmm_free(vm->source);
    bmm_free(vm->source_name);
    /* funcs owned by compiler; not freed here */
    bmm_free(vm);
}

void vm_push(BmmVM *vm, Value v) {
    if (vm->sp >= vm->stack_cap) { vm_error(vm, "stack overflow", 0, 0); return; }
    vm->stack[vm->sp++] = v;
}

Value vm_pop(BmmVM *vm) {
    if (vm->sp == 0) { vm_error(vm, "stack underflow", 0, 0); Value v; v.type=VAL_INT; v.as.i=0; return v; }
    return vm->stack[--vm->sp];
}

void vm_error(BmmVM *vm, const char *msg, int line, int col) {
    if (vm->had_error) return;
    vm->had_error = 1;
    size_t _m = strlen(msg);
    vm->error_msg = bmm_alloc(_m + 1);
    memcpy(vm->error_msg, msg, _m + 1);
    vm->error_line = line;
    vm->error_col = col;
}

/* helper to get current frame */
#define CUR (vm->frames[vm->fp-1])

int vm_run(BmmVM *vm, int entry_func) {
    gc_set_vm(vm);
    /* init first frame */
    if (entry_func < 0 || entry_func >= (int)vm->nfuncs) { vm_error(vm, "bad entry", 0, 0); return -1; }
    Frame *f = &vm->frames[vm->fp++];
    f->func = vm->funcs[entry_func];
    f->ip = 0;
    f->locals = bmm_alloc(f->func->nlocals * sizeof(Value));
    for (int i = 0; i < f->func->nlocals; i++) f->locals[i] = val_int(0);
    vm->steps = 0;

    while (vm->fp > 0 && !vm->had_error) {
        if (vm->max_steps > 0) {
            vm->steps++;
            if (vm->steps > vm->max_steps) { vm_error(vm, "step limit exceeded", 0, 0); break; }
        }
        Frame *fr = &vm->frames[vm->fp-1];
        if (fr->ip >= (int)fr->func->code_len) {
            /* implicit return */
            vm->fp--;
            if (fr->locals) bmm_free(fr->locals);
            continue;
        }
        size_t ip0 = fr->ip;
        unsigned char op = fr->func->code[fr->ip++];
        int line = 0, col = 0;
        if (fr->func->lineinfo) { line = fr->func->lineinfo[ip0]; col = fr->func->colinfo ? fr->func->colinfo[ip0] : 0; }
        switch (op) {
            case OP_CONST: {
                unsigned int idx = fr->func->code[fr->ip] | (fr->func->code[fr->ip+1] << 8);
                fr->ip += 2;
                if (idx >= fr->func->nconsts) { vm_error(vm, "bad const", line, col); break; }
                vm_push(vm, fr->func->consts[idx]);
                break;
            }
            case OP_POP: {
                vm_pop(vm);
                break;
            }
            case OP_DUP: {
                if (vm->sp == 0) { vm_error(vm, "stack underflow", line, col); break; }
                vm_push(vm, vm->stack[vm->sp-1]);
                break;
            }
            case OP_LOAD_LOCAL: {
                unsigned int slot = fr->func->code[fr->ip] | (fr->func->code[fr->ip+1] << 8);
                fr->ip += 2;
                if (slot >= (unsigned int)fr->func->nlocals) { vm_error(vm, "bad local", line, col); break; }
                vm_push(vm, fr->locals[slot]);
                break;
            }
            case OP_STORE_LOCAL: {
                unsigned int slot = fr->func->code[fr->ip] | (fr->func->code[fr->ip+1] << 8);
                fr->ip += 2;
                if (vm->sp == 0) { vm_error(vm, "stack underflow", line, col); break; }
                Value v = vm_pop(vm);
                if (slot >= (unsigned int)fr->func->nlocals) { vm_error(vm, "bad local", line, col); break; }
                fr->locals[slot] = v;
                vm_push(vm, v);
                break;
            }
            case OP_LOAD_GLOBAL: {
                unsigned int idx = fr->func->code[fr->ip] | (fr->func->code[fr->ip+1] << 8);
                fr->ip += 2;
                if (idx >= vm->nglobals) { vm_error(vm, "bad global", line, col); break; }
                if (vm->global_defined && !vm->global_defined[idx]) {
                    char msg[256];
                    if (vm->global_names && vm->global_names[idx]) {
                        char *gn = vm->global_names[idx];
                        int mi = 0;
                        /* "undefined variable '" */
                        const char *prefix = "undefined variable '";
                        while (prefix[mi]) { msg[mi] = prefix[mi]; mi++; }
                        for (char *q = gn; *q; q++) msg[mi++] = *q;
                        msg[mi++] = '\'';
                        msg[mi] = '\0';
                    } else {
                        strcpy(msg, "undefined variable");
                    }
                    vm_error(vm, msg, line, col); break;
                }
                vm_push(vm, vm->globals[idx]);
                break;
            }
            case OP_STORE_GLOBAL: {
                unsigned int idx = fr->func->code[fr->ip] | (fr->func->code[fr->ip+1] << 8);
                fr->ip += 2;
                if (vm->sp == 0) { vm_error(vm, "stack underflow", line, col); break; }
                Value v = vm_pop(vm);
                if (idx >= vm->nglobals) { vm_error(vm, "bad global", line, col); break; }
                if (vm->global_defined && !vm->global_defined[idx]) {
                    char msg[256];
                    if (vm->global_names && vm->global_names[idx]) {
                        char *gn = vm->global_names[idx];
                        int mi = 0;
                        /* "undefined variable '" */
                        const char *prefix = "undefined variable '";
                        while (prefix[mi]) { msg[mi] = prefix[mi]; mi++; }
                        for (char *q = gn; *q; q++) msg[mi++] = *q;
                        msg[mi++] = '\'';
                        msg[mi] = '\0';
                    } else {
                        strcpy(msg, "undefined variable");
                    }
                    vm_error(vm, msg, line, col); break;
                }
                vm->globals[idx] = v;
                if (vm->global_defined) vm->global_defined[idx] = 1;
                vm_push(vm, v);
                break;
            }
            case OP_DECLARE_GLOBAL: {
                unsigned int idx = fr->func->code[fr->ip] | (fr->func->code[fr->ip+1] << 8);
                fr->ip += 2;
                if (vm->sp == 0) { vm_error(vm, "stack underflow", line, col); break; }
                Value v = vm_pop(vm);
                if (idx >= vm->nglobals) { vm_error(vm, "bad global", line, col); break; }
                vm->globals[idx] = v;
                if (vm->global_defined) vm->global_defined[idx] = 1;
                vm_push(vm, v);
                break;
            }
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD: {
                if (vm->sp < 2) { vm_error(vm, "stack underflow", line, col); break; }
                Value rv = vm_pop(vm);
                Value lv = vm_pop(vm);
                if (lv.type != VAL_INT || rv.type != VAL_INT) { vm_error(vm, "expected int", line, col); break; }
                int32_t a = lv.as.i, b = rv.as.i, r = 0;
                if (op == OP_ADD) r = (int32_t)((uint32_t)a + (uint32_t)b);
                else if (op == OP_SUB) r = (int32_t)((uint32_t)a - (uint32_t)b);
                else if (op == OP_MUL) r = (int32_t)((uint32_t)a * (uint32_t)b);
                else if (op == OP_DIV) {
                    if (b == 0) { vm_error(vm, "division by zero", line, col); break; }
                    if (a == INT32_MIN && b == -1) r = INT32_MIN;
                    else r = a / b; /* C trunc toward zero, safe here */
                } else if (op == OP_MOD) {
                    if (b == 0) { vm_error(vm, "division by zero", line, col); break; }
                    if (a == INT32_MIN && b == -1) r = 0;
                    else r = a % b;
                }
                vm_push(vm, val_int(r));
                break;
            }
            case OP_NEG: {
                if (vm->sp == 0) { vm_error(vm, "stack underflow", line, col); break; }
                Value v = vm_pop(vm);
                if (v.type != VAL_INT) { vm_error(vm, "expected int", line, col); break; }
                if (v.as.i == INT32_MIN) vm_push(vm, val_int(INT32_MIN));
                else vm_push(vm, val_int(-v.as.i));
                break;
            }
            case OP_CONCAT: {
                if (vm->sp < 2) { vm_error(vm, "stack underflow", line, col); break; }
                Value lv = vm->stack[vm->sp-2];
                Value rv = vm->stack[vm->sp-1];
                if (lv.type != VAL_STR || rv.type != VAL_STR) { vm_error(vm, "expected str", line, col); break; }
                ObjString *ls = (ObjString*)lv.as.o;
                ObjString *rs = (ObjString*)rv.as.o;
                ObjString *ns = new_string(NULL, ls->len + rs->len);
                memcpy(ns->chars, ls->chars, ls->len);
                memcpy(ns->chars + ls->len, rs->chars, rs->len);
                ns->chars[ns->len] = '\0';
                vm->sp -= 2;
                vm_push(vm, val_str((Obj*)ns));
                break;
            }
            case OP_EQ: case OP_NE: {
                if (vm->sp < 2) { vm_error(vm, "stack underflow", line, col); break; }
                Value rv = vm_pop(vm);
                Value lv = vm_pop(vm);
                int eq = 0;
                if (lv.type == VAL_INT && rv.type == VAL_INT) eq = (lv.as.i == rv.as.i);
                else if (lv.type == VAL_STR && rv.type == VAL_STR) {
                    ObjString *a = (ObjString*)lv.as.o;
                    ObjString *b = (ObjString*)rv.as.o;
                    eq = (a->len == b->len && memcmp(a->chars, b->chars, a->len) == 0);
                } else if (lv.type == VAL_ARR && rv.type == VAL_ARR) {
                    eq = (lv.as.o == rv.as.o);
                }
                int res = (op == OP_EQ) ? (eq ? 1 : 0) : (eq ? 0 : 1);
                vm_push(vm, val_int(res));
                break;
            }
            case OP_LT: case OP_GT: case OP_LE: case OP_GE: {
                if (vm->sp < 2) { vm_error(vm, "stack underflow", line, col); break; }
                Value rv = vm_pop(vm);
                Value lv = vm_pop(vm);
                if (lv.type != VAL_INT || rv.type != VAL_INT) { vm_error(vm, "expected int", line, col); break; }
                int res = 0;
                if (op == OP_LT) res = lv.as.i < rv.as.i;
                else if (op == OP_GT) res = lv.as.i > rv.as.i;
                else if (op == OP_LE) res = lv.as.i <= rv.as.i;
                else if (op == OP_GE) res = lv.as.i >= rv.as.i;
                vm_push(vm, val_int(res ? 1 : 0));
                break;
            }
            case OP_NOT: {
                if (vm->sp == 0) { vm_error(vm, "stack underflow", line, col); break; }
                Value v = vm_pop(vm);
                if (v.type != VAL_INT) { vm_error(vm, "expected int", line, col); break; }
                vm_push(vm, val_int(v.as.i == 0 ? 1 : 0));
                break;
            }
            case OP_JUMP: {
                unsigned int off = (unsigned int)(fr->func->code[fr->ip]) |
                          ((unsigned int)(fr->func->code[fr->ip+1]) << 8) |
                          ((unsigned int)(fr->func->code[fr->ip+2]) << 16) |
                          ((unsigned int)(fr->func->code[fr->ip+3]) << 24);
                fr->ip += 4;
                fr->ip += (int)off;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                unsigned int off = (unsigned int)(fr->func->code[fr->ip]) |
                          ((unsigned int)(fr->func->code[fr->ip+1]) << 8) |
                          ((unsigned int)(fr->func->code[fr->ip+2]) << 16) |
                          ((unsigned int)(fr->func->code[fr->ip+3]) << 24);
                fr->ip += 4;
                if (vm->sp == 0) { vm_error(vm, "stack underflow", line, col); break; }
                Value v = vm_pop(vm);
                int truthy = (v.type == VAL_INT) ? (v.as.i != 0) : 1;
                if (!truthy) {
                    fr->ip += off;
                }
                break;
            }
            case OP_CALL: {
                unsigned int fi = fr->func->code[fr->ip] | (fr->func->code[fr->ip+1] << 8);
                fr->ip += 2;
                unsigned char argc = fr->func->code[fr->ip++];
                if (fi >= vm->nfuncs) { vm_error(vm, "bad function", line, col); break; }
                Func *callee = vm->funcs[fi];
                if (argc != callee->arity) {
                    char msg[256];
                    {
                        const char *nm = callee->name ? callee->name : "?";
                        int mi = 0;
                        const char *prefix = "arity mismatch for '";
                        while (prefix[mi]) { msg[mi] = prefix[mi]; mi++; }
                        for (const char *q = nm; *q; q++) msg[mi++] = *q;
                        msg[mi++] = '\'';
                        msg[mi] = '\0';
                    }
                    vm_error(vm, msg, line, col); break;
                }
                if (vm->fp >= vm->frame_cap) { vm_error(vm, "stack overflow", line, col); break; }
                /* pop args */
                Value *args = bmm_alloc(argc * sizeof(Value));
                for (int i = argc-1; i >= 0; i--) args[i] = vm_pop(vm);
                Frame *nf = &vm->frames[vm->fp++];
                nf->func = callee;
                nf->ip = 0;
                nf->locals = bmm_alloc(callee->nlocals * sizeof(Value));
                for (int i = 0; i < callee->nlocals; i++) nf->locals[i] = val_int(0);
                for (int i = 0; i < argc; i++) nf->locals[i] = args[i];
                bmm_free(args);
                break;
            }
            case OP_RET: {
                /* pop frame */
                vm->fp--;
                if (fr->locals) bmm_free(fr->locals);
                /* push default 0? For statements, no. */
                break;
            }
            case OP_RET_VAL: {
                Value v = vm_pop(vm);
                vm->fp--;
                if (fr->locals) bmm_free(fr->locals);
                if (vm->fp > 0) vm_push(vm, v);
                break;
            }
            case OP_ARRAY_NEW: {
                unsigned int count = fr->func->code[fr->ip] | (fr->func->code[fr->ip+1] << 8);
                fr->ip += 2;
                ObjArray *arr = new_array();
                /* ensure capacity */
                while (arr->cap < count) { arr->cap *= 2; arr->items = bmm_realloc(arr->items, arr->cap*sizeof(Value)); }
                /* pop count values in order */
                Value *tmp = bmm_alloc(count * sizeof(Value));
                for (int i = count-1; i >= 0; i--) tmp[i] = vm_pop(vm);
                for (unsigned int i = 0; i < count; i++) arr->items[i] = tmp[i];
                arr->len = count;
                bmm_free(tmp);
                vm_push(vm, val_arr((Obj*)arr));
                break;
            }
            case OP_INDEX_GET: {
                if (vm->sp < 2) { vm_error(vm, "stack underflow", line, col); break; }
                Value idxv = vm->stack[vm->sp-1];
                Value objv = vm->stack[vm->sp-2];
                if (idxv.type != VAL_INT) { vm_error(vm, "index must be int", line, col); break; }
                int idx = idxv.as.i;
                if (objv.type == VAL_STR) {
                    ObjString *s = (ObjString*)objv.as.o;
                    if (idx < 0 || idx >= (int)s->len) { vm_error(vm, "index out of range", line, col); break; }
                    ObjString *ns = new_string(s->chars + idx, 1);
                    vm->sp -= 2;
                    vm_push(vm, val_str((Obj*)ns));
                } else if (objv.type == VAL_ARR) {
                    ObjArray *a = (ObjArray*)objv.as.o;
                    if (idx < 0 || idx >= (int)a->len) { vm_error(vm, "index out of range", line, col); break; }
                    vm->sp -= 2;
                    vm_push(vm, a->items[idx]);
                } else {
                    vm_error(vm, "indexing requires str or arr", line, col);
                }
                break;
            }
            case OP_INDEX_SET: {
                if (vm->sp < 3) { vm_error(vm, "stack underflow", line, col); break; }
                Value val = vm_pop(vm);
                Value idxv = vm_pop(vm);
                Value objv = vm_pop(vm);
                if (objv.type != VAL_ARR) { vm_error(vm, "index assignment requires array", line, col); break; }
                if (idxv.type != VAL_INT) { vm_error(vm, "index must be int", line, col); break; }
                ObjArray *a = (ObjArray*)objv.as.o;
                int idx = idxv.as.i;
                if (idx < 0 || idx >= (int)a->len) { vm_error(vm, "index out of range", line, col); break; }
                a->items[idx] = val;
                vm_push(vm, val);
                break;
            }
            case OP_CALL_BUILTIN: {
                unsigned char bi = fr->func->code[fr->ip++];
                unsigned char argc = fr->func->code[fr->ip++];
                /* dispatch */
                if (bi == BI_PRINT) {
                    if (argc != 1) { vm_error(vm, "print expects 1 argument", line, col); break; }
                    Value v = vm_pop(vm);
                    char *r = val_to_cstr_repr(v);
                    pal_write_stdout(r, strlen(r));
                    bmm_free(r);
                    vm_push(vm, val_int(0));
                } else if (bi == BI_PRINTLN) {
                    if (argc != 1) { vm_error(vm, "println expects 1 argument", line, col); break; }
                    Value v = vm_pop(vm);
                    char *r = val_to_cstr_repr(v);
                    pal_write_stdout(r, strlen(r));
                    bmm_free(r);
                    pal_write_stdout("\n", 1);
                    vm_push(vm, val_int(0));
                } else if (bi == BI_INPUT) {
                    if (argc != 0) { vm_error(vm, "input expects 0 arguments", line, col); break; }
                    /* read line */
                    char buf[4096];
                    size_t len = 0;
                    /* read char by char */
                    while (1) {
                        char ch;
                        int r = pal_read_stdin(&ch, 1);
                        if (r <= 0) break;
                        if (ch == '\n') break;
                        if (len + 1 < sizeof(buf)) buf[len++] = ch;
                    }
                    buf[len] = '\0';
                    /* strip trailing \r */
                    if (len > 0 && buf[len-1] == '\r') { buf[--len] = '\0'; }
                    ObjString *s = new_string(buf, len);
                    vm_push(vm, val_str((Obj*)s));
                } else if (bi == BI_LEN) {
                    if (argc != 1) { vm_error(vm, "len expects 1 argument", line, col); break; }
                    Value v = vm_pop(vm);
                    if (v.type == VAL_STR) {
                        ObjString *s = (ObjString*)v.as.o;
                        vm_push(vm, val_int((int32_t)s->len));
                    } else if (v.type == VAL_ARR) {
                        ObjArray *a = (ObjArray*)v.as.o;
                        vm_push(vm, val_int((int32_t)a->len));
                    } else {
                        vm_error(vm, "len expects str or arr", line, col);
                    }
                } else if (bi == BI_PUSH) {
                    if (argc != 2) { vm_error(vm, "push expects 2 arguments", line, col); break; }
                    Value v = vm_pop(vm);
                    Value a = vm_pop(vm);
                    if (a.type != VAL_ARR) { vm_error(vm, "push expects array", line, col); break; }
                    array_push((ObjArray*)a.as.o, v);
                    vm_push(vm, val_int(0));
                } else if (bi == BI_POP) {
                    if (argc != 1) { vm_error(vm, "pop expects 1 argument", line, col); break; }
                    Value a = vm_pop(vm);
                    if (a.type != VAL_ARR) { vm_error(vm, "pop expects array", line, col); break; }
                    ObjArray *ar = (ObjArray*)a.as.o;
                    if (ar->len == 0) { vm_error(vm, "pop from empty array", line, col); break; }
                    Value v = array_pop(ar);
                    vm_push(vm, v);
                } else if (bi == BI_STR) {
                    if (argc != 1) { vm_error(vm, "str expects 1 argument", line, col); break; }
                    Value v = vm_pop(vm);
                    if (v.type == VAL_INT) {
                        char buf[32];
                        { int neg = v.as.i < 0; unsigned int uvi = neg ? (unsigned int)(-(v.as.i + 1)) + 1u : (unsigned int)v.as.i; char tmp[32]; int ti = 0; if (uvi == 0) tmp[ti++] = '0'; else while (uvi > 0) { tmp[ti++] = '0' + (uvi % 10); uvi /= 10; } int bi = 0; if (neg) buf[bi++] = '-'; while (ti > 0) buf[bi++] = tmp[--ti]; buf[bi] = '\0'; }
                        ObjString *s = new_string(buf, strlen(buf));
                        vm_push(vm, val_str((Obj*)s));
                    } else if (v.type == VAL_STR) {
                        vm_push(vm, v);
                    } else {
                        vm_error(vm, "str expects int or str", line, col);
                    }
                } else if (bi == BI_INT) {
                    if (argc != 1) { vm_error(vm, "int expects 1 argument", line, col); break; }
                    Value v = vm_pop(vm);
                    if (v.type == VAL_INT) {
                        vm_push(vm, v);
                    } else if (v.type == VAL_STR) {
                        ObjString *s = (ObjString*)v.as.o;
                        /* parse */
                        char *buf = bmm_alloc(s->len+1);
                        memcpy(buf, s->chars, s->len);
                        buf[s->len] = '\0';
                        /* trim? no */
                        char *p = buf;
                        int neg = 0;
                        if (*p == '-') { neg = 1; p++; }
                        if (*p == '\0') { vm_error(vm, "invalid integer", line, col); bmm_free(buf); break; }
                        for (char *q = p; *q; q++) {
                            if (*q < '0' || *q > '9') { vm_error(vm, "invalid integer", line, col); bmm_free(buf); goto done_int; }
                        }
                        long val = 0;
                        for (char *q = p; *q; q++) val = val * 10 + (*q - '0');
                        if (neg) val = -val;
                        vm_push(vm, val_int((int32_t)val));
                        bmm_free(buf);
                        done_int:;
                    } else {
                        vm_error(vm, "int expects int or str", line, col);
                    }
                } else if (bi == BI_CHR) {
                    if (argc != 1) { vm_error(vm, "chr expects 1 argument", line, col); break; }
                    Value v = vm_pop(vm);
                    if (v.type != VAL_INT) { vm_error(vm, "chr expects int", line, col); break; }
                    /* code point to UTF-8 */
                    int cp = v.as.i;
                    char buf[8];
                    int len = 0;
                    if (cp < 0x80) { buf[len++] = (char)cp; }
                    else if (cp < 0x800) { buf[len++] = (char)(0xC0 | (cp >> 6)); buf[len++] = (char)(0x80 | (cp & 0x3F)); }
                    else if (cp < 0x10000) { buf[len++] = (char)(0xE0 | (cp >> 12)); buf[len++] = (char)(0x80 | ((cp >> 6) & 0x3F)); buf[len++] = (char)(0x80 | (cp & 0x3F)); }
                    else { buf[len++] = (char)(0xF0 | (cp >> 18)); buf[len++] = (char)(0x80 | ((cp >> 12) & 0x3F)); buf[len++] = (char)(0x80 | ((cp >> 6) & 0x3F)); buf[len++] = (char)(0x80 | (cp & 0x3F)); }
                    buf[len] = '\0';
                    ObjString *s = new_string(buf, len);
                    vm_push(vm, val_str((Obj*)s));
                } else if (bi == BI_ORD) {
                    if (argc != 1) { vm_error(vm, "ord expects 1 argument", line, col); break; }
                    Value v = vm_pop(vm);
                    if (v.type != VAL_STR) { vm_error(vm, "ord expects str", line, col); break; }
                    ObjString *s = (ObjString*)v.as.o;
                    /* decode first code point */
                    unsigned char *p = (unsigned char*)s->chars;
                    int cp = 0, adv = 0;
                    if (s->len == 0) { vm_error(vm, "ord expects single character", line, col); break; }
                    if (p[0] < 0x80) { cp = p[0]; adv = 1; }
                    else if ((p[0] & 0xE0) == 0xC0) { cp = ((p[0]&0x1F)<<6)|(p[1]&0x3F); adv = 2; }
                    else if ((p[0] & 0xF0) == 0xE0) { cp = ((p[0]&0x0F)<<12)|((p[1]&0x3F)<<6)|(p[2]&0x3F); adv = 3; }
                    else { cp = ((p[0]&0x07)<<18)|((p[1]&0x3F)<<12)|((p[2]&0x3F)<<6)|(p[3]&0x3F); adv = 4; }
                    if (adv != (int)s->len) { vm_error(vm, "ord expects single character", line, col); break; }
                    vm_push(vm, val_int(cp));
                } else if (bi == BI_SUB) {
                    if (argc != 3) { vm_error(vm, "sub expects 3 arguments", line, col); break; }
                    Value ln = vm->stack[vm->sp-1];
                    Value st = vm->stack[vm->sp-2];
                    Value x = vm->stack[vm->sp-3];
                    if (st.type != VAL_INT || ln.type != VAL_INT) { vm_error(vm, "sub expects int indices", line, col); break; }
                    int start = st.as.i, length = ln.as.i;
                    if (x.type == VAL_STR) {
                        ObjString *s = (ObjString*)x.as.o;
                        if (start < 0 || length < 0 || start + length > (int)s->len) { vm_error(vm, "index out of range", line, col); break; }
                        ObjString *ns = new_string(s->chars + start, length);
                        vm->sp -= 3;
                        vm_push(vm, val_str((Obj*)ns));
                    } else if (x.type == VAL_ARR) {
                        ObjArray *a = (ObjArray*)x.as.o;
                        if (start < 0 || length < 0 || start + length > (int)a->len) { vm_error(vm, "index out of range", line, col); break; }
                        ObjArray *na = new_array();
                        while (na->cap < (size_t)length) { na->cap*=2; na->items=bmm_realloc(na->items, na->cap*sizeof(Value)); }
                        for (int i = 0; i < length; i++) na->items[i] = a->items[start+i];
                        na->len = length;
                        vm->sp -= 3;
                        vm_push(vm, val_arr((Obj*)na));
                    } else {
                        vm_error(vm, "sub expects str or arr", line, col);
                    }
                } else if (bi == BI_ARR) {
                    if (argc != 0) { vm_error(vm, "arr expects 0 arguments", line, col); break; }
                    vm_push(vm, val_arr((Obj*)new_array()));
                } else if (bi == BI_EXIT) {
                    if (argc != 1) { vm_error(vm, "exit expects 1 argument", line, col); break; }
                    Value v = vm_pop(vm);
                    if (v.type != VAL_INT) { vm_error(vm, "exit expects int", line, col); break; }
                    int code = v.as.i % 256;
                    if (code < 0) code += 256;
                    pal_exit(code);
                } else if (bi == BI_CLOCK) {
                    if (argc != 0) { vm_error(vm, "clock expects 0 arguments", line, col); break; }
                    vm_push(vm, val_int((int32_t)pal_clock_ms()));
                } else {
                    vm_error(vm, "unknown builtin", line, col);
                }
                break;
            }
            case OP_EXIT: {
                pal_exit(0);
                break;
            }
            case OP_HALT: {
                if (vm->fp > 0) {
                    Frame *fr = &vm->frames[vm->fp-1];
                    if (fr->locals) bmm_free(fr->locals);
                }
                vm->fp = 0;
                break;
            }
            default: {
                vm_error(vm, "unknown opcode", line, col);
                break;
            }
        }
        if (vm->had_error) break;
        /* GC maybe */
    }
    return vm->had_error ? -1 : 0;
}
