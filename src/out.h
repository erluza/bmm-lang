#ifndef OUT_H
#define OUT_H

#include <stddef.h>

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} OutBuf;

void out_init(OutBuf *o);
void out_free(OutBuf *o);
void out_putc(OutBuf *o, char c);
void out_puts(OutBuf *o, const char *s);
void out_write(OutBuf *o, const void *p, size_t n);
void out_int(OutBuf *o, long v);
void out_flush_stdout(OutBuf *o);
void out_flush_stderr(OutBuf *o);

#endif
