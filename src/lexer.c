#include "lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void lexer_init(Lexer *l, const char *src) {
    l->src  = src;
    l->pos  = 0;
    l->line = 1;
}

void token_free(Token *t) {
    free(t->value);
    t->value = NULL;
}

const char *token_type_name(TokenType t) {
    switch (t) {
        case TOK_NUMBER:    return "NUMBER";
        case TOK_STRING:    return "STRING";
        case TOK_IDENT:     return "IDENT";
        case TOK_TRUE:      return "true";
        case TOK_FALSE:     return "false";
        case TOK_LET:       return "let";
        case TOK_IF:        return "if";
        case TOK_ELSE:      return "else";
        case TOK_WHILE:     return "while";
        case TOK_FN:        return "fn";
        case TOK_RETURN:    return "return";
        case TOK_PRINT:     return "print";
        case TOK_PLUS:      return "+";
        case TOK_MINUS:     return "-";
        case TOK_STAR:      return "*";
        case TOK_SLASH:     return "/";
        case TOK_PERCENT:   return "%";
        case TOK_EQ:        return "==";
        case TOK_NEQ:       return "!=";
        case TOK_LT:        return "<";
        case TOK_GT:        return ">";
        case TOK_LEQ:       return "<=";
        case TOK_GEQ:       return ">=";
        case TOK_AND:       return "&&";
        case TOK_OR:        return "||";
        case TOK_BANG:      return "!";
        case TOK_ASSIGN:    return "=";
        case TOK_LPAREN:    return "(";
        case TOK_RPAREN:    return ")";
        case TOK_LBRACE:    return "{";
        case TOK_RBRACE:    return "}";
        case TOK_SEMICOLON: return ";";
        case TOK_COMMA:     return ",";
        case TOK_EOF:       return "EOF";
        case TOK_ERROR:     return "ERROR";
        default:            return "UNKNOWN";
    }
}

static char cur(Lexer *l)   { return l->src[l->pos]; }
static char peek2(Lexer *l) { return l->src[l->pos + 1]; }

static char advance(Lexer *l) {
    char c = l->src[l->pos++];
    if (c == '\n') l->line++;
    return c;
}

static int match_char(Lexer *l, char expected) {
    if (l->src[l->pos] == expected) { l->pos++; return 1; }
    return 0;
}

static Token make_tok(TokenType type, const char *val, int line) {
    Token t;
    t.type  = type;
    t.value = val ? strdup(val) : NULL;
    t.line  = line;
    return t;
}

/* ── Scan helpers ─────────────────────────────────────────────── */

static Token scan_number(Lexer *l, int line) {
    int start = l->pos - 1; /* first digit already consumed */
    while (isdigit(cur(l))) advance(l);
    if (cur(l) == '.' && isdigit(peek2(l))) {
        advance(l); /* consume '.' */
        while (isdigit(cur(l))) advance(l);
    }
    int  len = l->pos - start;
    char buf[64];
    if (len >= 63) len = 63;
    memcpy(buf, l->src + start, len);
    buf[len] = '\0';
    return make_tok(TOK_NUMBER, buf, line);
}

static Token scan_string(Lexer *l, int line) {
    /* opening '"' already consumed */
    int   cap = 64, sz = 0;
    char *buf = malloc(cap);

    while (cur(l) && cur(l) != '"') {
        char c = advance(l);
        if (c == '\\') {
            char esc = advance(l);
            switch (esc) {
                case 'n':  c = '\n'; break;
                case 't':  c = '\t'; break;
                case '"':  c = '"';  break;
                case '\\': c = '\\'; break;
                default:   c = esc;  break;
            }
        }
        if (sz + 1 >= cap) { cap *= 2; buf = realloc(buf, cap); }
        buf[sz++] = c;
    }
    buf[sz] = '\0';

    if (!cur(l)) {
        Token t = make_tok(TOK_ERROR, "unterminated string literal", line);
        free(buf);
        return t;
    }
    advance(l); /* consume closing '"' */

    Token t = make_tok(TOK_STRING, buf, line);
    free(buf);
    return t;
}

static Token scan_ident(Lexer *l, int line) {
    int start = l->pos - 1;
    while (isalnum(cur(l)) || cur(l) == '_') advance(l);
    int  len = l->pos - start;
    char buf[128];
    if (len >= 127) len = 127;
    memcpy(buf, l->src + start, len);
    buf[len] = '\0';

    /* keyword check */
    TokenType type = TOK_IDENT;
    if      (!strcmp(buf, "true"))   type = TOK_TRUE;
    else if (!strcmp(buf, "false"))  type = TOK_FALSE;
    else if (!strcmp(buf, "let"))    type = TOK_LET;
    else if (!strcmp(buf, "if"))     type = TOK_IF;
    else if (!strcmp(buf, "else"))   type = TOK_ELSE;
    else if (!strcmp(buf, "while"))  type = TOK_WHILE;
    else if (!strcmp(buf, "fn"))     type = TOK_FN;
    else if (!strcmp(buf, "return")) type = TOK_RETURN;
    else if (!strcmp(buf, "print"))  type = TOK_PRINT;

    return make_tok(type, (type == TOK_IDENT) ? buf : NULL, line);
}

/* ── Main tokenizer ───────────────────────────────────────────── */

Token lexer_next(Lexer *l) {
    /* skip whitespace and line comments */
    for (;;) {
        while (cur(l) && isspace(cur(l))) advance(l);
        if (cur(l) == '/' && peek2(l) == '/') {
            while (cur(l) && cur(l) != '\n') advance(l);
        } else {
            break;
        }
    }

    int line = l->line;
    if (!cur(l)) return make_tok(TOK_EOF, NULL, line);

    char c = advance(l);

    if (isdigit(c))             return scan_number(l, line);
    if (c == '"')               return scan_string(l, line);
    if (isalpha(c) || c == '_') return scan_ident(l, line);

    switch (c) {
        case '+': return make_tok(TOK_PLUS,     NULL, line);
        case '-': return make_tok(TOK_MINUS,    NULL, line);
        case '*': return make_tok(TOK_STAR,     NULL, line);
        case '/': return make_tok(TOK_SLASH,    NULL, line);
        case '%': return make_tok(TOK_PERCENT,  NULL, line);
        case '(': return make_tok(TOK_LPAREN,   NULL, line);
        case ')': return make_tok(TOK_RPAREN,   NULL, line);
        case '{': return make_tok(TOK_LBRACE,   NULL, line);
        case '}': return make_tok(TOK_RBRACE,   NULL, line);
        case ';': return make_tok(TOK_SEMICOLON,NULL, line);
        case ',': return make_tok(TOK_COMMA,    NULL, line);
        case '=': return make_tok(match_char(l,'=') ? TOK_EQ  : TOK_ASSIGN, NULL, line);
        case '!': return make_tok(match_char(l,'=') ? TOK_NEQ : TOK_BANG,   NULL, line);
        case '<': return make_tok(match_char(l,'=') ? TOK_LEQ : TOK_LT,     NULL, line);
        case '>': return make_tok(match_char(l,'=') ? TOK_GEQ : TOK_GT,     NULL, line);
        case '&':
            if (match_char(l, '&')) return make_tok(TOK_AND, NULL, line);
            break;
        case '|':
            if (match_char(l, '|')) return make_tok(TOK_OR, NULL, line);
            break;
    }

    char msg[40];
    snprintf(msg, sizeof(msg), "unexpected character '%c'", c);
    return make_tok(TOK_ERROR, msg, line);
}
