#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include "gc.h"
#include <string.h>

static Token *peek(Parser *p) { return &p->lx.toks[p->lx.pos]; }
static Token *advance(Parser *p) { return &p->lx.toks[p->lx.pos++]; }
static int is_sep(Token *t) { return t->kind == TK_NL || t->kind == TK_SEMI || t->kind == TK_COMMENT; }
static void skip_sep(Parser *p) { while (is_sep(peek(p))) advance(p); }

static void set_err(Parser *p, Token *t, const char *msg) {
    if (p->had_error) return;
    p->had_error = 1;
    p->error_msg = strdup(msg);
    p->error_line = t->line;
    p->error_col = t->col;
}

static int expect(Parser *p, TokenKind kind, const char *val) {
    Token *t = peek(p);
    if (t->kind != kind || (val && (!t->text || strcmp(t->text, val) != 0))) {
        char buf[128];
        const char *prefix = "expected ";
        size_t pi = 0;
        while (prefix[pi]) { buf[pi] = prefix[pi]; pi++; }
        const char *w = val ? val : "token";
        for (size_t qi = 0; w[qi]; qi++) buf[pi++] = w[qi];
        buf[pi] = '\0';
        set_err(p, t, buf);
        return 0;
    }
    advance(p);
    return 1;
}

static Ast *parse_expr(Parser *p);
static Ast *parse_program(Parser *p);

static Ast *parse_primary(Parser *p);
static Ast *parse_postfix(Parser *p);

