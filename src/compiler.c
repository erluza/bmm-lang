#include "compiler.h"
#include "lexer.h"
#include "parser.h"
#include "bmm_config.h"
#include "pal/pal.h"
#include <string.h>

static char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *b = bmm_alloc(n + 1);
    memcpy(b, s, n + 1);
    return b;
}

static char *find_last_slash(char *s) {
    char *r = NULL;
    for (char *p = s; *p; p++) if (*p == '/') r = p;
    return r;
}
#include <string.h>

typedef struct { unsigned char *buf; size_t len, cap; int *lines; int *cols; int cur_line; int cur_col; } CodeBuf;
static void cb_init(CodeBuf *c){ c->buf=NULL; c->len=0; c->cap=0; c->lines=NULL; c->cols=NULL; c->cur_line=0; c->cur_col=0; }
static void cb_write(CodeBuf *c, unsigned char b){
    if (c->len >= c->cap){ c->cap = c->cap?c->cap*2:256; c->buf = bmm_realloc(c->buf, c->cap); c->lines = bmm_realloc(c->lines, c->cap*sizeof(int)); c->cols = bmm_realloc(c->cols, c->cap*sizeof(int)); }
    c->buf[c->len] = b;
    c->lines[c->len] = c->cur_line;
    c->cols[c->len] = c->cur_col;
    c->len++;
}
static void cb_u16(CodeBuf *c, unsigned int v){ cb_write(c, v & 0xFF); cb_write(c, (v>>8)&0xFF); }
static void cb_i32(CodeBuf *c, int v){ cb_write(c, v & 0xFF); cb_write(c, (v>>8)&0xFF); cb_write(c, (v>>16)&0xFF); cb_write(c, (v>>24)&0xFF); }

static size_t cb_pos(CodeBuf *c){ return c->len; }
static void cb_patch_i32(CodeBuf *c, size_t pos, int v){
    c->buf[pos] = v & 0xFF;
    c->buf[pos+1] = (v>>8)&0xFF;
    c->buf[pos+2] = (v>>16)&0xFF;
    c->buf[pos+3] = (v>>24)&0xFF;
}

typedef struct { int *patches; int np, cap; } LoopCtx;
typedef struct { char **names; int *slots; int count, cap; } Scope;
typedef struct {
    Scope scopes[64];
    int nscopes;
    int nlocals;
    CodeBuf *code;
    Compiler *comp;
    LoopCtx loops[64];
    int nloops;
    const char *basedir;
    int top_level;
} Ctx;

static void scope_push(Ctx *ctx){ ctx->scopes[ctx->nscopes].names=NULL; ctx->scopes[ctx->nscopes].slots=NULL; ctx->scopes[ctx->nscopes].count=0; ctx->scopes[ctx->nscopes].cap=0; ctx->nscopes++; }
static void loop_push(Ctx *ctx){ ctx->loops[ctx->nloops].patches=NULL; ctx->loops[ctx->nloops].np=0; ctx->loops[ctx->nloops].cap=0; ctx->nloops++; }
static void scope_pop(Ctx *ctx){
    Scope *sc = &ctx->scopes[ctx->nscopes-1];
    for (int i = 0; i < sc->count; i++) bmm_free(sc->names[i]);
    bmm_free(sc->names);
    bmm_free(sc->slots);
    ctx->nscopes--;
}
static int scope_add(Ctx *ctx, const char *name, int slot){
    Scope *sc = &ctx->scopes[ctx->nscopes-1];
    if (sc->count >= sc->cap){ sc->cap = sc->cap?sc->cap*2:8; sc->names = bmm_realloc(sc->names, sc->cap*sizeof(char*)); sc->slots = bmm_realloc(sc->slots, sc->cap*sizeof(int)); }
    sc->names[sc->count] = xstrdup(name);
    sc->slots[sc->count] = slot;
    sc->count++;
    return slot;
}
static int find_local(Ctx *ctx, const char *name){
    for (int s = ctx->nscopes-1; s >= 0; s--){
        for (int i = 0; i < ctx->scopes[s].count; i++){
            if (strcmp(ctx->scopes[s].names[i], name)==0) return ctx->scopes[s].slots[i];
        }
    }
    return -1;
}

