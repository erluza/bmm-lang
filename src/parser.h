#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    Lexer lx;
    const char *filename;
    const char *src;
    int pos; /* index into lx.toks (lx.pos used) */
    int had_error;
    char *error_msg;
    int error_line, error_col;
} Parser;

int parse_source(const char *src, const char *filename, Ast **out_block, Parser *p);
/* out_block is an AST_BLOCK */

#endif
