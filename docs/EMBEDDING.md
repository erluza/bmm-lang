# Embedding

B-- is available as the library `libbmm`. The public header is `bmm.h`.

## API

```text noexec: C code
int bmm_run_file(const char *path, int argc, char **argv, int max_steps);
int bmm_run_source(const char *src, const char *filename,
                   int argc, char **argv, int max_steps);
void bmm_repl(void);
```

Compile your program against `src/*.c` and `src/pal/pal_posix.c`, linking with
no external libraries. All allocation passes through `bmm_alloc`,
`bmm_realloc`, and `bmm_free`, which may be replaced.