static int find_global(Compiler *c, const char *name){
    for (size_t i = 0; i < c->nglobals; i++) if (strcmp(c->global_names[i], name)==0) return (int)i;
    return -1;
}
static int add_global(Compiler *c, const char *name){
    int idx = find_global(c, name);
    if (idx >= 0) return idx;
    if (c->nglobals >= c->global_cap){
        c->global_cap = c->global_cap?c->global_cap*2:16;
        c->global_names = bmm_realloc(c->global_names, c->global_cap*sizeof(char*));
        c->global_defined_flags = bmm_realloc(c->global_defined_flags, c->global_cap*sizeof(char));
    }
    c->global_names[c->nglobals] = xstrdup(name);
    c->global_defined_flags[c->nglobals] = 0;
    return (int)c->nglobals++;
}

static int add_const_int(Compiler *c, long v){
    if (c->nconsts >= c->const_cap){ c->const_cap = c->const_cap?c->const_cap*2:64; c->consts = bmm_realloc(c->consts, c->const_cap*sizeof(Value)); }
    c->consts[c->nconsts] = val_int((int32_t)v);
    return (int)c->nconsts++;
}
static int add_const_str(Compiler *c, const char *s){
    if (c->nconsts >= c->const_cap){ c->const_cap = c->const_cap?c->const_cap*2:64; c->consts = bmm_realloc(c->consts, c->const_cap*sizeof(Value)); }
    ObjString *o = new_string(s, strlen(s));
    c->consts[c->nconsts] = val_str((Obj*)o);
    return (int)c->nconsts++;
}

static int builtin_id(const char *name){
    if (strcmp(name,"print")==0) return BI_PRINT;
    if (strcmp(name,"println")==0) return BI_PRINTLN;
    if (strcmp(name,"input")==0) return BI_INPUT;
    if (strcmp(name,"len")==0) return BI_LEN;
    if (strcmp(name,"push")==0) return BI_PUSH;
    if (strcmp(name,"pop")==0) return BI_POP;
    if (strcmp(name,"str")==0) return BI_STR;
    if (strcmp(name,"int")==0) return BI_INT;
    if (strcmp(name,"chr")==0) return BI_CHR;
    if (strcmp(name,"ord")==0) return BI_ORD;
    if (strcmp(name,"sub")==0) return BI_SUB;
    if (strcmp(name,"arr")==0) return BI_ARR;
    if (strcmp(name,"exit")==0) return BI_EXIT;
    if (strcmp(name,"clock")==0) return BI_CLOCK;
    return -1;
}

static int find_func(Compiler *c, const char *name){
    for (size_t i = 0; i < c->nfuncs; i++){
        if (c->funcs[i] && c->funcs[i]->name && strcmp(c->funcs[i]->name, name)==0) return (int)i;
    }
    return -1;
}

static void collect_functions(Compiler *c, Ast *node, const char *basedir);
static void collect_from_file(Compiler *c, const char *path) {
    char *buf = NULL; size_t len = 0;
    if (pal_read_file(path, &buf, &len) != 0) return;
    Ast *blk = NULL;
    Parser pp; memset(&pp,0,sizeof(pp));
    if (parse_source(buf, path, &blk, &pp) != 0) { lexer_free(&pp.lx); bmm_free(buf); return; }
    lexer_free(&pp.lx);
    char dir[4096]; strcpy(dir, path);
    char *slash = find_last_slash(dir);
    if (slash) *slash = '\0'; else strcpy(dir, ".");
    collect_functions(c, blk, dir);
    /* retain blk for function AST pointers */
    if (c->nextra >= c->extra_cap) { c->extra_cap = c->extra_cap ? c->extra_cap*2 : 8; c->extra_asts = bmm_realloc(c->extra_asts, c->extra_cap*sizeof(Ast*)); }
    c->extra_asts[c->nextra++] = blk;
    bmm_free(buf);
}
static void collect_functions(Compiler *c, Ast *node, const char *basedir) {
    if (!node) return;
    if (node->kind == AST_BLOCK) {
        for (int i = 0; i < node->as.block.nstmts; i++) collect_functions(c, node->as.block.stmts[i], basedir);
        return;
    }
    if (node->kind == AST_FN) {
        if (c->nfuncs >= c->func_cap){ c->func_cap = c->func_cap?c->func_cap*2:16; c->funcs = bmm_realloc(c->funcs, c->func_cap*sizeof(Func*)); }
        Func *f = bmm_alloc(sizeof(Func));
        memset(f, 0, sizeof(Func));
        f->name = xstrdup(node->as.fn.name);
        f->arity = node->as.fn.nparams;
        f->ast = node;
        c->funcs[c->nfuncs++] = f;
        collect_functions(c, node->as.fn.body, basedir);
        return;
    }
    if (node->kind == AST_IF) {
        collect_functions(c, node->as.iff.thenb, basedir);
        if (node->as.iff.elseb) collect_functions(c, node->as.iff.elseb, basedir);
        return;
    }
    if (node->kind == AST_LOOP) {
        collect_functions(c, node->as.loop.body, basedir);
        return;
    }
    if (node->kind == AST_USE) {
        static int use_depth = 0;
        if (use_depth > 32) return;
        use_depth++;
        char pathbuf[4096];
        /* build pathbuf = basedir + "/" + path */
        size_t bl = strlen(basedir);
        if (bl + 1 + strlen(node->as.use.path) + 1 > sizeof(pathbuf)) bl = 0;
        memcpy(pathbuf, basedir, bl);
        pathbuf[bl] = '/';
        strcpy(pathbuf + bl + 1, node->as.use.path);
        collect_from_file(c, pathbuf);
        use_depth--;
        return;
    }
}

