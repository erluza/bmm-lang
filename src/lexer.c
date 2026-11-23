#include "lexer.h"
#include "bmm_config.h"
#include "gc.h"
#include <string.h>

static const char *KEYWORDS[] = {
    "let","fn","if","else","loop","break","ret","use","and","or","not","end", NULL
};

static int is_kw(const char *s) {
    for (int i = 0; KEYWORDS[i]; i++) if (strcmp(KEYWORDS[i], s) == 0) return 1;
    return 0;
}

static void set_lex_error(Lexer *lx, const char *msg, int line, int col) {
    size_t n = strlen(msg);
    if (n >= sizeof(lx->error_msg)) n = sizeof(lx->error_msg) - 1;
    memcpy(lx->error_msg, msg, n);
    lx->error_msg[n] = '\0';
    lx->error_line = line;
    lx->error_col = col;
}

static char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *b = bmm_alloc(n + 1);
    memcpy(b, s, n + 1);
    return b;
}

static int is_ident_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static void add_tok(Lexer *lx, TokenKind kind, char *text, long num, int line, int col) {
    if (lx->ntoks >= lx->cap) {
        lx->cap = lx->cap ? lx->cap*2 : 256;
        lx->toks = bmm_realloc(lx->toks, lx->cap * sizeof(Token));
    }
    Token *t = &lx->toks[lx->ntoks++];
    t->kind = kind;
    t->text = text ? text : NULL;
    t->num = num;
    t->line = line;
    t->col = col;
}

static char *dupn(const char *s, size_t n) {
    char *b = bmm_alloc(n+1);
    memcpy(b, s, n);
    b[n] = '\0';
    return b;
}

