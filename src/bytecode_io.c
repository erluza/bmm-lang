#include "bytecode_io.h"
#include "bmm_config.h"
#include "gc.h"
#include <string.h>

static uint32_t crc32(const unsigned char *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320u;
            else crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

static unsigned int GET16F(const char *buf, size_t flen, size_t *pos) {
    unsigned int a = (*pos < flen) ? (unsigned char)buf[(*pos)++] : 0;
    unsigned int b = (*pos < flen) ? (unsigned char)buf[(*pos)++] : 0;
    return a | (b << 8);
}
static unsigned int GET32F(const char *buf, size_t flen, size_t *pos) {
    unsigned int a = (*pos < flen) ? (unsigned char)buf[(*pos)++] : 0;
    unsigned int b = (*pos < flen) ? (unsigned char)buf[(*pos)++] : 0;
    unsigned int c = (*pos < flen) ? (unsigned char)buf[(*pos)++] : 0;
    unsigned int d = (*pos < flen) ? (unsigned char)buf[(*pos)++] : 0;
    return a | (b << 8) | (c << 16) | (d << 24);
}

int bmc_encode(Compiler *c, char **out_buf, size_t *out_len) {
    size_t cap = 1 << 20;
    unsigned char *buf = bmm_alloc(cap);
    size_t len = 0;
    #define PUT(b) do { if (len >= cap) { cap*=2; buf=bmm_realloc(buf,cap);} buf[len++]=(b); } while(0)
    #define PUT16(v) do { PUT((v)&0xFF); PUT(((v)>>8)&0xFF); } while(0)
    #define PUT32(v) do { PUT((v)&0xFF); PUT(((v)>>8)&0xFF); PUT(((v)>>16)&0xFF); PUT(((v)>>24)&0xFF); } while(0)
    PUT('B'); PUT('M'); PUT('C'); PUT(0);
    PUT16(1);
    PUT16((unsigned int)c->nglobals);
    PUT16((unsigned int)c->nfuncs);
    PUT16((unsigned int)c->main_index);
    for (size_t fi = 0; fi < c->nfuncs; fi++) {
        Func *f = c->funcs[fi];
        const char *name = f->name ? f->name : "";
        size_t nl = strlen(name);
        for (size_t i = 0; i <= nl; i++) PUT(name[i]);
        PUT(f->arity);
        PUT16((unsigned int)f->nlocals);
        PUT32((unsigned int)f->code_len);
        for (size_t i = 0; i < f->code_len; i++) PUT(f->code[i]);
        PUT16((unsigned int)f->nconsts);
        for (size_t ci = 0; ci < f->nconsts; ci++) {
            Value v = f->consts[ci];
            if (v.type == VAL_INT) {
                PUT(0);
                uint32_t u = (uint32_t)v.as.i;
                PUT(u & 0xFF); PUT((u>>8)&0xFF); PUT((u>>16)&0xFF); PUT((u>>24)&0xFF);
            } else if (v.type == VAL_STR) {
                PUT(1);
                ObjString *s = (ObjString*)v.as.o;
                PUT16((unsigned int)s->len);
                for (size_t i = 0; i < s->len; i++) PUT(s->chars[i]);
            } else {
                PUT(0);
                PUT32(0);
            }
        }
    }
    uint32_t crc = crc32(buf, len);
    PUT32(crc);
    *out_buf = (char*)buf;
    *out_len = len;
    return 0;
}

int bmc_decode(const char *buf, size_t flen, BmmVM *vm, Compiler *c) {
    size_t pos = 0;
    #define GET() (pos < flen ? (unsigned char)buf[pos++] : 0)
    #define GET16() GET16F(buf, flen, &pos)
    #define GET32() GET32F(buf, flen, &pos)
    if (flen < 4 || !(buf[0]=='B' && buf[1]=='M' && buf[2]=='C' && buf[3]==0)) return -1;
    pos = 4;
    unsigned int version = GET16();
    if (version != 1) return -1;
    unsigned int nglobals = GET16();
    unsigned int nfuncs = GET16();
    unsigned int entry = GET16();
    vm->nglobals = nglobals;
    vm->globals = bmm_alloc((nglobals?nglobals:1)*sizeof(Value));
    vm->global_defined = bmm_alloc((nglobals?nglobals:1)*sizeof(char));
    vm->global_names = NULL;
    for (unsigned int i = 0; i < nglobals; i++) { vm->globals[i] = val_int(0); vm->global_defined[i] = 1; }
    vm->nfuncs = nfuncs;
    vm->func_cap = nfuncs ? nfuncs : 1;
    vm->funcs = bmm_alloc(vm->func_cap * sizeof(Func*));
    for (unsigned int fi = 0; fi < nfuncs; fi++) {
        Func *f = bmm_alloc(sizeof(Func));
        memset(f, 0, sizeof(Func));
        size_t nl = strlen(buf + pos);
        f->name = NULL;
        /* copy name */
        char *nm = bmm_alloc(nl+1);
        memcpy(nm, buf+pos, nl+1);
        f->name = nm;
        pos += nl + 1;
        f->arity = GET();
        f->nlocals = GET16();
        unsigned int code_len = GET32();
        f->code = bmm_alloc(code_len ? code_len : 1);
        f->code_len = code_len;
        for (unsigned int i = 0; i < code_len; i++) f->code[i] = GET();
        f->lineinfo = NULL;
        f->colinfo = NULL;
        unsigned int nconsts = GET16();
        f->nconsts = nconsts;
        f->const_cap = nconsts ? nconsts : 1;
        f->consts = bmm_alloc(f->const_cap * sizeof(Value));
        for (unsigned int ci = 0; ci < nconsts; ci++) {
            unsigned char type = GET();
            if (type == 0) {
                unsigned int u = GET32();
                f->consts[ci] = val_int((int32_t)u);
            } else {
                unsigned int slen = GET16();
                ObjString *s = new_string(buf + pos, slen);
                pos += slen;
                f->consts[ci] = val_str((Obj*)s);
            }
        }
        vm->funcs[fi] = f;
    }
    if (pos + 4 > flen) { return -1; }
    uint32_t stored_crc = GET32();
    uint32_t calc_crc = crc32((const unsigned char*)buf, pos - 4);
    if (stored_crc != calc_crc) return -1;
    vm->entry_func = entry;
    if (c) { c->nglobals = nglobals; c->nfuncs = nfuncs; c->main_index = entry; }
    return 0;
}