/* compile */
static void compile_expr(Ctx *ctx, Ast *e);
static void compile_stmt(Ctx *ctx, Ast *s);
static void compile_block(Ctx *ctx, Ast *blk);

static void compile_block(Ctx *ctx, Ast *blk) {
    if (!blk || blk->kind != AST_BLOCK) return;
    scope_push(ctx);
    for (int i = 0; i < blk->as.block.nstmts; i++) compile_stmt(ctx, blk->as.block.stmts[i]);
    scope_pop(ctx);
}

static void compile_expr(Ctx *ctx, Ast *e) {
    if (!e) return;
    int _sl = ctx->code ? ctx->code->cur_line : 0;
    int _sc = ctx->code ? ctx->code->cur_col : 0;
    if (ctx->code) { ctx->code->cur_line = e->line; ctx->code->cur_col = e->col; }
    Compiler *c = ctx->comp;
    switch (e->kind) {
        case AST_NUM: {
            int idx = add_const_int(c, e->as.num.value);
            cb_write(ctx->code, OP_CONST); cb_u16(ctx->code, idx);
            break;
        }
        case AST_STR: {
            int idx = add_const_str(c, e->as.str.value);
            cb_write(ctx->code, OP_CONST); cb_u16(ctx->code, idx);
            break;
        }
        case AST_NAME: {
            int slot = find_local(ctx, e->as.name.name);
            if (slot >= 0) {
                cb_write(ctx->code, OP_LOAD_LOCAL); cb_u16(ctx->code, slot);
            } else {
                int gi = find_global(c, e->as.name.name);
                if (gi < 0) gi = add_global(c, e->as.name.name);
                cb_write(ctx->code, OP_LOAD_GLOBAL); cb_u16(ctx->code, gi);
            }
            break;
        }
        case AST_UNARY: {
            compile_expr(ctx, e->as.unary.operand);
            if (e->as.unary.op == 0) cb_write(ctx->code, OP_NEG);
            else if (e->as.unary.op == 2) cb_write(ctx->code, OP_NOT);
            break;
        }
        case AST_BIN: {
            if (e->as.bin.op == 12) { /* and */
                compile_expr(ctx, e->as.bin.left);
                size_t jpos = cb_pos(ctx->code);
                cb_write(ctx->code, OP_JUMP_IF_FALSE); cb_i32(ctx->code, 0);
                compile_expr(ctx, e->as.bin.right);
                cb_write(ctx->code, OP_NOT); cb_write(ctx->code, OP_NOT);
                size_t jpos2 = cb_pos(ctx->code);
                cb_write(ctx->code, OP_JUMP); cb_i32(ctx->code, 0);
                size_t target = cb_pos(ctx->code);
                cb_patch_i32(ctx->code, jpos+1, (unsigned int)(target - (jpos+5)));
                int zi = add_const_int(c, 0);
                cb_write(ctx->code, OP_CONST); cb_u16(ctx->code, zi);
                size_t end = cb_pos(ctx->code);
                cb_patch_i32(ctx->code, jpos2+1, (unsigned int)(end - (jpos2+5)));
                break;
            }
            if (e->as.bin.op == 13) { /* or */
                compile_expr(ctx, e->as.bin.left);
                size_t jpos = cb_pos(ctx->code);
                cb_write(ctx->code, OP_JUMP_IF_FALSE); cb_i32(ctx->code, 0);
                int oi = add_const_int(c, 1);
                cb_write(ctx->code, OP_CONST); cb_u16(ctx->code, oi);
                size_t jpos2 = cb_pos(ctx->code);
                cb_write(ctx->code, OP_JUMP); cb_i32(ctx->code, 0);
                size_t target = cb_pos(ctx->code);
                cb_patch_i32(ctx->code, jpos+1, (unsigned int)(target - (jpos+5)));
                compile_expr(ctx, e->as.bin.right);
                cb_write(ctx->code, OP_NOT); cb_write(ctx->code, OP_NOT);
                size_t end = cb_pos(ctx->code);
                cb_patch_i32(ctx->code, jpos2+1, (unsigned int)(end - (jpos2+5)));
                break;
            }
            compile_expr(ctx, e->as.bin.left);
            compile_expr(ctx, e->as.bin.right);
            switch (e->as.bin.op) {
                case 0: cb_write(ctx->code, OP_ADD); break;
                case 1: cb_write(ctx->code, OP_SUB); break;
                case 2: cb_write(ctx->code, OP_MUL); break;
                case 3: cb_write(ctx->code, OP_DIV); break;
                case 4: cb_write(ctx->code, OP_MOD); break;
                case 5: cb_write(ctx->code, OP_CONCAT); break;
                case 6: cb_write(ctx->code, OP_EQ); break;
                case 7: cb_write(ctx->code, OP_NE); break;
                case 8: cb_write(ctx->code, OP_LT); break;
                case 9: cb_write(ctx->code, OP_GT); break;
                case 10: cb_write(ctx->code, OP_LE); break;
                case 11: cb_write(ctx->code, OP_GE); break;
            }
            break;
        }
        case AST_CALL: {
            int bi = builtin_id(e->as.call.name);
            if (bi >= 0) {
                for (int i = 0; i < e->as.call.nargs; i++) compile_expr(ctx, e->as.call.args[i]);
                cb_write(ctx->code, OP_CALL_BUILTIN); cb_write(ctx->code, (unsigned char)bi); cb_write(ctx->code, (unsigned char)e->as.call.nargs);
            } else {
                int fi = find_func(c, e->as.call.name);
                if (fi < 0) {
                    c->had_error = 1;
                    size_t nl = strlen(e->as.call.name);
                    char *msg = bmm_alloc(nl + 32);
                    const char *pre = "'";
                    size_t mi = 0;
                    while (pre[mi]) { msg[mi] = pre[mi]; mi++; }
                    for (size_t q = 0; q < nl; q++) msg[mi++] = e->as.call.name[q];
                    const char *post = "' is not a function";
                    for (size_t q = 0; post[q]; q++) msg[mi++] = post[q];
                    msg[mi] = '\0';
                    c->error_msg = msg;
                    c->error_line = e->line;
                    c->error_col = e->col;
                    return;
                }
                for (int i = 0; i < e->as.call.nargs; i++) compile_expr(ctx, e->as.call.args[i]);
                cb_write(ctx->code, OP_CALL); cb_u16(ctx->code, fi); cb_write(ctx->code, (unsigned char)e->as.call.nargs);
            }
            break;
        }
        case AST_INDEX: {
            compile_expr(ctx, e->as.idx.base);
            compile_expr(ctx, e->as.idx.index);
            cb_write(ctx->code, OP_INDEX_GET);
            break;
        }
        case AST_ARRAY: {
            for (int i = 0; i < e->as.arr.nelems; i++) compile_expr(ctx, e->as.arr.elems[i]);
            cb_write(ctx->code, OP_ARRAY_NEW); cb_u16(ctx->code, e->as.arr.nelems);
            break;
        }
        default: break;
    }
    if (ctx->code) { ctx->code->cur_line = _sl; ctx->code->cur_col = _sc; }
}

