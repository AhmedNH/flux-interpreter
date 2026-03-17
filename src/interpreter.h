#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "ast.h"

/* ── Runtime value types ──────────────────────────────────────── */

typedef enum {
    VAL_NUMBER,
    VAL_STRING,
    VAL_BOOL,
    VAL_NULL,
} ValueType;

typedef struct {
    ValueType type;
    union {
        double  number;
        char   *string;  /* heap-allocated; owned by value */
        int     boolean;
    };
} Value;

/* ── Scope / environment ──────────────────────────────────────── */

typedef struct Env Env;
struct Env {
    char  **names;
    Value  *values;
    int     count;
    int     capacity;
    Env    *parent;
};

/* ── Function registry ────────────────────────────────────────── */

typedef struct {
    char  *name;
    char **params;
    int    param_count;
    Node  *body;        /* borrowed from AST; freed by interp_free */
} FnEntry;

/* ── Interpreter ──────────────────────────────────────────────── */

typedef struct {
    Env     *globals;
    FnEntry *fns;
    int      fn_count;
    int      fn_cap;
    int      had_error;
} Interpreter;

void interp_init(Interpreter *interp);
void interp_free(Interpreter *interp);
void interp_run (Interpreter *interp, Node **stmts, int count);

#endif /* INTERPRETER_H */
