#ifndef PAL_H
#define PAL_H

#include <stddef.h>
#include <stdint.h>

/* Platform Abstraction Layer. Implement these ~10 functions per platform. */

/* Write bytes to stdout/stderr. Return number written or -1 on error. */
int pal_write_stdout(const void *buf, size_t len);
int pal_write_stderr(const void *buf, size_t len);

/* Read up to len bytes from stdin. Return bytes read, 0 on EOF, -1 on error. */
int pal_read_stdin(void *buf, size_t len);

/* Read entire file into a malloc'd buffer. Returns 0 on success. */
int pal_read_file(const char *path, char **out_buf, size_t *out_len);

/* Write a buffer to a file (overwrite). Return 0 on success. */
int pal_write_file(const char *path, const void *buf, size_t len);

/* Current time in milliseconds (monotonic-ish, since process start). */
long long pal_clock_ms(void);

/* Program arguments. argc/argv provided at startup; store internally. */
void pal_set_args(int argc, char **argv);
int pal_argc(void);
char **pal_argv(void);

/* Exit process with status. */
void pal_exit(int code);

/* Get current working directory into buf (size). Return 0 on success. */
int pal_getcwd(char *buf, size_t size);

/* Return 1 if stdin is a terminal. */
int pal_stdin_isatty(void);

#endif
