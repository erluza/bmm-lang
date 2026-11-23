#include "fmt.h"
#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include "out.h"
#include "gc.h"
#include "pal/pal.h"
#include <string.h>

int fmt_file(const char *path) {
    char *buf = NULL; size_t len = 0;
    if (pal_read_file(path, &buf, &len) != 0) {
        OutBuf o; out_init(&o);
        out_puts(&o, "bmm: cannot read ");
        out_puts(&o, path);
        out_putc(&o, '\n');
        out_flush_stderr(&o);
        return 1;
    }
    Lexer lx; memset(&lx, 0, sizeof(lx));
    if (lex_source(buf, path, &lx) != 0) {
        OutBuf o; out_init(&o);
        out_puts(&o, "bmm: lex error\n");
        out_flush_stderr(&o);
        lexer_free(&lx);
        bmm_free(buf);
        return 1;
    }
    Ast *blk = NULL;
    Parser pp; memset(&pp, 0, sizeof(pp));
    if (parse_source(buf, path, &blk, &pp) != 0) {
        OutBuf o; out_init(&o);
        out_puts(&o, "bmm: ");
        out_puts(&o, path);
        out_putc(&o, ':');
        out_int(&o, pp.error_line);
        out_putc(&o, ':');
        out_int(&o, pp.error_col);
        out_puts(&o, ": error: ");
        out_puts(&o, pp.error_msg ? pp.error_msg : "parse error");
        out_putc(&o, '\n');
        out_flush_stderr(&o);
        lexer_free(&lx);
        bmm_free(pp.error_msg);
        ast_free(blk);
        bmm_free(buf);
        return 1;
    }
    lexer_free(&pp.lx);
    ast_free(blk);

    /* token-based formatting */
    OutBuf out; out_init(&out);
    int indent = 0;
    int at_line_start = 1;
    int last_was_nl = 0;
    int increase_on_nl = 0;
    for (int i = 0; i < lx.ntoks; i++) {
        Token *t = &lx.toks[i];
        if (t->kind == TK_EOF) break;
        if (t->kind == TK_NL) {
            if (last_was_nl) continue; /* collapse blank lines */
            if (increase_on_nl) { indent++; increase_on_nl = 0; }
            out_putc(&out, '\n');
            at_line_start = 1;
            last_was_nl = 1;
            continue;
        }
        last_was_nl = 0;
        /* dedent for end/else at line start */
        if (at_line_start && t->kind == TK_KW && t->text &&
            (strcmp(t->text, "end") == 0 || strcmp(t->text, "else") == 0)) {
            if (indent > 0) indent--;
        }
        if (at_line_start) {
            for (int j = 0; j < indent; j++) out_puts(&out, "  ");
            at_line_start = 0;
        }
        /* spacing before token */
        if (t->kind == TK_COMMENT) {
            if (out.len > 0 && out.buf[out.len-1] != ' ' && out.buf[out.len-1] != '\n')
                out_putc(&out, ' ');
        }
        if (t->kind == TK_STR) {
            out_putc(&out, '"');
            for (char *p = t->text ? t->text : ""; *p; p++) {
                if (*p == '\n') out_puts(&out, "\\n");
                else if (*p == '\t') out_puts(&out, "\\t");
                else if (*p == '\r') out_puts(&out, "\\r");
                else if (*p == '\\') out_puts(&out, "\\\\");
                else if (*p == '"') out_puts(&out, "\\\"");
                else out_putc(&out, *p);
            }
            out_putc(&out, '"');
        } else if (t->kind == TK_COMMENT) {
            if (out.len > 0 && out.buf[out.len-1] != ' ' && out.buf[out.len-1] != '\n')
                out_putc(&out, ' ');
            out_puts(&out, t->text ? t->text : "");
        } else {
            out_puts(&out, t->text ? t->text : "");
        }
        /* block openers */
        if (t->kind == TK_KW && t->text &&
            (strcmp(t->text, "fn") == 0 || strcmp(t->text, "if") == 0 ||
             strcmp(t->text, "loop") == 0)) {
            increase_on_nl = 1;
        }
        if (t->kind == TK_KW && t->text && strcmp(t->text, "else") == 0) {
            increase_on_nl = 1;
        }
        /* spacing after token */
        Token *next = (i + 1 < lx.ntoks) ? &lx.toks[i+1] : NULL;
        if (next && next->kind != TK_NL && next->kind != TK_EOF) {
            if (t->kind == TK_COMMA) {
                out_putc(&out, ' ');
            } else if (next->kind == TK_COMMA || next->kind == TK_RPAREN || next->kind == TK_RBRACKET) {
                /* no space */
            } else if (next->kind == TK_LPAREN && (t->kind == TK_IDENT || t->kind == TK_KW) && !(t->kind == TK_KW && t->text && strcmp(t->text, "not") == 0)) {
                /* function call: no space */
            } else if (t->kind == TK_LPAREN || t->kind == TK_LBRACKET) {
                /* no space */
            } else {
                out_putc(&out, ' ');
            }
        }
    }
    out_putc(&out, '\n');
    out_flush_stdout(&out);
    lexer_free(&lx);
    bmm_free(buf);
    return 0;
}