static Ast *parse_stmt(Parser *p) {
    skip_sep(p);
    if (p->had_error) return NULL;
    Token *t = peek(p);
    if (t->kind == TK_EOF) return NULL;
    if (t->kind == TK_KW && (strcmp(t->text,"end")==0 || strcmp(t->text,"else")==0)) return NULL;

    if (t->kind == TK_KW && strcmp(t->text,"let")==0) {
        Token *kw = t;
        advance(p);
        Token *id = peek(p);
        if (id->kind != TK_IDENT) { set_err(p, id, "expected identifier"); return NULL; }
        char *name = strdup(id->text);
        advance(p);
        if (!expect(p, TK_OP, "=")) return NULL;
        Ast *e = parse_expr(p);
        if (p->had_error) return NULL;
        Ast *a = ast_new(AST_LET, kw->line, kw->col);
        a->as.let.name = name;
        a->as.let.init = e;
        return a;
    }
    if (t->kind == TK_KW && strcmp(t->text,"fn")==0) {
        Token *kw = t;
        advance(p);
        Token *id = peek(p);
        if (id->kind != TK_IDENT) { set_err(p, id, "expected function name"); return NULL; }
        char *name = strdup(id->text);
        advance(p);
        if (!expect(p, TK_LPAREN, NULL)) return NULL;
        char **params = NULL;
        int nparams = 0, cap = 0;
        if (peek(p)->kind != TK_RPAREN) {
            if (peek(p)->kind != TK_IDENT) { set_err(p, peek(p), "expected parameter"); return NULL; }
            /* ensure capacity */
            if (nparams >= cap) { cap = cap?cap*2:4; params = bmm_realloc(params, cap*sizeof(char*)); }
            params[nparams++] = strdup(advance(p)->text);
            while (peek(p)->kind == TK_COMMA) {
                advance(p);
                if (peek(p)->kind != TK_IDENT) { set_err(p, peek(p), "expected parameter"); return NULL; }
                if (nparams >= cap) { cap = cap?cap*2:4; params = bmm_realloc(params, cap*sizeof(char*)); }
                params[nparams++] = strdup(advance(p)->text);
            }
        }
        if (!expect(p, TK_RPAREN, NULL)) return NULL;
        skip_sep(p);
        Ast *body = parse_program(p);
        if (p->had_error) return NULL;
        if (!(peek(p)->kind == TK_KW && peek(p)->text && strcmp(peek(p)->text,"end")==0)) {
            set_err(p, peek(p), "expected 'end'"); return NULL;
        }
        advance(p);
        Ast *a = ast_new(AST_FN, kw->line, kw->col);
        a->as.fn.name = name;
        a->as.fn.params = params;
        a->as.fn.nparams = nparams;
        a->as.fn.body = body;
        return a;
    }
    if (t->kind == TK_KW && strcmp(t->text,"if")==0) {
        Token *kw = t;
        advance(p);
        Ast *cond = parse_expr(p);
        if (p->had_error) return NULL;
        skip_sep(p);
        Ast *thenb = parse_program(p);
        if (p->had_error) return NULL;
        Ast *elseb = NULL;
        if (peek(p)->kind == TK_KW && peek(p)->text && strcmp(peek(p)->text,"else")==0) {
            advance(p);
            skip_sep(p);
            elseb = parse_program(p);
            if (p->had_error) return NULL;
        }
        if (!(peek(p)->kind == TK_KW && peek(p)->text && strcmp(peek(p)->text,"end")==0)) {
            set_err(p, peek(p), "expected 'end'"); return NULL;
        }
        advance(p);
        Ast *a = ast_new(AST_IF, kw->line, kw->col);
        a->as.iff.cond = cond;
        a->as.iff.thenb = thenb;
        a->as.iff.elseb = elseb;
        return a;
    }
    if (t->kind == TK_KW && strcmp(t->text,"loop")==0) {
        Token *kw = t;
        advance(p);
        Ast *cond = NULL;
        if (!(is_sep(peek(p)) || (peek(p)->kind==TK_KW && strcmp(peek(p)->text,"end")==0) || peek(p)->kind==TK_EOF)) {
            cond = parse_expr(p);
            if (p->had_error) return NULL;
        }
        skip_sep(p);
        Ast *body = parse_program(p);
        if (p->had_error) return NULL;
        if (!(peek(p)->kind == TK_KW && peek(p)->text && strcmp(peek(p)->text,"end")==0)) {
            set_err(p, peek(p), "expected 'end'"); return NULL;
        }
        advance(p);
        Ast *a = ast_new(AST_LOOP, kw->line, kw->col);
        a->as.loop.cond = cond;
        a->as.loop.body = body;
        return a;
    }
    if (t->kind == TK_KW && strcmp(t->text,"break")==0) {
        Token *kw = t;
        advance(p);
        Ast *a = ast_new(AST_BREAK, kw->line, kw->col);
        return a;
    }
    if (t->kind == TK_KW && strcmp(t->text,"ret")==0) {
        Token *kw = t;
        advance(p);
        if (is_sep(peek(p)) || (peek(p)->kind==TK_KW && (strcmp(peek(p)->text,"end")==0||strcmp(peek(p)->text,"else")==0)) || peek(p)->kind==TK_EOF) {
            Ast *a = ast_new(AST_RET, kw->line, kw->col);
            a->as.ret.value = NULL;
            return a;
        }
        Ast *e = parse_expr(p);
        if (p->had_error) return NULL;
        Ast *a = ast_new(AST_RET, kw->line, kw->col);
        a->as.ret.value = e;
        return a;
    }
    if (t->kind == TK_KW && strcmp(t->text,"use")==0) {
        Token *kw = t;
        advance(p);
        Token *s = peek(p);
        if (s->kind != TK_STR) { set_err(p, s, "expected string path"); return NULL; }
        char *path = strdup(s->text);
        advance(p);
        Ast *a = ast_new(AST_USE, kw->line, kw->col);
        a->as.use.path = path;
        return a;
    }

    /* expression or assignment */
    Ast *e = parse_expr(p);
    if (p->had_error) return NULL;
    if (peek(p)->kind == TK_OP && peek(p)->text && strcmp(peek(p)->text,"=")==0) {
        advance(p);
        Ast *rhs = parse_expr(p);
        if (p->had_error) return NULL;
        Ast *a = ast_new(AST_ASSIGN, e->line, e->col);
        a->as.assign.target = e;
        a->as.assign.value = rhs;
        return a;
    }
    Ast *a = ast_new(AST_EXPRSTMT, e->line, e->col);
    a->as.exprstmt.expr = e;
    return a;
}

