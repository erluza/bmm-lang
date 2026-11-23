#ifndef VM_H
#define VM_H

#include "bmm_config.h"
#include "value.h"
#include "gc.h"
#include "opcodes.h"

typedef struct Func Func;

typedef struct {
    Func *func;
    int ip;
    Value *locals;
} Frame;

struct BmmVM {
    Value *stack;
    size_t stack_cap;
    size_t sp;
    Frame *frames;
    size_t frame_cap;
    size_t fp;
    Value *globals;
    char *global_defined;
    char **global_names;
    int owns_global_names;
    int owns_funcs;
    size_t nglobals;
    Func **funcs;
    size_t nfuncs;
    size_t func_cap;
    /* constant pool shared */
    /* source filename for errors */
    const char *filename;
    int trace;
    long max_steps;
    long steps;
    int had_error;
    char *error_msg;
    int error_line;
    int error_col;
    /* argv array object */
    ObjArray *argv_arr;
    int entry_func;
    char *source; /* optional source text for diagnostics */
    char *source_name;
};

struct Func {
    char *name;
    int arity;
    int nlocals;
    unsigned char *code;
    size_t code_len;
    int *lineinfo;
    int *colinfo;
    /* constants */
    Value *consts;
    size_t nconsts;
    size_t const_cap;
    /* ast for compilation (not used at runtime) */
    void *ast;
};

BmmVM *vm_new(void);
void vm_free(BmmVM *vm);
int vm_run(BmmVM *vm, int entry_func);
void vm_push(BmmVM *vm, Value v);
Value vm_pop(BmmVM *vm);
void vm_error(BmmVM *vm, const char *msg, int line, int col);

#endif
