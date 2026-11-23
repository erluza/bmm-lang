#include "ast.h"
#include "gc.h"
#include <string.h>

Ast *ast_new(AstKind kind, int line, int col) {
    Ast *a = bmm_alloc(sizeof(Ast));
    memset(a, 0, sizeof(Ast));
    a->kind = kind;
    a->line = line;
    a->col = col;
    return a;
}

static void free_str(char *s) { if (s) bmm_free(s); }

void ast_free(Ast *a) {
    if (!a) return;
    switch (a->kind) {
        case AST_NUM: break;
        case AST_STR: free_str(a->as.str.value); break;
        case AST_NAME: free_str(a->as.name.name); break;
        case AST_BIN:
            ast_free(a->as.bin.left);
            ast_free(a->as.bin.right);
            break;
        case AST_UNARY:
            ast_free(a->as.unary.operand);
            break;
        case AST_CALL:
            free_str(a->as.call.name);
            for (int i = 0; i < a->as.call.nargs; i++) ast_free(a->as.call.args[i]);
            bmm_free(a->as.call.args);
            break;
        case AST_INDEX:
            ast_free(a->as.idx.base);
            ast_free(a->as.idx.index);
            break;
        case AST_ARRAY:
            for (int i = 0; i < a->as.arr.nelems; i++) ast_free(a->as.arr.elems[i]);
            bmm_free(a->as.arr.elems);
            break;
        case AST_LET:
            free_str(a->as.let.name);
            ast_free(a->as.let.init);
            break;
        case AST_ASSIGN:
            ast_free(a->as.assign.target);
            ast_free(a->as.assign.value);
            break;
        case AST_FN:
            free_str(a->as.fn.name);
            for (int i = 0; i < a->as.fn.nparams; i++) free_str(a->as.fn.params[i]);
            bmm_free(a->as.fn.params);
            ast_free(a->as.fn.body);
            break;
        case AST_IF:
            ast_free(a->as.iff.cond);
            ast_free(a->as.iff.thenb);
            ast_free(a->as.iff.elseb);
            break;
        case AST_LOOP:
            ast_free(a->as.loop.cond);
            ast_free(a->as.loop.body);
            break;
        case AST_BREAK: break;
        case AST_RET:
            ast_free(a->as.ret.value);
            break;
        case AST_USE:
            free_str(a->as.use.path);
            break;
        case AST_EXPRSTMT:
            ast_free(a->as.exprstmt.expr);
            break;
        case AST_BLOCK:
            for (int i = 0; i < a->as.block.nstmts; i++) ast_free(a->as.block.stmts[i]);
            bmm_free(a->as.block.stmts);
            break;
    }
    bmm_free(a);
}
