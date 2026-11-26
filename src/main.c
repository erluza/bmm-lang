#include "bmm.h"
#include "bmm_config.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include "bytecode_io.h"
#include "disasm.h"
#include "fmt.h"
#include "repl.h"
#include "out.h"
#include "gc.h"
#include "pal/pal.h"
#include <string.h>

static char *read_file_src(const char *path) {
    char *buf = NULL; size_t len = 0;
    if (pal_read_file(path, &buf, &len) != 0) return NULL;
    return buf;
}

static void print_error(const char *filename, const char *src, int line, int col, const char *msg) {
    OutBuf o; out_init(&o);
    out_puts(&o, "bmm: ");
    out_puts(&o, filename);
    out_putc(&o, ':');
    out_int(&o, line);
    out_putc(&o, ':');
    out_int(&o, col);
    out_puts(&o, ": error: ");
    out_puts(&o, msg ? msg : "error");
    out_putc(&o, '\n');
    if (src && line >= 1) {
        int cur = 1;
        const char *p = src;
        const char *line_start = src;
        while (*p && cur < line) {
            if (*p == '\n') { cur++; line_start = p+1; }
            p++;
        }
        out_puts(&o, "  ");
        const char *q = line_start;
        while (*q && *q != '\n') { out_putc(&o, *q); q++; }
        out_putc(&o, '\n');
        out_puts(&o, "  ");
        int c = col > 0 ? col : 1;
        for (int i = 1; i < c; i++) out_putc(&o, ' ');
        out_putc(&o, '^');
        out_putc(&o, '\n');
    }
    out_flush_stderr(&o);
}

static int is_bmc(const char *path) {
    size_t L = strlen(path);
    return L > 4 && strcmp(path + L - 4, ".bmc") == 0;
}

static int run_source(const char *src, const char *filename, int argc, char **argv, int max_steps, int trace) {
    Ast *blk = NULL;
    Parser pp; memset(&pp, 0, sizeof(pp));
    if (parse_source(src, filename, &blk, &pp) != 0) {
        print_error(filename, src, pp.error_line, pp.error_col, pp.error_msg ? pp.error_msg : "parse error");
        lexer_free(&pp.lx);
        bmm_free(pp.error_msg);
        ast_free(blk);
        return 1;
    }
    lexer_free(&pp.lx);
    Compiler comp; memset(&comp, 0, sizeof(comp));
    if (compile_program(&comp, blk, filename) != 0) {
        if (comp.error_msg) {
            print_error(filename, src, comp.error_line ? comp.error_line : 1, comp.error_col ? comp.error_col : 1, comp.error_msg);
        } else {
            OutBuf o; out_init(&o);
            out_puts(&o, "bmm: ");
            out_puts(&o, filename);
            out_puts(&o, ": compile error\n");
            out_flush_stderr(&o);
        }
        /* cleanup compiler */
        for (size_t i = 0; i < comp.nglobals; i++) bmm_free(comp.global_names[i]);
        bmm_free(comp.global_names);
        bmm_free(comp.global_defined_flags);
        for (size_t i = 0; i < comp.nfuncs; i++) {
            if (comp.funcs[i]) {
                bmm_free(comp.funcs[i]->code);
                bmm_free(comp.funcs[i]->lineinfo);
                bmm_free(comp.funcs[i]->colinfo);
                bmm_free(comp.funcs[i]->name);
                bmm_free(comp.funcs[i]);
            }
        }
        bmm_free(comp.funcs);
        bmm_free(comp.consts);
        bmm_free(comp.error_msg);
        for (size_t i = 0; i < comp.nextra; i++) ast_free(comp.extra_asts[i]);
        bmm_free(comp.extra_asts);
        ast_free(blk);
        return 1;
    }
    BmmVM *vm = vm_new();
    vm->globals = bmm_alloc((comp.nglobals ? comp.nglobals : 1) * sizeof(Value));
    vm->global_defined = bmm_alloc((comp.nglobals ? comp.nglobals : 1) * sizeof(char));
    vm->global_names = comp.global_names;
    vm->nglobals = comp.nglobals;
    for (size_t i = 0; i < comp.nglobals; i++) { vm->globals[i] = val_int(0); vm->global_defined[i] = 0; }
    vm->funcs = comp.funcs;
    vm->nfuncs = comp.nfuncs;
    vm->func_cap = comp.nfuncs;
    vm->entry_func = comp.main_index;
    vm->trace = trace;
    vm->max_steps = max_steps;
    int ai = -1;
    for (size_t i = 0; i < comp.nglobals; i++) {
        if (comp.global_names[i] && strcmp(comp.global_names[i], "argv") == 0) { ai = (int)i; break; }
    }
    ObjArray *aa = new_array();
    vm->argv_arr = aa;
    for (int i = 0; i < argc; i++) {
        ObjString *s = new_string(argv[i], strlen(argv[i]));
        array_push(aa, val_str((Obj*)s));
    }
    if (ai >= 0) { vm->globals[ai] = val_arr((Obj*)aa); vm->global_defined[ai] = 1; }
    gc_set_vm(vm);
    int rc = vm_run(vm, vm->entry_func);
    if (vm->had_error) {
        print_error(filename, src, vm->error_line ? vm->error_line : 1, vm->error_col ? vm->error_col : 1, vm->error_msg ? vm->error_msg : "runtime error");
    }
    int result = vm->had_error ? 1 : (rc != 0 ? 1 : 0);
    gc_collect(vm);
    vm_free(vm);
    gc_shutdown();
    for (size_t i = 0; i < comp.nglobals; i++) bmm_free(comp.global_names[i]);
    bmm_free(comp.global_names);
    bmm_free(comp.global_defined_flags);
    for (size_t i = 0; i < comp.nfuncs; i++) {
        if (comp.funcs[i]) {
            bmm_free(comp.funcs[i]->code);
            bmm_free(comp.funcs[i]->lineinfo);
            bmm_free(comp.funcs[i]->colinfo);
            bmm_free(comp.funcs[i]->name);
            bmm_free(comp.funcs[i]);
        }
    }
    bmm_free(comp.funcs);
    bmm_free(comp.consts);
    bmm_free(comp.error_msg);
    for (size_t i = 0; i < comp.nextra; i++) ast_free(comp.extra_asts[i]);
    bmm_free(comp.extra_asts);
    ast_free(blk);
    return result;
}

