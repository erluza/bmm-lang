#include "out.h"
#include "gc.h"
#include "pal/pal.h"
#include <string.h>

static void out_grow(OutBuf *o, size_t need) {
    if (o->len + need <= o->cap) return;
    size_t nc = o->cap ? o->cap * 2 : 256;
    while (nc < o->len + need) nc *= 2;
    o->buf = bmm_realloc(o->buf, nc);
    o->cap = nc;
}

void out_init(OutBuf *o) {
    o->buf = NULL;
    o->len = 0;
    o->cap = 0;
}

void out_free(OutBuf *o) {
    bmm_free(o->buf);
    o->buf = NULL;
    o->len = 0;
    o->cap = 0;
}

void out_putc(OutBuf *o, char c) {
    out_grow(o, 1);
    o->buf[o->len++] = c;
}

void out_puts(OutBuf *o, const char *s) {
    if (!s) return;
    size_t n = strlen(s);
    out_write(o, s, n);
}

void out_write(OutBuf *o, const void *p, size_t n) {
    if (n == 0) return;
    out_grow(o, n);
    memcpy(o->buf + o->len, p, n);
    o->len += n;
}

void out_int(OutBuf *o, long v) {
    char tmp[32];
    int i = 0;
    int neg = 0;
    if (v < 0) { neg = 1; v = -v; }
    if (v == 0) tmp[i++] = '0';
    else {
        while (v > 0) { tmp[i++] = '0' + (int)(v % 10); v /= 10; }
    }
    if (neg) out_putc(o, '-');
    while (i > 0) out_putc(o, tmp[--i]);
}

void out_flush_stdout(OutBuf *o) {
    if (o->len > 0) pal_write_stdout(o->buf, o->len);
    out_free(o);
}

void out_flush_stderr(OutBuf *o) {
    if (o->len > 0) pal_write_stderr(o->buf, o->len);
    out_free(o);
}