static Ast *parse_program(Parser *p) {
    /* returns AST_BLOCK */
    Ast *blk = ast_new(AST_BLOCK, 1, 1);
    Ast **stmts = NULL;
    int n = 0, cap = 0;
    skip_sep(p);
    while (peek(p)->kind != TK_EOF) {
        if (peek(p)->kind == TK_KW && peek(p)->text && (strcmp(peek(p)->text,"end")==0 || strcmp(peek(p)->text,"else")==0)) break;
        Ast *s = parse_stmt(p);
        if (p->had_error) break;
        if (s == NULL) { skip_sep(p); continue; }
        if (n >= cap) { cap = cap?cap*2:16; stmts = bmm_realloc(stmts, cap*sizeof(Ast*)); }
        stmts[n++] = s;
        skip_sep(p);
    }
    blk->as.block.stmts = stmts;
    blk->as.block.nstmts = n;
    return blk;
}

/* precedence */

static Ast *parse_unary(Parser *p);
static Ast *parse_postfix(Parser *p) {
    Ast *base = parse_primary(p);
    if (p->had_error) return NULL;
    while (peek(p)->kind == TK_LBRACKET) {
        Token *t = advance(p);
        Ast *idx = parse_expr(p);
        if (p->had_error) return NULL;
        if (!expect(p, TK_RBRACKET, NULL)) return NULL;
        Ast *a = ast_new(AST_INDEX, t->line, t->col);
        a->as.idx.base = base;
        a->as.idx.index = idx;
        base = a;
    }
    return base;
}

static Ast *parse_primary(Parser *p) {
    Token *t = peek(p);
    if (t->kind == TK_NUM) {
        advance(p);
        Ast *a = ast_new(AST_NUM, t->line, t->col);
        a->as.num.value = t->num;
        return a;
    }
    if (t->kind == TK_STR) {
        advance(p);
        Ast *a = ast_new(AST_STR, t->line, t->col);
        a->as.str.value = strdup(t->text);
        return a;
    }
    if (t->kind == TK_IDENT) {
        advance(p);
        if (peek(p)->kind == TK_LPAREN) {
            advance(p);
            Ast **args = NULL;
            int nargs = 0, cap = 0;
            if (peek(p)->kind != TK_RPAREN) {
                Ast *e = parse_expr(p);
                if (p->had_error) return NULL;
                if (nargs >= cap) { cap = cap?cap*2:4; args = bmm_realloc(args, cap*sizeof(Ast*)); }
                args[nargs++] = e;
                while (peek(p)->kind == TK_COMMA) {
                    advance(p);
                    e = parse_expr(p);
                    if (p->had_error) return NULL;
                    if (nargs >= cap) { cap = cap?cap*2:4; args = bmm_realloc(args, cap*sizeof(Ast*)); }
                    args[nargs++] = e;
                }
            }
            if (!expect(p, TK_RPAREN, NULL)) return NULL;
            Ast *a = ast_new(AST_CALL, t->line, t->col);
            a->as.call.name = strdup(t->text);
            a->as.call.args = args;
            a->as.call.nargs = nargs;
            /* postfix indexing */
            while (peek(p)->kind == TK_LBRACKET) {
                Token *tt = advance(p);
                Ast *idx = parse_expr(p);
                if (p->had_error) return NULL;
                if (!expect(p, TK_RBRACKET, NULL)) return NULL;
                Ast *ix = ast_new(AST_INDEX, tt->line, tt->col);
                ix->as.idx.base = a;
                ix->as.idx.index = idx;
                a = ix;
            }
            return a;
        }
        Ast *a = ast_new(AST_NAME, t->line, t->col);
        a->as.name.name = strdup(t->text);
        /* postfix indexing */
        while (peek(p)->kind == TK_LBRACKET) {
            Token *tt = advance(p);
            Ast *idx = parse_expr(p);
            if (p->had_error) return NULL;
            if (!expect(p, TK_RBRACKET, NULL)) return NULL;
            Ast *ix = ast_new(AST_INDEX, tt->line, tt->col);
            ix->as.idx.base = a;
            ix->as.idx.index = idx;
            a = ix;
        }
        return a;
    }
    if (t->kind == TK_LBRACKET) {
        advance(p);
        Ast **elems = NULL;
        int ne = 0, cap = 0;
        if (peek(p)->kind != TK_RBRACKET) {
            Ast *e = parse_expr(p);
            if (p->had_error) return NULL;
            if (ne >= cap) { cap = cap?cap*2:4; elems = bmm_realloc(elems, cap*sizeof(Ast*)); }
            elems[ne++] = e;
            while (peek(p)->kind == TK_COMMA) {
                advance(p);
                if (peek(p)->kind == TK_RBRACKET) break;
                e = parse_expr(p);
                if (p->had_error) return NULL;
                if (ne >= cap) { cap = cap?cap*2:4; elems = bmm_realloc(elems, cap*sizeof(Ast*)); }
                elems[ne++] = e;
            }
        }
        if (!expect(p, TK_RBRACKET, NULL)) return NULL;
        Ast *a = ast_new(AST_ARRAY, t->line, t->col);
        a->as.arr.elems = elems;
        a->as.arr.nelems = ne;
        /* postfix indexing */
        while (peek(p)->kind == TK_LBRACKET) {
            Token *tt = advance(p);
            Ast *idx = parse_expr(p);
            if (p->had_error) return NULL;
            if (!expect(p, TK_RBRACKET, NULL)) return NULL;
            Ast *ix = ast_new(AST_INDEX, tt->line, tt->col);
            ix->as.idx.base = a;
            ix->as.idx.index = idx;
            a = ix;
        }
        return a;
    }
    if (t->kind == TK_LPAREN) {
        advance(p);
        Ast *e = parse_expr(p);
        if (p->had_error) return NULL;
        if (!expect(p, TK_RPAREN, NULL)) return NULL;
        /* postfix indexing */
        while (peek(p)->kind == TK_LBRACKET) {
            Token *tt = advance(p);
            Ast *idx = parse_expr(p);
            if (p->had_error) return NULL;
            if (!expect(p, TK_RBRACKET, NULL)) return NULL;
            Ast *ix = ast_new(AST_INDEX, tt->line, tt->col);
            ix->as.idx.base = e;
            ix->as.idx.index = idx;
            e = ix;
        }
        return e;
    }
    set_err(p, t, "unexpected token");
    return NULL;
}