static void compile_stmt(Ctx *ctx, Ast *s) {
    if (!s) return;
    int _sl = ctx->code ? ctx->code->cur_line : 0;
    int _sc = ctx->code ? ctx->code->cur_col : 0;
    if (ctx->code) { ctx->code->cur_line = s->line; ctx->code->cur_col = s->col; }
    Compiler *c = ctx->comp;
    switch (s->kind) {
        case AST_LET: {
            compile_expr(ctx, s->as.let.init);
            if (ctx->top_level) {
                int gi = add_global(c, s->as.let.name);
                c->global_defined_flags[gi] = 1;
                cb_write(ctx->code, OP_DECLARE_GLOBAL); cb_u16(ctx->code, gi);
            } else {
                int slot = ctx->nlocals++;
                scope_add(ctx, s->as.let.name, slot);
                cb_write(ctx->code, OP_STORE_LOCAL); cb_u16(ctx->code, slot);
            }
            cb_write(ctx->code, OP_POP);
            break;
        }
        case AST_FN: {
            if (!ctx->top_level) {
                ctx->comp->had_error = 1;
                ctx->comp->error_msg = xstrdup("nested functions are not supported");
                ctx->comp->error_line = s->line;
                ctx->comp->error_col = s->col;
            }
            break;
        }
        case AST_IF: {
            compile_expr(ctx, s->as.iff.cond);
            size_t jpos = cb_pos(ctx->code);
            cb_write(ctx->code, OP_JUMP_IF_FALSE); cb_i32(ctx->code, 0);
            if (s->as.iff.thenb && s->as.iff.thenb->kind == AST_BLOCK) compile_block(ctx, s->as.iff.thenb);
            else if (s->as.iff.thenb) { scope_push(ctx); compile_stmt(ctx, s->as.iff.thenb); scope_pop(ctx); }
            if (s->as.iff.elseb) {
                size_t jpos2 = cb_pos(ctx->code);
                cb_write(ctx->code, OP_JUMP); cb_i32(ctx->code, 0);
                size_t target = cb_pos(ctx->code);
                cb_patch_i32(ctx->code, jpos+1, (unsigned int)(target - (jpos+5)));
                if (s->as.iff.elseb->kind == AST_BLOCK) compile_block(ctx, s->as.iff.elseb);
                else { scope_push(ctx); compile_stmt(ctx, s->as.iff.elseb); scope_pop(ctx); }
                size_t end = cb_pos(ctx->code);
                cb_patch_i32(ctx->code, jpos2+1, (unsigned int)(end - (jpos2+5)));
            } else {
                size_t target = cb_pos(ctx->code);
                cb_patch_i32(ctx->code, jpos+1, (unsigned int)(target - (jpos+5)));
            }
            break;
        }
        case AST_LOOP: {
            size_t start = cb_pos(ctx->code);
            if (s->as.loop.cond) {
                compile_expr(ctx, s->as.loop.cond);
                size_t jpos = cb_pos(ctx->code);
                cb_write(ctx->code, OP_JUMP_IF_FALSE); cb_i32(ctx->code, 0);
                loop_push(ctx); /* ensure slot */
                /* initialize loop ctx */
                ctx->loops[ctx->nloops-1].patches = NULL; ctx->loops[ctx->nloops-1].np=0; ctx->loops[ctx->nloops-1].cap=0;
                /* Actually push already incremented? loop_push increments nloops. Need push before. Let's adjust: we called loop_push after? Fix: push before body. */
                /* We'll restructure: call loop_push before compile body. But we already incremented? Let's just manage manually. */
                /* For safety, decrement and redo */
                ctx->nloops--;
                ctx->loops[ctx->nloops].patches=NULL; ctx->loops[ctx->nloops].np=0; ctx->loops[ctx->nloops].cap=0;
                ctx->nloops++;
                if (s->as.loop.body && s->as.loop.body->kind==AST_BLOCK) compile_block(ctx, s->as.loop.body);
                else if (s->as.loop.body) { scope_push(ctx); compile_stmt(ctx, s->as.loop.body); scope_pop(ctx); }
                cb_write(ctx->code, OP_JUMP); cb_i32(ctx->code, 0);
                size_t jmpPos = cb_pos(ctx->code)-4;
                cb_patch_i32(ctx->code, jmpPos, (unsigned int)(start - (jmpPos+4)));
                size_t end = cb_pos(ctx->code);
                cb_patch_i32(ctx->code, jpos+1, (unsigned int)(end - (jpos+5)));
                LoopCtx *lc = &ctx->loops[ctx->nloops-1];
                for (int b = 0; b < lc->np; b++) {
                    cb_patch_i32(ctx->code, lc->patches[b], (unsigned int)(end - (lc->patches[b]+4)));
                }
                bmm_free(lc->patches);
                ctx->nloops--;
            } else {
                ctx->loops[ctx->nloops].patches=NULL; ctx->loops[ctx->nloops].np=0; ctx->loops[ctx->nloops].cap=0;
                ctx->nloops++;
                size_t bodyStart = cb_pos(ctx->code);
                if (s->as.loop.body && s->as.loop.body->kind==AST_BLOCK) compile_block(ctx, s->as.loop.body);
                else if (s->as.loop.body) { scope_push(ctx); compile_stmt(ctx, s->as.loop.body); scope_pop(ctx); }
                cb_write(ctx->code, OP_JUMP); cb_i32(ctx->code, 0);
                size_t jmpPos = cb_pos(ctx->code)-4;
                cb_patch_i32(ctx->code, jmpPos, (unsigned int)(bodyStart - (jmpPos+4)));
                size_t end = cb_pos(ctx->code);
                LoopCtx *lc = &ctx->loops[ctx->nloops-1];
                for (int b = 0; b < lc->np; b++) {
                    cb_patch_i32(ctx->code, lc->patches[b], (unsigned int)(end - (lc->patches[b]+4)));
                }
                bmm_free(lc->patches);
                ctx->nloops--;
            }
            break;
        }
        case AST_BREAK: {
            if (ctx->nloops <= 0) {
                ctx->comp->had_error = 1;
                ctx->comp->error_msg = xstrdup("break outside loop");
                ctx->comp->error_line = s->line;
                ctx->comp->error_col = s->col;
                break;
            }
            LoopCtx *lc = &ctx->loops[ctx->nloops-1];
            size_t pos = cb_pos(ctx->code);
            cb_write(ctx->code, OP_JUMP); cb_i32(ctx->code, 0);
            size_t operandPos = pos+1;
            if (lc->np >= lc->cap){ lc->cap = lc->cap?lc->cap*2:8; lc->patches = bmm_realloc(lc->patches, lc->cap*sizeof(int)); }
            lc->patches[lc->np++] = (int)operandPos;
            break;
        }
        case AST_RET: {
            if (ctx->top_level) {
                ctx->comp->had_error = 1;
                ctx->comp->error_msg = xstrdup("ret outside function");
                ctx->comp->error_line = s->line;
                ctx->comp->error_col = s->col;
                break;
            }
            if (s->as.ret.value) {
                compile_expr(ctx, s->as.ret.value);
                cb_write(ctx->code, OP_RET_VAL);
            } else {
                cb_write(ctx->code, OP_RET);
            }
            break;
        }
        case AST_USE: {
            char pathbuf[4096];
            const char *bd = ctx->basedir ? ctx->basedir : ".";
            size_t bll = strlen(bd);
            if (bll + 1 + strlen(s->as.use.path) + 1 <= sizeof(pathbuf)) {
                memcpy(pathbuf, bd, bll);
                pathbuf[bll] = '/';
                strcpy(pathbuf + bll + 1, s->as.use.path);
            } else {
                pathbuf[0] = '\0';
            }
            for (int u = 0; u < ctx->comp->nactive; u++) {
                if (strcmp(ctx->comp->active_uses[u], pathbuf) == 0) {
                    ctx->comp->had_error = 1;
                    ctx->comp->error_msg = xstrdup("circular use");
                    ctx->comp->error_line = s->line;
                    ctx->comp->error_col = s->col;
                    break;
                }
            }
            if (ctx->comp->had_error) break;
            char *buf = NULL; size_t len = 0;
            if (pal_read_file(pathbuf, &buf, &len) != 0) {
                if (pal_read_file(s->as.use.path, &buf, &len) != 0) {
                    ctx->comp->had_error = 1;
                    ctx->comp->error_msg = xstrdup("cannot open file");
                    ctx->comp->error_line = s->line;
                    ctx->comp->error_col = s->col;
                    break;
                }
                strcpy(pathbuf, s->as.use.path);
            }
            if (ctx->comp->nactive < 64) ctx->comp->active_uses[ctx->comp->nactive++] = xstrdup(pathbuf);
            Ast *blk = NULL;
            Parser pp; memset(&pp,0,sizeof(pp));
            if (parse_source(buf, pathbuf, &blk, &pp) != 0) { lexer_free(&pp.lx); bmm_free(buf); break; }
            lexer_free(&pp.lx);
            if (blk && blk->kind == AST_BLOCK) {
                char dir[4096]; strcpy(dir, pathbuf);
                char *slash = find_last_slash(dir); if (slash) *slash='\0'; else strcpy(dir,".");
                const char *oldbd = ctx->basedir;
                int oldtop = ctx->top_level;
                ctx->basedir = dir;
                for (int i = 0; i < blk->as.block.nstmts; i++) compile_stmt(ctx, blk->as.block.stmts[i]);
                ctx->basedir = oldbd;
                ctx->top_level = oldtop;
            }
            if (ctx->comp->nactive > 0) {
                ctx->comp->nactive--;
                bmm_free(ctx->comp->active_uses[ctx->comp->nactive]);
                ctx->comp->active_uses[ctx->comp->nactive] = NULL;
            }
            ast_free(blk);
            bmm_free(buf);
            break;
        }
        case AST_ASSIGN: {
            Ast *target = s->as.assign.target;
            if (target->kind == AST_NAME) {
                compile_expr(ctx, s->as.assign.value);
                int slot = find_local(ctx, target->as.name.name);
                if (slot >= 0) {
                    cb_write(ctx->code, OP_STORE_LOCAL); cb_u16(ctx->code, slot);
                } else {
                    int gi = find_global(c, target->as.name.name);
                    if (gi < 0) gi = add_global(c, target->as.name.name);
                    cb_write(ctx->code, OP_STORE_GLOBAL); cb_u16(ctx->code, gi);
                }
                cb_write(ctx->code, OP_POP);
            } else if (target->kind == AST_INDEX) {
                compile_expr(ctx, target->as.idx.base);
                compile_expr(ctx, target->as.idx.index);
                compile_expr(ctx, s->as.assign.value);
                cb_write(ctx->code, OP_INDEX_SET);
                cb_write(ctx->code, OP_POP);
            }
            break;
        }
        case AST_EXPRSTMT: {
            compile_expr(ctx, s->as.exprstmt.expr);
            cb_write(ctx->code, OP_POP);
            break;
        }
        case AST_BLOCK: {
            compile_block(ctx, s);
            break;
        }
        default: break;
    }
    if (ctx->code) { ctx->code->cur_line = _sl; ctx->code->cur_col = _sc; }
}

