#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Error reporting ──────────────────────────────────────────── */

static void parse_error(Parser *p, const char *msg) {
    const char *got = p->current.value
                    ? p->current.value
                    : token_type_name(p->current.type);
    fprintf(stderr, "[line %d] Parse error: %s (got '%s')\n",
            p->current.line, msg, got);
    p->had_error = 1;
}

/* ── Token helpers ────────────────────────────────────────────── */

static void advance_tok(Parser *p) {
    token_free(&p->current);
    p->current = p->next;
    p->next    = lexer_next(&p->lexer);
}

void parser_init(Parser *p, const char *src) {
    lexer_init(&p->lexer, src);
    p->had_error     = 0;
    p->current.value = NULL;
    p->next.value    = NULL;
    p->current = lexer_next(&p->lexer);
    p->next    = lexer_next(&p->lexer);
}

void parser_cleanup(Parser *p) {
    token_free(&p->current);
    token_free(&p->next);
}

static int check(Parser *p, TokenType t)  { return p->current.type == t; }
static int peek_is(Parser *p, TokenType t){ return p->next.type    == t; }

static int match_tok(Parser *p, TokenType t) {
    if (check(p, t)) { advance_tok(p); return 1; }
    return 0;
}

static int expect(Parser *p, TokenType t, const char *msg) {
    if (!check(p, t)) { parse_error(p, msg); return 0; }
    advance_tok(p);
    return 1;
}

/* ── Forward declarations ─────────────────────────────────────── */

static Node *parse_expr (Parser *p);
static Node *parse_stmt (Parser *p);
static Node *parse_block(Parser *p);

/* ── Expression parsing (recursive descent / Pratt-style) ─────── */

static Node *parse_primary(Parser *p) {
    int line = p->current.line;

    if (check(p, TOK_NUMBER)) {
        double val = atof(p->current.value);
        advance_tok(p);
        return node_number(val, line);
    }

    if (check(p, TOK_STRING)) {
        Node *n = node_string(p->current.value, line);
        advance_tok(p);
        return n;
    }

    if (check(p, TOK_TRUE))  { advance_tok(p); return node_bool(1, line); }
    if (check(p, TOK_FALSE)) { advance_tok(p); return node_bool(0, line); }

    if (check(p, TOK_IDENT)) {
        char name[256];
        strncpy(name, p->current.value, 255);
        name[255] = '\0';
        advance_tok(p);

        /* function call: name ( args ) */
        if (match_tok(p, TOK_LPAREN)) {
            Node *args[64];
            int   ac = 0;
            if (!check(p, TOK_RPAREN)) {
                do {
                    if (ac >= 64) { parse_error(p, "too many arguments"); break; }
                    args[ac++] = parse_expr(p);
                } while (match_tok(p, TOK_COMMA));
            }
            expect(p, TOK_RPAREN, "expected ')' after arguments");
            return node_fn_call(name, args, ac, line);
        }

        return node_ident(name, line);
    }

    if (match_tok(p, TOK_LPAREN)) {
        Node *inner = parse_expr(p);
        expect(p, TOK_RPAREN, "expected ')'");
        return inner;
    }

    parse_error(p, "expected an expression");
    advance_tok(p);
    return node_number(0.0, line); /* error recovery */
}

static Node *parse_unary(Parser *p) {
    int line = p->current.line;
    if (check(p, TOK_BANG)) {
        advance_tok(p);
        return node_unop("!", parse_unary(p), line);
    }
    if (check(p, TOK_MINUS)) {
        advance_tok(p);
        return node_unop("-", parse_unary(p), line);
    }
    return parse_primary(p);
}

static Node *parse_mul(Parser *p) {
    Node *left = parse_unary(p);
    while (check(p, TOK_STAR) || check(p, TOK_SLASH) || check(p, TOK_PERCENT)) {
        char op[4];
        strncpy(op, token_type_name(p->current.type), 3);
        op[3] = '\0';
        int line = p->current.line;
        advance_tok(p);
        left = node_binop(op, left, parse_unary(p), line);
    }
    return left;
}

static Node *parse_add(Parser *p) {
    Node *left = parse_mul(p);
    while (check(p, TOK_PLUS) || check(p, TOK_MINUS)) {
        char op[4];
        strncpy(op, token_type_name(p->current.type), 3);
        op[3] = '\0';
        int line = p->current.line;
        advance_tok(p);
        left = node_binop(op, left, parse_mul(p), line);
    }
    return left;
}

static Node *parse_cmp(Parser *p) {
    Node *left = parse_add(p);
    while (check(p,TOK_LT)||check(p,TOK_GT)||check(p,TOK_LEQ)||check(p,TOK_GEQ)) {
        char op[4];
        strncpy(op, token_type_name(p->current.type), 3);
        op[3] = '\0';
        int line = p->current.line;
        advance_tok(p);
        left = node_binop(op, left, parse_add(p), line);
    }
    return left;
}

static Node *parse_eq(Parser *p) {
    Node *left = parse_cmp(p);
    while (check(p, TOK_EQ) || check(p, TOK_NEQ)) {
        char op[4];
        strncpy(op, token_type_name(p->current.type), 3);
        op[3] = '\0';
        int line = p->current.line;
        advance_tok(p);
        left = node_binop(op, left, parse_cmp(p), line);
    }
    return left;
}

static Node *parse_and(Parser *p) {
    Node *left = parse_eq(p);
    while (check(p, TOK_AND)) {
        int line = p->current.line;
        advance_tok(p);
        left = node_binop("&&", left, parse_eq(p), line);
    }
    return left;
}

