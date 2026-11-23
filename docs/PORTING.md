# Porting Guide

1. Copy `src/pal/pal_template.c` to `src/pal/pal_<platform>.c`.
2. Implement the functions in `pal.h`:
   - `pal_write_stdout`, `pal_write_stderr`
   - `pal_read_stdin`
   - `pal_read_file`, `pal_write_file`
   - `pal_clock_ms`
   - `pal_set_args`, `pal_argc`, `pal_argv`
   - `pal_exit`
   - `pal_getcwd`
3. Adjust `src/bmm_config.h` if needed.
4. Build with your platform compiler, replacing `pal_posix.c`.

## Allowed headers outside src/pal

`stdint.h`, `stddef.h`, `stdarg.h`, `limits.h`, and `string.h`. Additionally,
`stdlib.h` is permitted only in `src/gc.c` for the allocation primitives.

No other source file may include operating-system headers. The core depends
only on these headers and the PAL.

## Checklist

- [ ] Byte output to stdout and stderr works.
- [ ] Line input works.
- [ ] File reading and writing work.
- [ ] Clock returns monotonic milliseconds.
- [ ] Arguments are forwarded.
- [ ] Exit terminates the process.
- [ ] Current working directory is available.
- [ ] All tests pass on the new platform.
