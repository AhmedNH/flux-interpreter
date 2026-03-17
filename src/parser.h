#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    Lexer lexer;
    Token current;
    Token next;
    int   had_error;
} Parser;

void   parser_init   (Parser *p, const char *src);
void   parser_cleanup(Parser *p);
Node **parser_parse  (Parser *p, int *out_count); /* malloc'd array; caller frees */

#endif /* PARSER_H */
