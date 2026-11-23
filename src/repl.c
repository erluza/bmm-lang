#include "repl.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include "out.h"
#include "gc.h"
#include "pal/pal.h"
#include <string.h>


static int repl_tty = 0;

static Compiler comp;
static BmmVM *vm = NULL;
static Value *g_values = NULL;
static char *g_defined = NULL;
static char **g_names = NULL;
static size_t g_cap = 0;
static int initialized = 0;

static void ensure_globals(void) {
    if (comp.nglobals > g_cap) {
        g_values = bmm_realloc(g_values, comp.nglobals * sizeof(Value));
        g_defined = bmm_realloc(g_defined, comp.nglobals * sizeof(char));
        g_names = bmm_realloc(g_names, comp.nglobals * sizeof(char*));
        for (size_t i = g_cap; i < comp.nglobals; i++) {
            g_values[i] = val_int(0);
            g_defined[i] = 0;
            g_names[i] = comp.global_names[i];
        }
        g_cap = comp.nglobals;
    }
    for (size_t i = 0; i < comp.nglobals; i++) {
        g_names[i] = comp.global_names[i];
    }
    if (!vm) vm = vm_new();
    vm->globals = g_values;
    vm->global_defined = g_defined;
    vm->global_names = g_names;
    vm->nglobals = comp.nglobals;
    vm->funcs = comp.funcs;
    vm->nfuncs = comp.nfuncs;
    vm->func_cap = comp.nfuncs ? comp.nfuncs : 1;
    vm->entry_func = 0;
}

/* need xstrdup available */
/* defined above */

static void repl_run_entry(int entry_idx, int *had_error) {
    ensure_globals();
    /* snapshot globals */
    size_t snap_n = comp.nglobals;
    Value *snap_vals = bmm_alloc((snap_n ? snap_n : 1) * sizeof(Value));
    char *snap_def = bmm_alloc((snap_n ? snap_n : 1) * sizeof(char));
    for (size_t i = 0; i < snap_n; i++) { snap_vals[i] = g_values[i]; snap_def[i] = g_defined[i]; }
    int snap_nfuncs = (int)comp.nfuncs;
    int snap_nglobals = (int)comp.nglobals;

    vm->entry_func = entry_idx;
    gc_set_vm(vm);
    vm_run(vm, entry_idx);
    if (vm->had_error) {
        OutBuf e; out_init(&e);
        out_puts(&e, "bmm: <repl>:");
        out_int(&e, vm->error_line);
        out_putc(&e, ':');
        out_int(&e, vm->error_col);
        out_puts(&e, ": error: ");
        out_puts(&e, vm->error_msg ? vm->error_msg : "runtime error");
        out_putc(&e, '\n');
        out_flush_stderr(&e);
        /* rollback globals */
        vm->sp = 0;
        vm->fp = 0;
        comp.nglobals = snap_nglobals;
        comp.nfuncs = snap_nfuncs;
        ensure_globals();
        for (size_t i = 0; i < snap_n; i++) { g_values[i] = snap_vals[i]; g_defined[i] = snap_def[i]; }
        *had_error = 1;
    } else {
        *had_error = 0;
    }
    bmm_free(snap_vals);
    bmm_free(snap_def);
}

static void read_line(char *line, size_t cap) {
    size_t li = 0;
    while (li + 1 < cap) {
        char ch;
        int r = pal_read_stdin(&ch, 1);
        if (r <= 0) { line[li] = '\0'; return; }
        if (ch == '\n') { line[li] = '\0'; return; }
        line[li++] = ch;
    }
    line[li] = '\0';
}