static Node *parse_or(Parser *p) {
    Node *left = parse_and(p);
    while (check(p, TOK_OR)) {
        int line = p->current.line;
        advance_tok(p);
        left = node_binop("||", left, parse_and(p), line);
    }
    return left;
}

/*
 * parse_expr — top of expression precedence hierarchy.
 * Handles variable reassignment: IDENT '=' expr
 */
static Node *parse_expr(Parser *p) {
    if (check(p, TOK_IDENT) && peek_is(p, TOK_ASSIGN)) {
        int  line = p->current.line;
        char name[256];
        strncpy(name, p->current.value, 255);
        name[255] = '\0';
        advance_tok(p); /* consume IDENT */
        advance_tok(p); /* consume '='   */
        return node_assign(name, parse_expr(p), line);
    }
    return parse_or(p);
}

/* ── Statement parsing ────────────────────────────────────────── */

static Node *parse_block(Parser *p) {
    int   line  = p->current.line;
    Node *stmts[512];
    int   count = 0;

    expect(p, TOK_LBRACE, "expected '{'");
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF) && !p->had_error) {
        if (count >= 512) { parse_error(p, "block too large"); break; }
        stmts[count++] = parse_stmt(p);
    }
    expect(p, TOK_RBRACE, "expected '}'");
    return node_block(stmts, count, line);
}

static Node *parse_stmt(Parser *p) {
    int line = p->current.line;

    /* let x = expr ; */
    if (match_tok(p, TOK_LET)) {
        if (!check(p, TOK_IDENT)) {
            parse_error(p, "expected variable name after 'let'");
            return node_number(0, line);
        }
        char name[256];
        strncpy(name, p->current.value, 255);
        name[255] = '\0';
        advance_tok(p);
        expect(p, TOK_ASSIGN, "expected '=' after variable name");
        Node *val = parse_expr(p);
        expect(p, TOK_SEMICOLON, "expected ';'");
        return node_let(name, val, line);
    }

    /* if ( cond ) block [ else block ] */
    if (match_tok(p, TOK_IF)) {
        expect(p, TOK_LPAREN, "expected '(' after 'if'");
        Node *cond = parse_expr(p);
        expect(p, TOK_RPAREN, "expected ')' after condition");
        Node *then_br = parse_block(p);
        Node *else_br = NULL;
        if (match_tok(p, TOK_ELSE)) else_br = parse_block(p);
        return node_if(cond, then_br, else_br, line);
    }

    /* while ( cond ) block */
    if (match_tok(p, TOK_WHILE)) {
        expect(p, TOK_LPAREN, "expected '(' after 'while'");
        Node *cond = parse_expr(p);
        expect(p, TOK_RPAREN, "expected ')' after condition");
        Node *body = parse_block(p);
        return node_while(cond, body, line);
    }

    /* fn name ( params ) block */
    if (match_tok(p, TOK_FN)) {
        if (!check(p, TOK_IDENT)) {
            parse_error(p, "expected function name after 'fn'");
            return node_number(0, line);
        }
        char name[256];
        strncpy(name, p->current.value, 255);
        name[255] = '\0';
        advance_tok(p);

        expect(p, TOK_LPAREN, "expected '(' after function name");
        char *params[64];
        int   pc = 0;
        if (!check(p, TOK_RPAREN)) {
            do {
                if (!check(p, TOK_IDENT)) { parse_error(p, "expected parameter name"); break; }
                if (pc >= 64)             { parse_error(p, "too many parameters"); break; }
                params[pc++] = strdup(p->current.value);
                advance_tok(p);
            } while (match_tok(p, TOK_COMMA));
        }
        expect(p, TOK_RPAREN, "expected ')' after parameters");
        Node *body = parse_block(p);
        Node *fn   = node_fn_def(name, params, pc, body, line);
        for (int i = 0; i < pc; i++) free(params[i]);
        return fn;
    }

    /* return [ expr ] ; */
    if (match_tok(p, TOK_RETURN)) {
        Node *val = NULL;
        if (!check(p, TOK_SEMICOLON)) val = parse_expr(p);
        expect(p, TOK_SEMICOLON, "expected ';' after return");
        return node_return(val, line);
    }

    /* print ( expr ) ; */
    if (match_tok(p, TOK_PRINT)) {
        expect(p, TOK_LPAREN, "expected '(' after 'print'");
        Node *expr = parse_expr(p);
        expect(p, TOK_RPAREN, "expected ')'");
        expect(p, TOK_SEMICOLON, "expected ';'");
        return node_print(expr, line);
    }

    /* expression statement: expr ; */
    Node *expr = parse_expr(p);
    expect(p, TOK_SEMICOLON, "expected ';'");
    return expr;
}

/* ── Top-level parse ──────────────────────────────────────────── */

Node **parser_parse(Parser *p, int *out_count) {
    int    cap   = 64;
    Node **stmts = malloc(cap * sizeof(Node *));
    *out_count = 0;

    while (!check(p, TOK_EOF) && !p->had_error) {
        if (check(p, TOK_ERROR)) {
            parse_error(p, p->current.value ? p->current.value : "lex error");
            break;
        }
        if (*out_count >= cap) { cap *= 2; stmts = realloc(stmts, cap * sizeof(Node *)); }
        stmts[(*out_count)++] = parse_stmt(p);
    }

    return stmts;
}
