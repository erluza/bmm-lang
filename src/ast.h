#ifndef AST_H
#define AST_H

#include "bmm_config.h"

typedef enum {
    AST_NUM, AST_STR, AST_NAME, AST_BIN, AST_UNARY, AST_CALL, AST_INDEX, AST_ARRAY,
    AST_LET, AST_ASSIGN, AST_FN, AST_IF, AST_LOOP, AST_BREAK, AST_RET, AST_USE,
    AST_EXPRSTMT, AST_BLOCK
} AstKind;

typedef struct Ast Ast;

struct Ast {
    AstKind kind;
    int line, col;
    union {
        struct { long value; } num;
        struct { char *value; } str;
        struct { char *name; } name;
        struct { int op; Ast *left; Ast *right; } bin;
        struct { int op; Ast *operand; } unary;
        struct { char *name; Ast **args; int nargs; } call;
        struct { Ast *base; Ast *index; } idx;
        struct { Ast **elems; int nelems; } arr;
        struct { char *name; Ast *init; } let;
        struct { Ast *target; Ast *value; } assign;
        struct { char *name; char **params; int nparams; Ast *body; } fn;
        struct { Ast *cond; Ast *thenb; Ast *elseb; } iff;
        struct { Ast *cond; Ast *body; } loop;
        struct { Ast *value; } ret;
        struct { char *path; } use;
        struct { Ast *expr; } exprstmt;
        struct { Ast **stmts; int nstmts; } block;
    } as;
};

Ast *ast_new(AstKind kind, int line, int col);
void ast_free(Ast *a);

#endif