void bmm_repl(void) {
    repl_tty = pal_stdin_isatty();
    if (repl_tty) {
        OutBuf o; out_init(&o);
        out_puts(&o, "B-- REPL. :help :quit :reset :load <file>\n");
        out_flush_stdout(&o);
    }

    if (!initialized) {
        memset(&comp, 0, sizeof(comp));
        initialized = 1;
    }

    char buf[1 << 16];
    size_t buflen = 0;
    buf[0] = '\0';

    while (1) {
        if (repl_tty) {
            OutBuf p; out_init(&p);
            out_puts(&p, buflen ? "... " : ">>> ");
            out_flush_stdout(&p);
        }

        char line[4096];
        read_line(line, sizeof(line));

        /* EOF */
        if (line[0] == '\0') {
            /* check if stdin closed */
            /* if no input and buffer empty, exit */
            /* We'll exit on EOF when buffer empty */
            if (buflen == 0) return;
        }

        if (buflen == 0 && line[0] == ':') {
            if (strcmp(line, ":quit") == 0) break;
            if (strcmp(line, ":help") == 0) {
                if (repl_tty) {
                    OutBuf h; out_init(&h);
                    out_puts(&h, ":quit :reset :load <file> :help\n");
                    out_flush_stdout(&h);
                }
                continue;
            }
            if (strcmp(line, ":reset") == 0) {
                memset(&comp, 0, sizeof(comp));
                g_cap = 0;
                g_values = NULL; g_defined = NULL; g_names = NULL;
                if (vm) { vm_free(vm); vm = NULL; }
                if (repl_tty) {
                    OutBuf h; out_init(&h);
                    out_puts(&h, "reset\n");
                    out_flush_stdout(&h);
                }
                continue;
            }
            if (strncmp(line, ":load", 5) == 0) {
                char *path = line + 5;
                while (*path == ' ') path++;
                char *src = NULL; size_t slen = 0;
                if (pal_read_file(path, &src, &slen) == 0) {
                    Ast *blk = NULL; Parser pp; memset(&pp, 0, sizeof(pp));
                    if (parse_source(src, path, &blk, &pp) == 0) {
                        lexer_free(&pp.lx);
                        /* snapshot */
                        size_t old_nfuncs = comp.nfuncs;
                        size_t old_nglobals = comp.nglobals;
                        int entry_idx;
                        /* collect+compile */
                        /* We need compile entry; if fails, restore */
                        /* deep-ish: save globals count */
                        if (compile_repl_entry(&comp, blk, ".", &entry_idx) == 0) {
                            int he = 0;
                            repl_run_entry(entry_idx, &he);
                            if (he) {
                                /* restore comp */
                                /* free new funcs? skip */
                                comp.nfuncs = old_nfuncs;
                                comp.nglobals = old_nglobals;
                                ensure_globals();

                            }
                        } else {
                            OutBuf e; out_init(&e);
                            out_puts(&e, "bmm: compile error\n");
                            out_flush_stderr(&e);
                        }
                        ast_free(blk);
                    } else {
                        lexer_free(&pp.lx);
                        OutBuf e; out_init(&e);
                        out_puts(&e, "bmm: parse error\n");
                        out_flush_stderr(&e);
                    }
                    bmm_free(src);
                } else {
                    OutBuf e; out_init(&e);
                    out_puts(&e, "bmm: cannot read file\n");
                    out_flush_stderr(&e);
                }
                continue;
            }
            if (repl_tty) {
                OutBuf u; out_init(&u);
                out_puts(&u, "unknown command\n");
                out_flush_stdout(&u);
            }
            continue;
        }

        if (buflen + strlen(line) + 2 >= sizeof(buf)) { buflen = 0; buf[0] = '\0'; continue; }
        strcpy(buf + buflen, line);
        buflen += strlen(line);
        buf[buflen++] = '\n';
        buf[buflen] = '\0';

        /* count opens/ends */
        int opens = 0, ends = 0;
        char tmp[1 << 16]; strcpy(tmp, buf);
        char *tok = strtok(tmp, " \t\n()[]{};,+-*/%<>=!");
        while (tok) {
            if (strcmp(tok, "fn")==0 || strcmp(tok, "if")==0 || strcmp(tok, "loop")==0) opens++;
            if (strcmp(tok, "end")==0) ends++;
            tok = strtok(NULL, " \t\n()[]{};,+-*/%<>=!");
        }
        if (opens > ends) continue;

        Ast *blk = NULL; Parser pp; memset(&pp, 0, sizeof(pp));
        if (parse_source(buf, "<repl>", &blk, &pp) != 0) {
            lexer_free(&pp.lx);
            /* maybe incomplete; continue accumulating */
            continue;
        }
        lexer_free(&pp.lx);

        /* snapshot */
        size_t old_nfuncs = comp.nfuncs;
        size_t old_nglobals = comp.nglobals;

        int entry_idx;
        if (compile_repl_entry(&comp, blk, ".", &entry_idx) != 0) {
            OutBuf e; out_init(&e);
            out_puts(&e, "bmm: compile error\n");
            out_flush_stderr(&e);
            ast_free(blk);
            buflen = 0; buf[0] = '\0';
            continue;
        }

        int he = 0;
        repl_run_entry(entry_idx, &he);
        if (he) {
            /* rollback */
            comp.nfuncs = old_nfuncs;
            comp.nglobals = old_nglobals;
            ensure_globals();
            if (vm) { vm->had_error = 0; }

        }
        ast_free(blk);
        buflen = 0; buf[0] = '\0';
    }
}