int lex_source(const char *src, const char *filename, Lexer *lx) {
    (void)filename;
    memset(lx, 0, sizeof(*lx));
    int i = 0;
    int n = (int)strlen(src);
    int line = 1, col = 1;
    while (i < n) {
        char c = src[i];
        if (c == '\n') {
            if (lx->ntoks > 0 && lx->toks[lx->ntoks-1].kind != TK_NL)
                add_tok(lx, TK_NL, xstrdup("\n"), 0, line, col);
            i++; line++; col = 1;
            continue;
        }
        if (c == '\r') {
            if (i+1 < n && src[i+1] == '\n') {
                if (lx->ntoks > 0 && lx->toks[lx->ntoks-1].kind != TK_NL)
                    add_tok(lx, TK_NL, xstrdup("\n"), 0, line, col);
                i += 2; line++; col = 1;
                continue;
            }
            i++; col++;
            continue;
        }
        if (c == ' ' || c == '\t') { i++; col++; continue; }
        if (c == '-' && i+1 < n && src[i+1] == '-') {
            int start = i, startcol = col;
            while (i < n && src[i] != '\n') { i++; col++; }
            char *w = dupn(src + start, i - start);
            add_tok(lx, TK_COMMENT, w, 0, line, startcol);
            continue;
        }
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            int start = i, startcol = col;
            while (i < n && is_ident_char(src[i])) { i++; col++; }
            char *w = dupn(src + start, i - start);
            if (is_kw(w)) add_tok(lx, TK_KW, w, 0, line, startcol);
            else add_tok(lx, TK_IDENT, w, 0, line, startcol);
            /* note: w owned by token; do not free */
            continue;
        }
        if (c >= '0' && c <= '9') {
            int start = i, startcol = col;
            while (i < n && (src[i] >= '0' && src[i] <= '9')) { i++; col++; }
            char *w = dupn(src + start, i - start);
            long nv = 0;
            for (char *q = w; *q; q++) nv = nv * 10 + (*q - '0');
            add_tok(lx, TK_NUM, w, nv, line, startcol);
            continue;
        }
        if (c == '"') {
            int startcol = col;
            i++; col++;
            /* build string */
            size_t cap = 64, len = 0;
            char *buf = bmm_alloc(cap);
            while (i < n && src[i] != '"') {
                if (src[i] == '\n') {
                    set_lex_error(lx, "unterminated string", line, startcol);
                    bmm_free(buf);
                    return -1;
                }
                if (src[i] == '\\') {
                    if (i+1 >= n) { set_lex_error(lx, "unterminated string", line, startcol); bmm_free(buf); return -1; }
                    char e = src[i+1];
                    char out;
                    if (e == 'n') out = '\n';
                    else if (e == 't') out = '\t';
                    else if (e == 'r') out = '\r';
                    else if (e == '\\') out = '\\';
                    else if (e == '"') out = '"';
                    else { set_lex_error(lx, "bad escape in string", line, col+1); bmm_free(buf); return -1; }
                    if (len + 2 >= cap) { cap *= 2; buf = bmm_realloc(buf, cap); }
                    buf[len++] = out;
                    i += 2; col += 2;
                } else {
                    if (len + 2 >= cap) { cap *= 2; buf = bmm_realloc(buf, cap); }
                    buf[len++] = src[i];
                    i++; col++;
                }
            }
            if (i >= n) { set_lex_error(lx, "unterminated string", line, startcol); bmm_free(buf); return -1; }
            i++; col++;
            buf[len] = '\0';
            /* store as text (may contain embedded NUL? no, \0 terminator; strings can contain? no escapes for NUL) */
            add_tok(lx, TK_STR, buf, 0, line, startcol);
            /* buf owned by token text */
            continue;
        }
        /* two-char ops */
        if (i+1 < n) {
            char two[3] = { src[i], src[i+1], '\0' };
            if (strcmp(two, "==")==0 || strcmp(two, "!=")==0 ||
                strcmp(two, "<=")==0 || strcmp(two, ">=")==0 ||
                strcmp(two, "..")==0) {
                char *t = dupn(two, 2);
                add_tok(lx, TK_OP, t, 0, line, col);
                i += 2; col += 2;
                continue;
            }
        }
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
            c == '<' || c == '>' || c == '=' || c == ',') {
            char buf2[2] = { c, '\0' };
            char *t = xstrdup(buf2);
            if (c == ',') add_tok(lx, TK_COMMA, t, 0, line, col);
            else add_tok(lx, TK_OP, t, 0, line, col);
            i++; col++;
            continue;
        }
        if (c == ';') {
            char *t = xstrdup(";");
            add_tok(lx, TK_SEMI, t, 0, line, col);
            i++; col++;
            continue;
        }
        if (c == '(') { add_tok(lx, TK_LPAREN, xstrdup("("), 0, line, col); i++; col++; continue; }
        if (c == ')') { add_tok(lx, TK_RPAREN, xstrdup(")"), 0, line, col); i++; col++; continue; }
        if (c == '[') { add_tok(lx, TK_LBRACKET, xstrdup("["), 0, line, col); i++; col++; continue; }
        if (c == ']') { add_tok(lx, TK_RBRACKET, xstrdup("]"), 0, line, col); i++; col++; continue; }
        /* unknown */
        {
            char msg[64];
            const char *pre = "unexpected character '";
            size_t mi = 0;
            while (pre[mi]) { msg[mi] = pre[mi]; mi++; }
            if (c == '\\') { msg[mi++] = '\\'; msg[mi++] = '\\'; }
            else if (c == '\'') { msg[mi++] = '\\'; msg[mi++] = '\''; }
            else msg[mi++] = c;
            msg[mi++] = '\'';
            msg[mi] = '\0';
            set_lex_error(lx, msg, line, col);
        }
        return -1;
    }
    if (lx->ntoks > 0 && lx->toks[lx->ntoks-1].kind != TK_NL)
        add_tok(lx, TK_NL, xstrdup("\n"), 0, line, col);
    add_tok(lx, TK_EOF, NULL, 0, line, col);
    lx->pos = 0;
    return 0;
}

void lexer_free(Lexer *lx) {
    for (int i = 0; i < lx->ntoks; i++) {
        if (lx->toks[i].text) bmm_free(lx->toks[i].text);
    }
    bmm_free(lx->toks);
    memset(lx, 0, sizeof(*lx));
}

Token *lx_peek(Lexer *lx) { return &lx->toks[lx->pos]; }
Token *lx_advance(Lexer *lx) { return &lx->toks[lx->pos++]; }