static int run_file(const char *path, int argc, char **argv, int max_steps, int trace) {
    if (is_bmc(path)) {
        char *buf = NULL; size_t len = 0;
        if (pal_read_file(path, &buf, &len) != 0) {
            OutBuf o; out_init(&o);
            out_puts(&o, "bmm: cannot read ");
            out_puts(&o, path);
            out_putc(&o, '\n');
            out_flush_stderr(&o);
            return 1;
        }
        BmmVM *vm = vm_new();
        Compiler comp; memset(&comp, 0, sizeof(comp));
        if (bmc_decode(buf, len, vm, &comp) != 0) {
            bmm_free(buf);
            OutBuf o; out_init(&o);
            out_puts(&o, "bmm: cannot load ");
            out_puts(&o, path);
            out_putc(&o, '\n');
            out_flush_stderr(&o);
            vm_free(vm);
            return 1;
        }
        bmm_free(buf);
        vm->max_steps = max_steps;
        vm->trace = trace;
        ObjArray *aa = new_array();
        vm->argv_arr = aa;
        for (int i = 0; i < argc; i++) {
            ObjString *s = new_string(argv[i], strlen(argv[i]));
            array_push(aa, val_str((Obj*)s));
        }
        if (vm->nglobals > 0) { vm->globals[0] = val_arr((Obj*)aa); vm->global_defined[0] = 1; }
        gc_set_vm(vm);
        int rc = vm_run(vm, vm->entry_func);
        if (vm->had_error) {
            print_error(vm->source_name ? vm->source_name : path, vm->source,
                        vm->error_line ? vm->error_line : 1,
                        vm->error_col ? vm->error_col : 1,
                        vm->error_msg ? vm->error_msg : "runtime error");
        }
        int result = vm->had_error ? 1 : (rc != 0 ? 1 : 0);
        gc_collect(vm);
        vm_free(vm);
        gc_shutdown();
        return result;
    }
    char *src = read_file_src(path);
    if (!src) {
        OutBuf o; out_init(&o);
        out_puts(&o, "bmm: cannot read ");
        out_puts(&o, path);
        out_putc(&o, '\n');
        out_flush_stderr(&o);
        return 1;
    }
    int rc = run_source(src, path, argc, argv, max_steps, trace);
    bmm_free(src);
    return rc;
}

