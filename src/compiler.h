#ifndef COMPILER_H
#define COMPILER_H

#include "ast.h"
#include "vm.h"

typedef struct {
    Func **funcs;
    size_t nfuncs;
    size_t func_cap;
    /* globals */
    char **global_names;
    char *global_defined_flags;
    size_t nglobals;
    size_t global_cap;
    /* constants */
    Value *consts;
    size_t nconsts;
    size_t const_cap;
    /* main function index */
    int main_index;
    /* error */
    int had_error;
    char *error_msg;
    int error_line, error_col;
    /* retained ASTs from included files */
    Ast **extra_asts;
    size_t nextra;
    size_t extra_cap;
    const char *source; /* optional source text for .bmc diagnostics */
    const char *filename;
    char *active_uses[64];
    int nactive;
} Compiler;

int compile_program(Compiler *c, Ast *program, const char *filename);
int compile_repl_entry(Compiler *c, Ast *blk, const char *basedir, int *out_entry);
/* returns 0 on success */

#endif
