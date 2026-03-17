#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "interpreter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read an entire file into a heap-allocated string. */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "Error: cannot open file '%s'\n", path); return NULL; }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

/* Run a source string through the full pipeline. */
static int run_source(Interpreter *interp, const char *src) {
    Parser  p;
    int     count;

    parser_init(&p, src);
    Node **stmts = parser_parse(&p, &count);

    if (p.had_error) {
        parser_cleanup(&p);
        free(stmts);
        return 1;
    }
    parser_cleanup(&p);

    interp_run(interp, stmts, count);

    /* Free all AST nodes (fn bodies are now owned by the interpreter). */
    for (int i = 0; i < count; i++) node_free(stmts[i]);
    free(stmts);

    return interp->had_error ? 1 : 0;
}

/* ── REPL ─────────────────────────────────────────────────────── */

/*
 * A statement may span multiple lines (e.g. a function definition).
 * We accumulate input until braces are balanced and we have seen at
 * least one non-whitespace character, then submit the buffer.
 */
static void run_repl(void) {
    Interpreter interp;
    interp_init(&interp);

    printf("Flux 1.0 — type 'exit' to quit\n");

    char   line[1024];
    char   buf[65536];
    int    buf_len  = 0;
    int    depth    = 0;  /* brace nesting depth */
    int    in_str   = 0;

    for (;;) {
        printf(depth > 0 ? "...  " : "flux> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) { printf("\n"); break; }

        /* Handle exit command */
        char trimmed[16];
        sscanf(line, "%15s", trimmed);
        if (!strcmp(trimmed, "exit")) break;

        /* Append line to buffer */
        int len = (int)strlen(line);
        if (buf_len + len >= (int)sizeof(buf) - 1) {
            fprintf(stderr, "Input too long\n");
            buf_len = 0; depth = 0; in_str = 0;
            continue;
        }
        memcpy(buf + buf_len, line, len);
        buf_len += len;
        buf[buf_len] = '\0';

        /* Update brace depth to know if the statement is complete. */
        for (int i = buf_len - len; i < buf_len; i++) {
            char c = buf[i];
            if (c == '"')  { in_str = !in_str; continue; }
            if (in_str)    continue;
            if (c == '{')  depth++;
            if (c == '}')  depth--;
        }

        /* Submit when we have a complete unit (depth == 0). */
        if (depth <= 0) {
            depth  = 0;
            in_str = 0;
            interp.had_error = 0;
            run_source(&interp, buf);
            buf_len = 0;
            buf[0]  = '\0';
        }
    }

    interp_free(&interp);
}

/* ── main ─────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    if (argc == 1) {
        run_repl();
        return 0;
    }

    if (argc == 2) {
        char *src = read_file(argv[1]);
        if (!src) return 1;

        Interpreter interp;
        interp_init(&interp);
        int status = run_source(&interp, src);
        interp_free(&interp);
        free(src);
        return status;
    }

    fprintf(stderr, "Usage: flux [script.flux]\n");
    return 1;
}