static Ast *parse_unary(Parser *p) {
    Token *t = peek(p);
    if (t->kind == TK_OP && t->text && (strcmp(t->text,"-")==0 || strcmp(t->text,"+")==0)) {
        advance(p);
        Ast *operand = parse_unary(p);
        if (p->had_error) return NULL;
        Ast *a = ast_new(AST_UNARY, t->line, t->col);
        a->as.unary.op = (strcmp(t->text,"-")==0) ? 0 : 1; /* 0 neg, 1 pos */
        a->as.unary.operand = operand;
        /* encode op as int? We'll map later; store via text? Use op: 0 '-' 1 '+' */
        return a;
    }
    if (t->kind == TK_KW && strcmp(t->text,"not")==0) {
        advance(p);
        Ast *operand = parse_unary(p);
        if (p->had_error) return NULL;
        Ast *a = ast_new(AST_UNARY, t->line, t->col);
        a->as.unary.op = 2; /* not */
        a->as.unary.operand = operand;
        return a;
    }
    return parse_postfix(p);
}

/* We need bin op discrimination. Use op string stored? Ast bin has int op. Define mapping:
   0:+ 1:- 2:* 3:/ 4:% 5:.. 6:== 7:!= 8:< 9:> 10:<= 11:>= 12:and 13:or
   We'll store operator code. */

static int op_code(const char *s) {
    if (strcmp(s,"+")==0) return 0;
    if (strcmp(s,"-")==0) return 1;
    if (strcmp(s,"*")==0) return 2;
    if (strcmp(s,"/")==0) return 3;
    if (strcmp(s,"%")==0) return 4;
    if (strcmp(s,"..")==0) return 5;
    if (strcmp(s,"==")==0) return 6;
    if (strcmp(s,"!=")==0) return 7;
    if (strcmp(s,"<")==0) return 8;
    if (strcmp(s,">")==0) return 9;
    if (strcmp(s,"<=")==0) return 10;
    if (strcmp(s,">=")==0) return 11;
    return -1;
}