/* compile function body helper */
static void compile_one_function(Compiler *c, Func *f, const char *basedir) {
    Ast *node = (Ast*)f->ast;
    /* node is AST_FN */
    CodeBuf cb; cb_init(&cb);
    Ctx ctx; memset(&ctx, 0, sizeof(ctx));
    ctx.code = &cb;
    ctx.comp = c;
    ctx.basedir = basedir;
    ctx.top_level = 0;
    scope_push(&ctx);
    /* params */
    for (int i = 0; i < node->as.fn.nparams; i++) {
        int slot = ctx.nlocals++;
        scope_add(&ctx, node->as.fn.params[i], slot);
    }
    /* body is AST_BLOCK */
    if (node->as.fn.body && node->as.fn.body->kind == AST_BLOCK) {
        for (int i = 0; i < node->as.fn.body->as.block.nstmts; i++) compile_stmt(&ctx, node->as.fn.body->as.block.stmts[i]);
    } else if (node->as.fn.body) {
        compile_stmt(&ctx, node->as.fn.body);
    }
    scope_pop(&ctx);
    /* ensure return */
    cb_write(&cb, OP_RET);
    f->code = cb.buf;
    f->code_len = cb.len;
    f->lineinfo = cb.lines;
    f->colinfo = cb.cols;
    f->nlocals = ctx.nlocals;
    /* constants are module-level in c->consts; function shares c->consts. 
       But Func has its own consts pointer. VM uses func->consts. Need assign c->consts to f->consts? 
       We used module-level consts during compile; but each function's code references indices into c->consts. 
       Need copy pointer. */
    f->consts = c->consts;
    f->nconsts = c->nconsts;
    f->const_cap = c->const_cap;
}

