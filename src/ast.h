#ifndef AST_H
#define AST_H

typedef enum {
    NODE_NUMBER,
    NODE_STRING,
    NODE_BOOL,
    NODE_IDENT,
    NODE_BINOP,
    NODE_UNOP,
    NODE_LET,       /* let x = expr  */
    NODE_ASSIGN,    /* x = expr      */
    NODE_PRINT,
    NODE_IF,
    NODE_WHILE,
    NODE_BLOCK,
    NODE_FN_DEF,
    NODE_FN_CALL,
    NODE_RETURN,
} NodeType;

typedef struct Node Node;

struct Node {
    NodeType type;
    int      line;
    union {
        /* NODE_NUMBER */
        double number;

        /* NODE_STRING */
        char *string;

        /* NODE_BOOL */
        int boolean;

        /* NODE_IDENT */
        char *ident;

        /* NODE_BINOP */
        struct { char op[4]; Node *left; Node *right; } binop;

        /* NODE_UNOP */
        struct { char op[4]; Node *operand; } unop;

        /* NODE_LET, NODE_ASSIGN */
        struct { char *name; Node *value; } assign;

        /* NODE_PRINT */
        Node *print_expr;

        /* NODE_IF */
        struct { Node *cond; Node *then_br; Node *else_br; } if_stmt;

        /* NODE_WHILE */
        struct { Node *cond; Node *body; } while_stmt;

        /* NODE_BLOCK */
        struct { Node **stmts; int count; } block;

        /* NODE_FN_DEF */
        struct {
            char  *name;
            char **params;
            int    param_count;
            Node  *body;
        } fn_def;

        /* NODE_FN_CALL */
        struct {
            char  *name;
            Node **args;
            int    arg_count;
        } fn_call;

        /* NODE_RETURN */
        Node *ret_val;  /* may be NULL */
    };
};

Node *node_number (double val, int line);
Node *node_string (const char *val, int line);
Node *node_bool   (int val, int line);
Node *node_ident  (const char *name, int line);
Node *node_binop  (const char *op, Node *left, Node *right, int line);
Node *node_unop   (const char *op, Node *operand, int line);
Node *node_let    (const char *name, Node *value, int line);
Node *node_assign (const char *name, Node *value, int line);
Node *node_print  (Node *expr, int line);
Node *node_if     (Node *cond, Node *then_br, Node *else_br, int line);
Node *node_while  (Node *cond, Node *body, int line);
Node *node_block  (Node **stmts, int count, int line);
Node *node_fn_def (const char *name, char **params, int pc, Node *body, int line);
Node *node_fn_call(const char *name, Node **args, int ac, int line);
Node *node_return (Node *val, int line);
void  node_free   (Node *n);

#endif /* AST_H */
