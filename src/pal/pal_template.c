/* Template for porting B-- to a new platform.
 *
 * Copy this file to pal_<platform>.c and implement every function below.
 * No other source file may include OS headers; all system access goes here.
 *
 * Steps:
 * 1. Implement pal_write_stdout / pal_write_stderr (byte output).
 * 2. Implement pal_read_stdin (byte input).
 * 3. Implement pal_read_file (load a file into memory).
 * 4. Implement pal_clock_ms (monotonic-ish ms).
 * 5. Implement pal_set_args / pal_argc / pal_argv.
 * 6. Implement pal_exit.
 * 7. Implement pal_getcwd.
 * 8. Add the new file to your build and remove pal_posix.c.
 */

#include "pal.h"

/* TODO: implement for your platform. */

int pal_write_stdout(const void *buf, size_t len) { (void)buf; (void)len; return -1; }
int pal_write_stderr(const void *buf, size_t len) { (void)buf; (void)len; return -1; }
int pal_read_stdin(void *buf, size_t len) { (void)buf; (void)len; return -1; }
int pal_read_file(const char *path, char **out_buf, size_t *out_len) { (void)path; (void)out_buf; (void)out_len; return -1; }
int pal_write_file(const char *path, const void *buf, size_t len) { (void)path; (void)buf; (void)len; return -1; }
long long pal_clock_ms(void) { return 0; }
static int g_argc = 0;
static char **g_argv = NULL;
void pal_set_args(int argc, char **argv) { g_argc = argc; g_argv = argv; }
int pal_argc(void) { return g_argc; }
char **pal_argv(void) { return g_argv; }
void pal_exit(int code) { (void)code; }
int pal_getcwd(char *buf, size_t size) { (void)buf; (void)size; return -1; }
int pal_stdin_isatty(void) { return 0; }