int compile_program(Compiler *c, Ast *program, const char *filename) {
    memset(c, 0, sizeof(*c));
    c->filename = filename;
    char dirbuf[4096]; strcpy(dirbuf, filename ? filename : ".");
    char *slash = find_last_slash(dirbuf);
    if (slash) *slash = '\0'; else strcpy(dirbuf, ".");
    const char *basedir = dirbuf;
    /* reserve global slot 0 for argv so .bmc files can restore it without names */
    int argv_idx = add_global(c, "argv");
    c->global_defined_flags[argv_idx] = 1;
    collect_functions(c, program, basedir);
    /* compile user functions */
    for (size_t i = 0; i < c->nfuncs; i++) {
        compile_one_function(c, c->funcs[i], basedir);
    }
    /* compile main module as last function */
    /* create main func */
    if (c->nfuncs >= c->func_cap){ c->func_cap = c->func_cap?c->func_cap*2:16; c->funcs = bmm_realloc(c->funcs, c->func_cap*sizeof(Func*)); }
    Func *mainf = bmm_alloc(sizeof(Func));
    memset(mainf, 0, sizeof(Func));
    mainf->name = xstrdup("__main__");
    mainf->arity = 0;
    c->funcs[c->nfuncs] = mainf;
    c->main_index = (int)c->nfuncs;
    c->nfuncs++;
    CodeBuf cb; cb_init(&cb);
    Ctx ctx; memset(&ctx, 0, sizeof(ctx));
    ctx.code = &cb;
    ctx.comp = c;
    ctx.basedir = basedir;
    ctx.top_level = 1;
    scope_push(&ctx);
    if (program && program->kind == AST_BLOCK) {
        for (int i = 0; i < program->as.block.nstmts; i++) compile_stmt(&ctx, program->as.block.stmts[i]);
    } else if (program) {
        compile_stmt(&ctx, program);
    }
    scope_pop(&ctx);
    cb_write(&cb, OP_HALT);
    mainf->code = cb.buf;
    mainf->code_len = cb.len;
    mainf->lineinfo = cb.lines;
    mainf->colinfo = cb.cols;
    mainf->nlocals = ctx.nlocals;
    mainf->consts = c->consts;
    mainf->nconsts = c->nconsts;
    mainf->const_cap = c->const_cap;
    for (size_t i = 0; i < c->nfuncs; i++) {
        c->funcs[i]->consts = c->consts;
        c->funcs[i]->nconsts = c->nconsts;
        c->funcs[i]->const_cap = c->const_cap;
    }
    if (c->had_error) return -1;
    return 0;
}

