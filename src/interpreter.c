#include "interpreter.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Value helpers ────────────────────────────────────────────── */

static Value val_num (double n) { Value v; v.type = VAL_NUMBER; v.number  = n; return v; }
static Value val_str (const char *s) { Value v; v.type = VAL_STRING; v.string  = strdup(s); return v; }
static Value val_bool(int    b) { Value v; v.type = VAL_BOOL;   v.boolean = b; return v; }
static Value val_null(void)     { Value v; v.type = VAL_NULL;   v.number  = 0; return v; }

static void value_free(Value *v) {
    if (v->type == VAL_STRING) { free(v->string); v->string = NULL; }
}

/*
 * value_copy — deep copy a value (strings are strdup'd).
 * All values returned from eval() are owned by the caller.
 */
static Value value_copy(Value v) {
    if (v.type == VAL_STRING) return val_str(v.string);
    return v;
}

static int value_truthy(Value v) {
    switch (v.type) {
        case VAL_BOOL:   return v.boolean;
        case VAL_NUMBER: return v.number != 0.0;
        case VAL_STRING: return v.string && v.string[0] != '\0';
        default:         return 0;
    }
}

/* Convert any value to a heap-allocated string (caller must free). */
static char *value_to_str(Value v) {
    char buf[64];
    switch (v.type) {
        case VAL_STRING: return strdup(v.string);
        case VAL_BOOL:   return strdup(v.boolean ? "true" : "false");
        case VAL_NULL:   return strdup("null");
        case VAL_NUMBER:
            if (v.number == (double)(long long)v.number)
                snprintf(buf, sizeof(buf), "%lld", (long long)v.number);
            else
                snprintf(buf, sizeof(buf), "%g", v.number);
            return strdup(buf);
        default: return strdup("");
    }
}

static void value_print(Value v) {
    char *s = value_to_str(v);
    printf("%s\n", s);
    free(s);
}

/* ── Environment ──────────────────────────────────────────────── */

static Env *env_new(Env *parent) {
    Env *e      = calloc(1, sizeof(Env));
    e->parent   = parent;
    e->capacity = 8;
    e->names    = malloc(e->capacity * sizeof(char *));
    e->values   = malloc(e->capacity * sizeof(Value));
    return e;
}

static void env_free(Env *e) {
    for (int i = 0; i < e->count; i++) {
        free(e->names[i]);
        value_free(&e->values[i]);
    }
    free(e->names);
    free(e->values);
    free(e);
}

/* Look up a name; returns pointer into env storage or NULL. */
static Value *env_lookup(Env *e, const char *name) {
    for (Env *s = e; s; s = s->parent)
        for (int i = s->count - 1; i >= 0; i--)
            if (!strcmp(s->names[i], name))
                return &s->values[i];
    return NULL;
}

/* Define a new binding in the innermost scope (for `let`). */
static void env_define(Env *e, const char *name, Value val) {
    if (e->count >= e->capacity) {
        e->capacity *= 2;
        e->names  = realloc(e->names,  e->capacity * sizeof(char *));
        e->values = realloc(e->values, e->capacity * sizeof(Value));
    }
    e->names[e->count]    = strdup(name);
    e->values[e->count++] = val;  /* takes ownership */
}

/* Update an existing binding anywhere in the scope chain. */
static void env_set(Env *e, const char *name, Value val) {
    for (Env *s = e; s; s = s->parent)
        for (int i = s->count - 1; i >= 0; i--)
            if (!strcmp(s->names[i], name)) {
                value_free(&s->values[i]);
                s->values[i] = val;  /* takes ownership */
                return;
            }
    /* not found — define in current scope */
    env_define(e, name, val);
}

/* ── Interpreter init / free ──────────────────────────────────── */

void interp_init(Interpreter *interp) {
    interp->globals   = env_new(NULL);
    interp->fns       = NULL;
    interp->fn_count  = 0;
    interp->fn_cap    = 0;
    interp->had_error = 0;
}

void interp_free(Interpreter *interp) {
    env_free(interp->globals);
    for (int i = 0; i < interp->fn_count; i++) {
        free(interp->fns[i].name);
        for (int j = 0; j < interp->fns[i].param_count; j++)
            free(interp->fns[i].params[j]);
        free(interp->fns[i].params);
        node_free(interp->fns[i].body); /* owned here */
    }
    free(interp->fns);
}

static void runtime_error(Interpreter *interp, int line, const char *msg) {
    fprintf(stderr, "[line %d] Runtime error: %s\n", line, msg);
    interp->had_error = 1;
}

static FnEntry *find_fn(Interpreter *interp, const char *name) {
    for (int i = 0; i < interp->fn_count; i++)
        if (!strcmp(interp->fns[i].name, name))
            return &interp->fns[i];
    return NULL;
}

/* ── Return-value propagation ─────────────────────────────────── */
/*
 * When a `return` statement executes, it sets g_returning = 1 and
 * stores the returned value in g_return_val.  Every eval() that
 * checks g_returning short-circuits immediately, propagating the
 * signal up the call stack until NODE_FN_CALL captures it.
 */