static Ast *parse_mul(Parser *p) {
    Ast *left = parse_unary(p);
    if (p->had_error) return NULL;
    while (peek(p)->kind == TK_OP && peek(p)->text && (strcmp(peek(p)->text,"*")==0||strcmp(peek(p)->text,"/")==0||strcmp(peek(p)->text,"%")==0)) {
        Token *t = advance(p);
        Ast *right = parse_unary(p);
        if (p->had_error) return NULL;
        Ast *a = ast_new(AST_BIN, t->line, t->col);
        a->as.bin.op = op_code(t->text);
        a->as.bin.left = left;
        a->as.bin.right = right;
        left = a;
    }
    return left;
}

static Ast *parse_add(Parser *p) {
    Ast *left = parse_mul(p);
    if (p->had_error) return NULL;
    while (peek(p)->kind == TK_OP && peek(p)->text && (strcmp(peek(p)->text,"+")==0||strcmp(peek(p)->text,"-")==0)) {
        Token *t = advance(p);
        Ast *right = parse_mul(p);
        if (p->had_error) return NULL;
        Ast *a = ast_new(AST_BIN, t->line, t->col);
        a->as.bin.op = op_code(t->text);
        a->as.bin.left = left;
        a->as.bin.right = right;
        left = a;
    }
    return left;
}

static Ast *parse_concat(Parser *p) {
    Ast *left = parse_add(p);
    if (p->had_error) return NULL;
    while (peek(p)->kind == TK_OP && peek(p)->text && strcmp(peek(p)->text,"..")==0) {
        Token *t = advance(p);
        Ast *right = parse_add(p);
        if (p->had_error) return NULL;
        Ast *a = ast_new(AST_BIN, t->line, t->col);
        a->as.bin.op = 5;
        a->as.bin.left = left;
        a->as.bin.right = right;
        left = a;
    }
    return left;
}

static Ast *parse_cmp(Parser *p) {
    Ast *left = parse_concat(p);
    if (p->had_error) return NULL;
    while (peek(p)->kind == TK_OP && peek(p)->text && (strcmp(peek(p)->text,"==")==0||strcmp(peek(p)->text,"!=")==0||strcmp(peek(p)->text,"<")==0||strcmp(peek(p)->text,">")==0||strcmp(peek(p)->text,"<=")==0||strcmp(peek(p)->text,">=")==0)) {
        Token *t = advance(p);
        Ast *right = parse_concat(p);
        if (p->had_error) return NULL;
        Ast *a = ast_new(AST_BIN, t->line, t->col);
        a->as.bin.op = op_code(t->text);
        a->as.bin.left = left;
        a->as.bin.right = right;
        left = a;
    }
    return left;
}

static Ast *parse_and(Parser *p) {
    Ast *left = parse_cmp(p);
    if (p->had_error) return NULL;
    while (peek(p)->kind == TK_KW && peek(p)->text && strcmp(peek(p)->text,"and")==0) {
        Token *t = advance(p);
        Ast *right = parse_cmp(p);
        if (p->had_error) return NULL;
        Ast *a = ast_new(AST_BIN, t->line, t->col);
        a->as.bin.op = 12;
        a->as.bin.left = left;
        a->as.bin.right = right;
        left = a;
    }
    return left;
}

static Ast *parse_or(Parser *p) {
    Ast *left = parse_and(p);
    if (p->had_error) return NULL;
    while (peek(p)->kind == TK_KW && peek(p)->text && strcmp(peek(p)->text,"or")==0) {
        Token *t = advance(p);
        Ast *right = parse_and(p);
        if (p->had_error) return NULL;
        Ast *a = ast_new(AST_BIN, t->line, t->col);
        a->as.bin.op = 13;
        a->as.bin.left = left;
        a->as.bin.right = right;
        left = a;
    }
    return left;
}

static Ast *parse_expr(Parser *p) { return parse_or(p); }

/* fix forward decls */
/* parse_primary defined above uses parse_expr, ok */

int parse_source(const char *src, const char *filename, Ast **out_block, Parser *p) {
    memset(p, 0, sizeof(*p));
    p->filename = filename;
    p->src = src;
    if (lex_source(src, filename, &p->lx) != 0) {
        p->had_error = 1;
        p->error_msg = strdup("lex error");
        p->error_line = 1;
        p->error_col = 1;
        return -1;
    }
    Ast *blk = parse_program(p);
    if (p->had_error) {
        return -1;
    }
    *out_block = blk;
    return 0;
}