int compile_repl_entry(Compiler *c, Ast *blk, const char *basedir, int *out_entry) {
    collect_functions(c, blk, basedir ? basedir : ".");
    for (size_t i = 0; i < c->nfuncs; i++) {
        if (c->funcs[i] && c->funcs[i]->code == NULL) {
            compile_one_function(c, c->funcs[i], basedir ? basedir : ".");
        }
    }
    if (c->nfuncs >= c->func_cap) {
        size_t nc = c->func_cap ? c->func_cap * 2 : 16;
        c->funcs = bmm_realloc(c->funcs, nc * sizeof(Func*));
        c->func_cap = nc;
    }
    Func *ef = bmm_alloc(sizeof(Func));
    memset(ef, 0, sizeof(Func));
    ef->name = xstrdup("__repl__");
    ef->arity = 0;
    c->funcs[c->nfuncs] = ef;
    int entry_idx = (int)c->nfuncs;
    c->nfuncs++;

    CodeBuf cb; cb_init(&cb);
    Ctx ctx; memset(&ctx, 0, sizeof(ctx));
    ctx.code = &cb;
    ctx.comp = c;
    ctx.basedir = basedir ? basedir : ".";
    ctx.top_level = 1;
    scope_push(&ctx);
    if (blk && blk->kind == AST_BLOCK) {
        for (int i = 0; i < blk->as.block.nstmts; i++) compile_stmt(&ctx, blk->as.block.stmts[i]);
    } else if (blk) {
        compile_stmt(&ctx, blk);
    }
    scope_pop(&ctx);
    cb_write(&cb, OP_RET);
    ef->code = cb.buf;
    ef->code_len = cb.len;
    ef->lineinfo = cb.lines;
    ef->colinfo = cb.cols;
    ef->nlocals = ctx.nlocals;
    for (size_t i = 0; i < c->nfuncs; i++) {
        c->funcs[i]->consts = c->consts;
        c->funcs[i]->nconsts = c->nconsts;
        c->funcs[i]->const_cap = c->const_cap;
    }
    *out_entry = entry_idx;
    if (c->had_error) return -1;
    return 0;
}