static int   g_returning = 0;
static Value g_return_val;

/* ── Evaluate ─────────────────────────────────────────────────── */

static Value eval(Interpreter *interp, Env *env, Node *n);

static Value eval_binop(Interpreter *interp, Env *env, Node *n) {
    const char *op = n->binop.op;

    /* Short-circuit logical operators */
    if (!strcmp(op, "&&")) {
        Value l = eval(interp, env, n->binop.left);
        if (!value_truthy(l)) { value_free(&l); return val_bool(0); }
        value_free(&l);
        Value r = eval(interp, env, n->binop.right);
        int   t = value_truthy(r);
        value_free(&r);
        return val_bool(t);
    }
    if (!strcmp(op, "||")) {
        Value l = eval(interp, env, n->binop.left);
        if (value_truthy(l)) { value_free(&l); return val_bool(1); }
        value_free(&l);
        Value r = eval(interp, env, n->binop.right);
        int   t = value_truthy(r);
        value_free(&r);
        return val_bool(t);
    }

    Value left  = eval(interp, env, n->binop.left);
    if (interp->had_error) return val_null();
    Value right = eval(interp, env, n->binop.right);
    if (interp->had_error) { value_free(&left); return val_null(); }

    Value result = val_null();

    /* String concatenation with '+' */
    if (!strcmp(op, "+") && (left.type == VAL_STRING || right.type == VAL_STRING)) {
        char *ls = value_to_str(left);
        char *rs = value_to_str(right);
        char *cat = malloc(strlen(ls) + strlen(rs) + 1);
        strcpy(cat, ls);
        strcat(cat, rs);
        result = (Value){ .type = VAL_STRING, .string = cat };
        free(ls); free(rs);
        goto done;
    }

    /* Arithmetic and comparison (number operands) */
    if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
        double l = left.number, r = right.number;
        if      (!strcmp(op, "+"))  result = val_num(l + r);
        else if (!strcmp(op, "-"))  result = val_num(l - r);
        else if (!strcmp(op, "*"))  result = val_num(l * r);
        else if (!strcmp(op, "/")) {
            if (r == 0.0) { runtime_error(interp, n->line, "division by zero"); goto done; }
            result = val_num(l / r);
        }
        else if (!strcmp(op, "%")) {
            if (r == 0.0) { runtime_error(interp, n->line, "modulo by zero"); goto done; }
            result = val_num(fmod(l, r));
        }
        else if (!strcmp(op, "<"))  result = val_bool(l <  r);
        else if (!strcmp(op, ">"))  result = val_bool(l >  r);
        else if (!strcmp(op, "<=")) result = val_bool(l <= r);
        else if (!strcmp(op, ">=")) result = val_bool(l >= r);
        else if (!strcmp(op, "==")) result = val_bool(l == r);
        else if (!strcmp(op, "!=")) result = val_bool(l != r);
        goto done;
    }

    /* Equality for non-number types */
    if (!strcmp(op, "==")) {
        if (left.type != right.type) { result = val_bool(0); goto done; }
        if (left.type == VAL_STRING) { result = val_bool(!strcmp(left.string, right.string)); goto done; }
        if (left.type == VAL_BOOL)   { result = val_bool(left.boolean == right.boolean); goto done; }
        if (left.type == VAL_NULL)   { result = val_bool(1); goto done; }
    }
    if (!strcmp(op, "!=")) {
        if (left.type != right.type) { result = val_bool(1); goto done; }
        if (left.type == VAL_STRING) { result = val_bool(strcmp(left.string, right.string) != 0); goto done; }
        if (left.type == VAL_BOOL)   { result = val_bool(left.boolean != right.boolean); goto done; }
        if (left.type == VAL_NULL)   { result = val_bool(0); goto done; }
    }

    {
        char msg[64];
        snprintf(msg, sizeof(msg), "invalid operands for '%s'", op);
        runtime_error(interp, n->line, msg);
    }

done:
    value_free(&left);
    value_free(&right);
    return result;
}

