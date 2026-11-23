#include "pal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>

static int g_argc = 0;
static char **g_argv = NULL;
static struct timespec start_ts;

__attribute__((constructor)) static void pal_init(void) {
    clock_gettime(CLOCK_MONOTONIC, &start_ts);
}

int pal_write_stdout(const void *buf, size_t len) {
    size_t done = 0;
    while (done < len) {
        ssize_t r = write(STDOUT_FILENO, (const char*)buf + done, len - done);
        if (r < 0) return -1;
        done += (size_t)r;
    }
    return (int)done;
}

int pal_write_stderr(const void *buf, size_t len) {
    size_t done = 0;
    while (done < len) {
        ssize_t r = write(STDERR_FILENO, (const char*)buf + done, len - done);
        if (r < 0) return -1;
        done += (size_t)r;
    }
    return (int)done;
}

int pal_read_stdin(void *buf, size_t len) {
    ssize_t r = read(STDIN_FILENO, buf, len);
    if (r < 0) return -1;
    return (int)r;
}

int pal_read_file(const char *path, char **out_buf, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return -1; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return -1; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return -1; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (rd != (size_t)sz) { free(buf); return -1; }
    buf[sz] = '\0';
    *out_buf = buf;
    *out_len = (size_t)sz;
    return 0;
}

long long pal_clock_ms(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long long sec = (long long)(now.tv_sec - start_ts.tv_sec);
    long long nsec = (long long)(now.tv_nsec - start_ts.tv_nsec);
    return sec * 1000LL + nsec / 1000000LL;
}

void pal_set_args(int argc, char **argv) {
    g_argc = argc;
    g_argv = argv;
}

int pal_argc(void) { return g_argc; }
char **pal_argv(void) { return g_argv; }

void pal_exit(int code) { exit(code); }

int pal_write_file(const char *path, const void *buf, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t w = fwrite(buf, 1, len, f);
    fclose(f);
    if (w != len) return -1;
    return 0;
}

int pal_getcwd(char *buf, size_t size) {
    if (getcwd(buf, size) == NULL) return -1;
    return 0;
}

int pal_stdin_isatty(void) {
    return isatty(STDIN_FILENO) ? 1 : 0;
}
