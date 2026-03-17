#ifndef LEXER_H
#define LEXER_H

typedef enum {
    /* Literals */
    TOK_NUMBER, TOK_STRING, TOK_IDENT,
    /* Keywords */
    TOK_TRUE, TOK_FALSE,
    TOK_LET, TOK_IF, TOK_ELSE, TOK_WHILE,
    TOK_FN, TOK_RETURN, TOK_PRINT,
    /* Arithmetic */
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    /* Comparison */
    TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT, TOK_LEQ, TOK_GEQ,
    /* Logical */
    TOK_AND, TOK_OR, TOK_BANG,
    /* Assignment */
    TOK_ASSIGN,
    /* Delimiters */
    TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACE, TOK_RBRACE,
    TOK_SEMICOLON, TOK_COMMA,
    /* Special */
    TOK_EOF, TOK_ERROR
} TokenType;

typedef struct {
    TokenType  type;
    char      *value;   /* heap-allocated; NULL for punctuation */
    int        line;
} Token;

typedef struct {
    const char *src;
    int         pos;
    int         line;
} Lexer;

void        lexer_init(Lexer *l, const char *src);
Token       lexer_next(Lexer *l);
void        token_free(Token *t);
const char *token_type_name(TokenType t);

#endif /* LEXER_H */