static Value eval(Interpreter *interp, Env *env, Node *n) {
    if (!n || interp->had_error) return val_null();

    switch (n->type) {

        case NODE_NUMBER: return val_num(n->number);
        case NODE_STRING: return val_str(n->string);
        case NODE_BOOL:   return val_bool(n->boolean);

        case NODE_IDENT: {
            Value *v = env_lookup(env, n->ident);
            if (!v) {
                char msg[128];
                snprintf(msg, sizeof(msg), "undefined variable '%s'", n->ident);
                runtime_error(interp, n->line, msg);
                return val_null();
            }
            return value_copy(*v); /* caller owns the copy */
        }

        case NODE_BINOP: return eval_binop(interp, env, n);

        case NODE_UNOP: {
            Value operand = eval(interp, env, n->unop.operand);
            if (interp->had_error) { value_free(&operand); return val_null(); }
            if (!strcmp(n->unop.op, "-")) {
                if (operand.type != VAL_NUMBER) {
                    runtime_error(interp, n->line, "unary '-' requires a number");
                    return val_null();
                }
                return val_num(-operand.number);
            }
            if (!strcmp(n->unop.op, "!")) {
                int t = value_truthy(operand);
                value_free(&operand);
                return val_bool(!t);
            }
            return val_null();
        }

        case NODE_LET: {
            Value val = eval(interp, env, n->assign.value);
            if (interp->had_error) { value_free(&val); return val_null(); }
            env_define(env, n->assign.name, val); /* env takes ownership */
            return val_null();
        }

        case NODE_ASSIGN: {
            Value *existing = env_lookup(env, n->assign.name);
            if (!existing) {
                char msg[128];
                snprintf(msg, sizeof(msg), "undefined variable '%s'", n->assign.name);
                runtime_error(interp, n->line, msg);
                return val_null();
            }
            Value val = eval(interp, env, n->assign.value);
            if (interp->had_error) { value_free(&val); return val_null(); }
            env_set(env, n->assign.name, val); /* env takes ownership */
            return val_null();
        }

        case NODE_PRINT: {
            Value val = eval(interp, env, n->print_expr);
            if (!interp->had_error) value_print(val);
            value_free(&val);
            return val_null();
        }

        case NODE_IF: {
            Value cond = eval(interp, env, n->if_stmt.cond);
            int   t    = value_truthy(cond);
            value_free(&cond);
            if (t)                      return eval(interp, env, n->if_stmt.then_br);
            if (n->if_stmt.else_br)     return eval(interp, env, n->if_stmt.else_br);
            return val_null();
        }

        case NODE_WHILE: {
            while (!interp->had_error && !g_returning) {
                Value cond = eval(interp, env, n->while_stmt.cond);
                int   t    = value_truthy(cond);
                value_free(&cond);
                if (!t) break;
                Value body_val = eval(interp, env, n->while_stmt.body);
                value_free(&body_val);
            }
            return val_null();
        }

        case NODE_BLOCK: {
            Env  *block_env = env_new(env);
            Value result    = val_null();
            for (int i = 0; i < n->block.count && !interp->had_error && !g_returning; i++) {
                value_free(&result);
                result = eval(interp, block_env, n->block.stmts[i]);
            }
            env_free(block_env);
            return result;
        }

        case NODE_FN_DEF: {
            /* Register the function; interpreter takes ownership of the body. */
            if (interp->fn_count >= interp->fn_cap) {
                interp->fn_cap = interp->fn_cap ? interp->fn_cap * 2 : 8;
                interp->fns    = realloc(interp->fns, interp->fn_cap * sizeof(FnEntry));
            }
            FnEntry *fe    = &interp->fns[interp->fn_count++];
            fe->name        = strdup(n->fn_def.name);
            fe->param_count = n->fn_def.param_count;
            fe->params      = malloc(fe->param_count * sizeof(char *));
            for (int i = 0; i < fe->param_count; i++)
                fe->params[i] = strdup(n->fn_def.params[i]);
            fe->body       = n->fn_def.body;
            n->fn_def.body = NULL; /* transfer ownership */
            return val_null();
        }

        case NODE_FN_CALL: {
            FnEntry *fe = find_fn(interp, n->fn_call.name);
            if (!fe) {
                char msg[128];
                snprintf(msg, sizeof(msg), "undefined function '%s'", n->fn_call.name);
                runtime_error(interp, n->line, msg);
                return val_null();
            }
            if (n->fn_call.arg_count != fe->param_count) {
                char msg[128];
                snprintf(msg, sizeof(msg),
                         "'%s' expects %d argument(s) but got %d",
                         fe->name, fe->param_count, n->fn_call.arg_count);
                runtime_error(interp, n->line, msg);
                return val_null();
            }

            /* Build a new scope for this call, parented to globals. */
            Env *fn_env = env_new(interp->globals);
            for (int i = 0; i < fe->param_count; i++) {
                Value arg = eval(interp, env, n->fn_call.args[i]);
                if (interp->had_error) { env_free(fn_env); return val_null(); }
                env_define(fn_env, fe->params[i], arg);
            }

            Value result = eval(interp, fn_env, fe->body);
            env_free(fn_env);

            /* Capture return value if `return` was executed. */
            if (g_returning) {
                value_free(&result);
                result      = g_return_val;
                g_returning = 0;
            }
            return result;
        }

        case NODE_RETURN: {
            Value val   = n->ret_val ? eval(interp, env, n->ret_val) : val_null();
            g_returning  = 1;
            g_return_val = val;
            return val_null(); /* propagates up; captured by NODE_FN_CALL */
        }

        default:
            runtime_error(interp, n->line, "unknown AST node");
            return val_null();
    }
}

/* ── Public entry point ───────────────────────────────────────── */

void interp_run(Interpreter *interp, Node **stmts, int count) {
    for (int i = 0; i < count && !interp->had_error; i++) {
        Value v = eval(interp, interp->globals, stmts[i]);
        value_free(&v);
    }
}
