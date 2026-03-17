#include "ast.h"
#include <stdlib.h>
#include <string.h>

static Node *alloc_node(NodeType type, int line) {
    Node *n = calloc(1, sizeof(Node));
    n->type = type;
    n->line = line;
    return n;
}

Node *node_number(double val, int line) {
    Node *n = alloc_node(NODE_NUMBER, line);
    n->number = val;
    return n;
}

Node *node_string(const char *val, int line) {
    Node *n = alloc_node(NODE_STRING, line);
    n->string = strdup(val);
    return n;
}

Node *node_bool(int val, int line) {
    Node *n = alloc_node(NODE_BOOL, line);
    n->boolean = val;
    return n;
}

Node *node_ident(const char *name, int line) {
    Node *n = alloc_node(NODE_IDENT, line);
    n->ident = strdup(name);
    return n;
}

Node *node_binop(const char *op, Node *left, Node *right, int line) {
    Node *n = alloc_node(NODE_BINOP, line);
    strncpy(n->binop.op, op, 3);
    n->binop.op[3] = '\0';
    n->binop.left  = left;
    n->binop.right = right;
    return n;
}

Node *node_unop(const char *op, Node *operand, int line) {
    Node *n = alloc_node(NODE_UNOP, line);
    strncpy(n->unop.op, op, 3);
    n->unop.op[3]   = '\0';
    n->unop.operand = operand;
    return n;
}

Node *node_let(const char *name, Node *value, int line) {
    Node *n = alloc_node(NODE_LET, line);
    n->assign.name  = strdup(name);
    n->assign.value = value;
    return n;
}

Node *node_assign(const char *name, Node *value, int line) {
    Node *n = alloc_node(NODE_ASSIGN, line);
    n->assign.name  = strdup(name);
    n->assign.value = value;
    return n;
}

Node *node_print(Node *expr, int line) {
    Node *n = alloc_node(NODE_PRINT, line);
    n->print_expr = expr;
    return n;
}

Node *node_if(Node *cond, Node *then_br, Node *else_br, int line) {
    Node *n = alloc_node(NODE_IF, line);
    n->if_stmt.cond    = cond;
    n->if_stmt.then_br = then_br;
    n->if_stmt.else_br = else_br;
    return n;
}

Node *node_while(Node *cond, Node *body, int line) {
    Node *n = alloc_node(NODE_WHILE, line);
    n->while_stmt.cond = cond;
    n->while_stmt.body = body;
    return n;
}

Node *node_block(Node **stmts, int count, int line) {
    Node *n = alloc_node(NODE_BLOCK, line);
    n->block.count = count;
    n->block.stmts = malloc(count * sizeof(Node *));
    memcpy(n->block.stmts, stmts, count * sizeof(Node *));
    return n;
}

Node *node_fn_def(const char *name, char **params, int pc, Node *body, int line) {
    Node *n = alloc_node(NODE_FN_DEF, line);
    n->fn_def.name        = strdup(name);
    n->fn_def.param_count = pc;
    n->fn_def.params      = malloc(pc * sizeof(char *));
    for (int i = 0; i < pc; i++)
        n->fn_def.params[i] = strdup(params[i]);
    n->fn_def.body = body;
    return n;
}

Node *node_fn_call(const char *name, Node **args, int ac, int line) {
    Node *n = alloc_node(NODE_FN_CALL, line);
    n->fn_call.name      = strdup(name);
    n->fn_call.arg_count = ac;
    n->fn_call.args      = malloc(ac * sizeof(Node *));
    memcpy(n->fn_call.args, args, ac * sizeof(Node *));
    return n;
}

Node *node_return(Node *val, int line) {
    Node *n = alloc_node(NODE_RETURN, line);
    n->ret_val = val;
    return n;
}

/* ── Recursive destructor ─────────────────────────────────────── */

void node_free(Node *n) {
    if (!n) return;
    switch (n->type) {
        case NODE_STRING:
            free(n->string);
            break;
        case NODE_IDENT:
            free(n->ident);
            break;
        case NODE_BINOP:
            node_free(n->binop.left);
            node_free(n->binop.right);
            break;
        case NODE_UNOP:
            node_free(n->unop.operand);
            break;
        case NODE_LET:
        case NODE_ASSIGN:
            free(n->assign.name);
            node_free(n->assign.value);
            break;
        case NODE_PRINT:
            node_free(n->print_expr);
            break;
        case NODE_IF:
            node_free(n->if_stmt.cond);
            node_free(n->if_stmt.then_br);
            node_free(n->if_stmt.else_br);
            break;
        case NODE_WHILE:
            node_free(n->while_stmt.cond);
            node_free(n->while_stmt.body);
            break;
        case NODE_BLOCK:
            for (int i = 0; i < n->block.count; i++)
                node_free(n->block.stmts[i]);
            free(n->block.stmts);
            break;
        case NODE_FN_DEF:
            free(n->fn_def.name);
            for (int i = 0; i < n->fn_def.param_count; i++)
                free(n->fn_def.params[i]);
            free(n->fn_def.params);
            /* body ownership transferred to interpreter; not freed here */
            break;
        case NODE_FN_CALL:
            free(n->fn_call.name);
            for (int i = 0; i < n->fn_call.arg_count; i++)
                node_free(n->fn_call.args[i]);
            free(n->fn_call.args);
            break;
        case NODE_RETURN:
            node_free(n->ret_val);
            break;
        default:
            break;
    }
    free(n);
}
