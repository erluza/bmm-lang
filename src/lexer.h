#ifndef LEXER_H
#define LEXER_H

typedef enum {
    TK_NUM, TK_STR, TK_IDENT, TK_KW, TK_OP, TK_COMMA, TK_SEMI,
    TK_LPAREN, TK_RPAREN, TK_LBRACKET, TK_RBRACKET, TK_COMMENT, TK_NL, TK_EOF
} TokenKind;

typedef struct {
    TokenKind kind;
    char *text;   /* for STR/IDENT/KW/OP */
    long num;     /* for NUM */
    int line, col;
} Token;

typedef struct {
    Token *toks;
    int ntoks;
    int cap;
    int pos;
} Lexer;

/* Lex src into a Lexer (allocated). Returns 0 on success. */
int lex_source(const char *src, const char *filename, Lexer *lx);
void lexer_free(Lexer *lx);

Token *lx_peek(Lexer *lx);
Token *lx_advance(Lexer *lx);

#endif
