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
    PUT16(2);
    const char *fname = c->filename ? c->filename : "";
    uint32_t fnlen = (uint32_t)strlen(fname);
    PUT32(fnlen);
    for (uint32_t i = 0; i < fnlen; i++) PUT(fname[i]);
    PUT16((unsigned int)c->nglobals);
    PUT16((unsigned int)c->nfuncs);
    PUT16((unsigned int)c->main_index);
    for (size_t gi = 0; gi < c->nglobals; gi++) {
        PUT(c->global_defined_flags ? (unsigned char)c->global_defined_flags[gi] : 0);
    }
    for (size_t gi = 0; gi < c->nglobals; gi++) {
        const char *gn = c->global_names[gi] ? c->global_names[gi] : "";
        uint16_t gl = (uint16_t)strlen(gn);
        PUT16(gl);
        for (uint16_t q = 0; q < gl; q++) PUT(gn[q]);
    }
    for (size_t fi = 0; fi < c->nfuncs; fi++) {
        Func *f = c->funcs[fi];
        const char *name = f->name ? f->name : "";
        size_t nl = strlen(name);
        for (size_t i = 0; i <= nl; i++) PUT(name[i]);
        PUT(f->arity);
        PUT16((unsigned int)f->nlocals);
        PUT32((unsigned int)f->code_len);
        for (size_t i = 0; i < f->code_len; i++) PUT(f->code[i]);
        for (size_t i = 0; i < f->code_len; i++) {
            uint32_t u = f->lineinfo ? (uint32_t)f->lineinfo[i] : 0;
            PUT(u & 0xFF); PUT((u>>8)&0xFF); PUT((u>>16)&0xFF); PUT((u>>24)&0xFF);
        }
        for (size_t i = 0; i < f->code_len; i++) {
            uint32_t u = f->colinfo ? (uint32_t)f->colinfo[i] : 0;
            PUT(u & 0xFF); PUT((u>>8)&0xFF); PUT((u>>16)&0xFF); PUT((u>>24)&0xFF);
        }
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
    const char *src = c->source ? c->source : "";
    uint32_t srclen = (uint32_t)strlen(src);
    PUT32(srclen);
    for (uint32_t i = 0; i < srclen; i++) PUT(src[i]);
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
    if (version != 2) return -1;
    uint32_t fnlen = GET32();
    if (fnlen > flen || pos + fnlen > flen) return -1;
    vm->source_name = bmm_alloc(fnlen + 1);
    memcpy(vm->source_name, buf + pos, fnlen);
    vm->source_name[fnlen] = '\0';
    pos += fnlen;
    unsigned int nglobals = GET16();
    unsigned int nfuncs = GET16();
    unsigned int entry = GET16();
    if (nglobals > flen || nfuncs > flen) return -1;
    vm->nglobals = nglobals;
    vm->globals = bmm_alloc((nglobals?nglobals:1)*sizeof(Value));
    vm->global_defined = bmm_alloc((nglobals?nglobals:1)*sizeof(char));
    vm->global_names = NULL;
    for (unsigned int i = 0; i < nglobals; i++) { vm->globals[i] = val_int(0); vm->global_defined[i] = (char)GET(); }
    vm->global_names = bmm_alloc((nglobals?nglobals:1)*sizeof(char*));
    vm->owns_global_names = 1;
    for (unsigned int i = 0; i < nglobals; i++) vm->global_names[i] = NULL;
    for (unsigned int i = 0; i < nglobals; i++) {
        unsigned int gl = GET16();
        if (gl > flen || pos + gl > flen) return -1;
        char *gn = bmm_alloc(gl + 1);
        memcpy(gn, buf + pos, gl);
        gn[gl] = '\0';
        pos += gl;
        vm->global_names[i] = gn;
    }
    vm->nfuncs = nfuncs;
    vm->func_cap = nfuncs ? nfuncs : 1;
    vm->funcs = bmm_alloc(vm->func_cap * sizeof(Func*));
    vm->owns_funcs = 1;
    for (unsigned int i = 0; i < nfuncs; i++) vm->funcs[i] = NULL;
    for (unsigned int fi = 0; fi < nfuncs; fi++) {
        Func *f = bmm_alloc(sizeof(Func));
        memset(f, 0, sizeof(Func));
        vm->funcs[fi] = f;
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
        if (code_len > flen) return -1;
        f->code = bmm_alloc(code_len ? code_len : 1);
        f->code_len = code_len;
        for (unsigned int i = 0; i < code_len; i++) f->code[i] = GET();
        f->lineinfo = bmm_alloc((code_len ? code_len : 1) * sizeof(int));
        f->colinfo = bmm_alloc((code_len ? code_len : 1) * sizeof(int));
        for (unsigned int i = 0; i < code_len; i++) f->lineinfo[i] = (int)GET32();
        for (unsigned int i = 0; i < code_len; i++) f->colinfo[i] = (int)GET32();
        unsigned int nconsts = GET16();
        if (nconsts > flen) return -1;
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
                if (slen > flen || pos + slen > flen) return -1;
                ObjString *s = new_string(buf + pos, slen);
                pos += slen;
                f->consts[ci] = val_str((Obj*)s);
            }
        }
    }
    uint32_t srclen = GET32();
    if (srclen > flen || pos + srclen + 4 > flen) { return -1; }
    vm->source = bmm_alloc(srclen + 1);
    memcpy(vm->source, buf + pos, srclen);
    vm->source[srclen] = '\0';
    pos += srclen;
    if (pos + 4 > flen) { return -1; }
    uint32_t stored_crc = GET32();
    uint32_t calc_crc = crc32((const unsigned char*)buf, pos - 4);
    if (stored_crc != calc_crc) return -1;
    vm->entry_func = entry;
    if (c) { c->nglobals = nglobals; c->nfuncs = nfuncs; c->main_index = entry; }
    return 0;
}