int main(int argc, char **argv) {
    pal_set_args(argc, argv);
    if (argc < 2) {
        bmm_repl();
        return 0;
    }
    if (strcmp(argv[1], "--version") == 0) {
        OutBuf o; out_init(&o);
        out_puts(&o, "bmm 17.14\n");
        out_flush_stdout(&o);
        return 0;
    }
    if (strcmp(argv[1], "--help") == 0) {
        OutBuf o; out_init(&o);
        out_puts(&o, "usage: bmm [file.bmm [args...]]\n");
        out_puts(&o, "       bmm repl\n");
        out_puts(&o, "       bmm -e \"code\" [args...]\n");
        out_puts(&o, "       bmm build file.bmm -o out.bmc\n");
        out_puts(&o, "       bmm run out.bmc [args...]\n");
        out_puts(&o, "       bmm dis file\n");
        out_puts(&o, "       bmm fmt file.bmm\n");
        out_puts(&o, "       bmm check file.bmm\n");
        out_flush_stdout(&o);
        return 0;
    }
    if (strcmp(argv[1], "repl") == 0) {
        bmm_repl();
        return 0;
    }
    if (strcmp(argv[1], "-e") == 0) {
        if (argc < 3) {
            OutBuf o; out_init(&o);
            out_puts(&o, "usage: bmm -e \"code\"\n");
            out_flush_stderr(&o);
            return 2;
        }
        const char *code = argv[2];
        char **pargs = argv + 3;
        int nargs = argc - 3;
        return run_source(code, "<stdin>", nargs, pargs, 0, 0);
    }
    if (strcmp(argv[1], "build") == 0) {
        if (argc < 3) {
            OutBuf o; out_init(&o);
            out_puts(&o, "usage: bmm build file.bmm -o out.bmc\n");
            out_flush_stderr(&o);
            return 2;
        }
        const char *srcpath = argv[2];
        const char *outpath = NULL;
        for (int j = 3; j < argc; j++) {
            if (strcmp(argv[j], "-o") == 0 && j+1 < argc) { outpath = argv[++j]; }
        }
        if (!outpath) {
            OutBuf o; out_init(&o);
            out_puts(&o, "bmm build: missing -o\n");
            out_flush_stderr(&o);
            return 2;
        }
        char *src = read_file_src(srcpath);
        if (!src) {
            OutBuf o; out_init(&o);
            out_puts(&o, "bmm: cannot read ");
            out_puts(&o, srcpath);
            out_putc(&o, '\n');
            out_flush_stderr(&o);
            return 1;
        }
        Ast *blk = NULL; Parser pp; memset(&pp, 0, sizeof(pp));
        if (parse_source(src, srcpath, &blk, &pp) != 0) {
            print_error(srcpath, src, pp.error_line, pp.error_col, pp.error_msg ? pp.error_msg : "parse error");
            lexer_free(&pp.lx);
            bmm_free(pp.error_msg);
            ast_free(blk);
            bmm_free(src);
            return 1;
        }
        lexer_free(&pp.lx);
        Compiler comp; memset(&comp, 0, sizeof(comp));
        if (compile_program(&comp, blk, srcpath) != 0) {
            if (comp.error_msg) {
                print_error(srcpath, src, comp.error_line ? comp.error_line : 1, comp.error_col ? comp.error_col : 1, comp.error_msg);
            } else {
                OutBuf o; out_init(&o);
                out_puts(&o, "bmm: compile error\n");
                out_flush_stderr(&o);
            }
            ast_free(blk);
            bmm_free(src);
            return 1;
        }
        comp.source = src;
        char *buf = NULL; size_t buflen = 0;
        if (bmc_encode(&comp, &buf, &buflen) != 0) {
            OutBuf o; out_init(&o);
            out_puts(&o, "bmm: encode error\n");
            out_flush_stderr(&o);
            bmm_free(src);
            return 1;
        }
        int wrc = pal_write_file(outpath, buf, buflen);
        bmm_free(buf);
        if (wrc != 0) {
            OutBuf o; out_init(&o);
            out_puts(&o, "bmm: cannot write ");
            out_puts(&o, outpath);
            out_putc(&o, '\n');
            out_flush_stderr(&o);
            bmm_free(src);
            return 1;
        }
        /* cleanup */
        for (size_t i = 0; i < comp.nglobals; i++) bmm_free(comp.global_names[i]);
        bmm_free(comp.global_names);
        bmm_free(comp.global_defined_flags);
        for (size_t i = 0; i < comp.nfuncs; i++) {
            if (comp.funcs[i]) {
                bmm_free(comp.funcs[i]->code);
                bmm_free(comp.funcs[i]->lineinfo);
                bmm_free(comp.funcs[i]->colinfo);
                bmm_free(comp.funcs[i]->name);
                bmm_free(comp.funcs[i]);
            }
        }
        bmm_free(comp.funcs);
        bmm_free(comp.consts);
        for (size_t i = 0; i < comp.nextra; i++) ast_free(comp.extra_asts[i]);
        bmm_free(comp.extra_asts);
        ast_free(blk);
        bmm_free(src);
        return 0;
    }
    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            OutBuf o; out_init(&o);
            out_puts(&o, "usage: bmm run file.bmc\n");
            out_flush_stderr(&o);
            return 2;
        }
        const char *path = argv[2];
        char **pargs = argv + 3;
        int nargs = argc - 3;
        return run_file(path, nargs, pargs, 0, 0);
    }
    if (strcmp(argv[1], "dis") == 0) {
        if (argc < 3) {
            OutBuf o; out_init(&o);
            out_puts(&o, "usage: bmm dis file\n");
            out_flush_stderr(&o);
            return 2;
        }
        const char *path = argv[2];
        if (is_bmc(path)) {
            char *buf = NULL; size_t len = 0;
            if (pal_read_file(path, &buf, &len) != 0) {
                OutBuf o; out_init(&o);
                out_puts(&o, "bmm: cannot load ");
                out_puts(&o, path);
                out_putc(&o, '\n');
                out_flush_stderr(&o);
                return 1;
            }
            BmmVM *vm = vm_new();
            Compiler comp; memset(&comp, 0, sizeof(comp));
            if (bmc_decode(buf, len, vm, &comp) != 0) {
                bmm_free(buf);
                OutBuf o; out_init(&o);
                out_puts(&o, "bmm: cannot load ");
                out_puts(&o, path);
                out_putc(&o, '\n');
                out_flush_stderr(&o);
                vm_free(vm);
                return 1;
            }
            bmm_free(buf);
            disasm_vm(vm);
            vm_free(vm);
            return 0;
        } else {
            char *src = read_file_src(path);
            if (!src) {
                OutBuf o; out_init(&o);
                out_puts(&o, "bmm: cannot read ");
                out_puts(&o, path);
                out_putc(&o, '\n');
                out_flush_stderr(&o);
                return 1;
            }
            Ast *blk = NULL; Parser pp; memset(&pp, 0, sizeof(pp));
            if (parse_source(src, path, &blk, &pp) != 0) {
                print_error(path, src, pp.error_line, pp.error_col, pp.error_msg ? pp.error_msg : "parse error");
                lexer_free(&pp.lx);
                bmm_free(pp.error_msg);
                ast_free(blk);
                bmm_free(src);
                return 1;
            }
            lexer_free(&pp.lx);
            Compiler comp; memset(&comp, 0, sizeof(comp));
            if (compile_program(&comp, blk, path) != 0) {
                if (comp.error_msg) {
                    print_error(path, src, comp.error_line ? comp.error_line : 1, comp.error_col ? comp.error_col : 1, comp.error_msg);
                } else {
                    OutBuf o; out_init(&o);
                    out_puts(&o, "bmm: compile error\n");
                    out_flush_stderr(&o);
                }
                ast_free(blk);
                bmm_free(src);
                return 1;
            }
            BmmVM *vm = vm_new();
            vm->globals = bmm_alloc((comp.nglobals ? comp.nglobals : 1) * sizeof(Value));
            vm->global_defined = bmm_alloc((comp.nglobals ? comp.nglobals : 1) * sizeof(char));
            vm->global_names = comp.global_names;
            vm->nglobals = comp.nglobals;
            for (size_t i = 0; i < comp.nglobals; i++) { vm->globals[i] = val_int(0); vm->global_defined[i] = 0; }
            vm->funcs = comp.funcs;
            vm->nfuncs = comp.nfuncs;
            vm->func_cap = comp.nfuncs;
            vm->entry_func = comp.main_index;
            disasm_vm(vm);
            vm_free(vm);
            /* cleanup */
            for (size_t i = 0; i < comp.nglobals; i++) bmm_free(comp.global_names[i]);
            bmm_free(comp.global_names);
            bmm_free(comp.global_defined_flags);
            for (size_t i = 0; i < comp.nfuncs; i++) {
                if (comp.funcs[i]) {
                    bmm_free(comp.funcs[i]->code);
                    bmm_free(comp.funcs[i]->lineinfo);
                    bmm_free(comp.funcs[i]->colinfo);
                    bmm_free(comp.funcs[i]->name);
                    bmm_free(comp.funcs[i]);
                }
            }
            bmm_free(comp.funcs);
            bmm_free(comp.consts);
            for (size_t i = 0; i < comp.nextra; i++) ast_free(comp.extra_asts[i]);
            bmm_free(comp.extra_asts);
            ast_free(blk);
            bmm_free(src);
            return 0;
        }
    }
    if (strcmp(argv[1], "fmt") == 0) {
        if (argc < 3) {
            OutBuf o; out_init(&o);
            out_puts(&o, "usage: bmm fmt file.bmm\n");
            out_flush_stderr(&o);
            return 2;
        }
        if (fmt_file(argv[2]) != 0) {
            OutBuf o; out_init(&o);
            out_puts(&o, "bmm: fmt failed\n");
            out_flush_stderr(&o);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "check") == 0) {
        if (argc < 3) {
            OutBuf o; out_init(&o);
            out_puts(&o, "usage: bmm check file.bmm\n");
            out_flush_stderr(&o);
            return 2;
        }
        char *src = read_file_src(argv[2]);
        if (!src) {
            OutBuf o; out_init(&o);
            out_puts(&o, "bmm: cannot read ");
            out_puts(&o, argv[2]);
            out_putc(&o, '\n');
            out_flush_stderr(&o);
            return 1;
        }
        Ast *blk = NULL; Parser pp; memset(&pp, 0, sizeof(pp));
        if (parse_source(src, argv[2], &blk, &pp) != 0) {
            print_error(argv[2], src, pp.error_line, pp.error_col, pp.error_msg ? pp.error_msg : "parse error");
            lexer_free(&pp.lx);
            bmm_free(pp.error_msg);
            ast_free(blk);
            bmm_free(src);
            return 1;
        }
        lexer_free(&pp.lx);
        Compiler comp; memset(&comp, 0, sizeof(comp));
        if (compile_program(&comp, blk, argv[2]) != 0) {
            if (comp.error_msg) {
                print_error(argv[2], src, comp.error_line ? comp.error_line : 1, comp.error_col ? comp.error_col : 1, comp.error_msg);
            } else {
                OutBuf o; out_init(&o);
                out_puts(&o, "bmm: compile error\n");
                out_flush_stderr(&o);
            }
            ast_free(blk);
            bmm_free(src);
            return 1;
        }
        /* cleanup */
        for (size_t i = 0; i < comp.nglobals; i++) bmm_free(comp.global_names[i]);
        bmm_free(comp.global_names);
        bmm_free(comp.global_defined_flags);
        for (size_t i = 0; i < comp.nfuncs; i++) {
            if (comp.funcs[i]) {
                bmm_free(comp.funcs[i]->code);
                bmm_free(comp.funcs[i]->lineinfo);
                bmm_free(comp.funcs[i]->colinfo);
                bmm_free(comp.funcs[i]->name);
                bmm_free(comp.funcs[i]);
            }
        }
        bmm_free(comp.funcs);
        bmm_free(comp.consts);
        for (size_t i = 0; i < comp.nextra; i++) ast_free(comp.extra_asts[i]);
        bmm_free(comp.extra_asts);
        ast_free(blk);
        bmm_free(src);
        return 0;
    }
    /* flags */
    int max_steps = 0;
    int trace = 0;
    int idx = 1;
    while (idx < argc && argv[idx][0] == '-') {
        if (strcmp(argv[idx], "--trace") == 0) { trace = 1; idx++; }
        else if (strcmp(argv[idx], "--max-steps") == 0) {
            idx++;
            int v = 0;
            for (char *q = argv[idx]; *q; q++) { if (*q >= '0' && *q <= '9') v = v * 10 + (*q - '0'); }
            max_steps = v;
            idx++;
        }
        else break;
    }
    if (idx >= argc) {
        bmm_repl();
        return 0;
    }
    const char *path = argv[idx];
    char **pargs = argv + idx + 1;
    int nargs = argc - idx - 1;
    /* filter flags? skip for simplicity */
    return run_file(path, nargs, pargs, max_steps, trace);
}
