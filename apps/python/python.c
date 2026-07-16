/*
 * Eclipse32 "python" -- a small subset-of-Python interpreter.
 *
 * Usage:
 *   python                run an interactive REPL
 *   python <path.py>      run a script file
 *
 * Supported language subset:
 *   - types: int, float, bool, str, None, list
 *   - literals: 123, 1.5, "text"/'text', True, False, None, [1, 2, 3]
 *   - operators: + - * / // % **  == != < > <= >=  and or not  in / not in
 *   - statements: assignment (=, +=, -=, *=, /=), if/elif/else, while, for x in y,
 *                 def NAME(params): ... , return, break, continue, pass
 *   - builtins: print, len, range, str, int, float, bool, input, abs, min, max
 *
 * Not supported (kept out on purpose to fit this OS's constraints):
 *   dicts, slicing, string formatting, closures/nested defs, *args or **kwargs,
 *   exceptions, imports, classes, float ** float, float % float.
 *
 * Design notes for maintainers:
 *   - No garbage collection: allocations are never freed. Fine for short-lived
 *     scripts / REPL sessions; long-running REPL sessions will grow memory.
 *   - No setjmp/longjmp (not guaranteed available in this freestanding target).
 *     Errors propagate via a global flag (g_error) checked after every
 *     recursive call -- see rt_error() / CHKERR().
 *   - The user process stack is only 16KB (see kernel/sched/sched.h). All
 *     recursive entry points (eval_expr, exec_stmt, exec_block, call_function)
 *     share one global depth counter capped at MAX_DEPTH so a runaway/careless
 *     script gets a clean "RecursionError" instead of corrupting the OS by
 *     overrunning its stack.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "e32_syscall.h"

#define O_RDONLY 0x00

/* ============================================================================
 * Small portable helpers (SDK's string.h is missing strdup/strcpy/strchr/etc.)
 * ==========================================================================*/

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) {
        fputs("python: out of memory\n", 1);
        e32_exit(1);
    }
    return p;
}

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n);
    if (!q) {
        fputs("python: out of memory\n", 1);
        e32_exit(1);
    }
    return q;
}

static char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *p = (char *)xmalloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

static char *xstrndup(const char *s, size_t n) {
    char *p = (char *)xmalloc(n + 1);
    memcpy(p, s, n);
    p[n] = 0;
    return p;
}

/* The SDK's stdlib.h only declares atoi/strtol/strtoul (no atol/atof), so we
 * roll tiny replacements rather than depend on functions the OS doesn't ship. */
static long my_atol(const char *s) { return strtol(s, NULL, 10); }

static double my_atof(const char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    double ip = 0.0;
    while (*s >= '0' && *s <= '9') { ip = ip * 10.0 + (double)(*s - '0'); s++; }
    double frac = 0.0, scale = 1.0;
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') { frac = frac * 10.0 + (double)(*s - '0'); scale *= 10.0; s++; }
    }
    double v = ip + frac / scale;
    return neg ? -v : v;
}

static int str_find(const char *hay, const char *needle) {
    /* returns index of first match, or -1. Empty needle matches at 0. */
    size_t nlen = strlen(needle);
    if (nlen == 0) return 0;
    size_t hlen = strlen(hay);
    if (nlen > hlen) return -1;
    for (size_t i = 0; i + nlen <= hlen; i++) {
        if (memcmp(hay + i, needle, nlen) == 0) return (int)i;
    }
    return -1;
}

/* Growable byte buffer used for building output / string results. */
typedef struct {
    char *buf;
    int len, cap;
} SB;

static void sb_init(SB *sb) {
    sb->cap = 64;
    sb->len = 0;
    sb->buf = (char *)xmalloc((size_t)sb->cap);
    sb->buf[0] = 0;
}

static void sb_ensure(SB *sb, int extra) {
    if (sb->len + extra + 1 <= sb->cap) return;
    while (sb->len + extra + 1 > sb->cap) sb->cap *= 2;
    sb->buf = (char *)xrealloc(sb->buf, (size_t)sb->cap);
}

static void sb_putc(SB *sb, char c) {
    sb_ensure(sb, 1);
    sb->buf[sb->len++] = c;
    sb->buf[sb->len] = 0;
}

static void sb_puts(SB *sb, const char *s) {
    int n = (int)strlen(s);
    sb_ensure(sb, n);
    memcpy(sb->buf + sb->len, s, (size_t)n);
    sb->len += n;
    sb->buf[sb->len] = 0;
}

static void sb_putint(SB *sb, long v) {
    char tmp[32];
    int n = 0;
    unsigned long u;
    int neg = v < 0;
    u = neg ? (unsigned long)(-v) : (unsigned long)v;
    if (u == 0) tmp[n++] = '0';
    while (u > 0) {
        tmp[n++] = (char)('0' + (u % 10));
        u /= 10;
    }
    if (neg) sb_putc(sb, '-');
    while (n > 0) sb_putc(sb, tmp[--n]);
}

/* Format a double the way Python's repr roughly looks: trims trailing zeros,
 * always keeps at least one fractional digit. No exponent form (kept simple
 * on purpose -- this is a subset interpreter). */
static void sb_putfloat(SB *sb, double v) {
    if (v < 0) {
        sb_putc(sb, '-');
        v = -v;
    }
    long ip = (long)v;
    double frac = v - (double)ip;
    long fp = (long)(frac * 1000000.0 + 0.5);
    if (fp >= 1000000) {
        fp -= 1000000;
        ip += 1;
    }
    sb_putint(sb, ip);
    sb_putc(sb, '.');
    char digits[6];
    for (int i = 5; i >= 0; i--) {
        digits[i] = (char)('0' + (fp % 10));
        fp /= 10;
    }
    int last = 5;
    while (last > 0 && digits[last] == '0') last--;
    for (int i = 0; i <= last; i++) sb_putc(sb, digits[i]);
}

/* ============================================================================
 * Recursion-depth guard (protects the 16KB user stack of this OS)
 * ==========================================================================*/

#define MAX_DEPTH 48
static int g_depth = 0;

/* ============================================================================
 * Error handling (no setjmp available -- flag + early-return propagation)
 * ==========================================================================*/

static int g_error = 0;
static char g_error_msg[160];

static void rt_error(const char *fmt_literal_msg) {
    if (g_error) return; /* keep the first error */
    g_error = 1;
    size_t n = strlen(fmt_literal_msg);
    if (n >= sizeof(g_error_msg)) n = sizeof(g_error_msg) - 1;
    memcpy(g_error_msg, fmt_literal_msg, n);
    g_error_msg[n] = 0;
}

/* error with one string param appended, e.g. rt_errorv("NameError: ", name) */
static void rt_errorv(const char *prefix, const char *val) {
    if (g_error) return;
    char tmp[160];
    int n = snprintf(tmp, sizeof(tmp), "%s%s", prefix, val);
    (void)n;
    rt_error(tmp);
}

/* ============================================================================
 * Lexer
 * ==========================================================================*/

enum {
    TK_EOF, TK_NEWLINE, TK_INDENT, TK_DEDENT,
    TK_INT, TK_FLOAT, TK_STRING, TK_NAME,
    TK_TRUE, TK_FALSE, TK_NONE,
    TK_IF, TK_ELIF, TK_ELSE, TK_WHILE, TK_FOR, TK_IN, TK_NOTIN, TK_DEF, TK_RETURN,
    TK_BREAK, TK_CONTINUE, TK_PASS, TK_AND, TK_OR, TK_NOT,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_DSLASH, TK_PERCENT, TK_DSTAR,
    TK_ASSIGN, TK_EQ, TK_NE, TK_LT, TK_GT, TK_LE, TK_GE,
    TK_PLUSEQ, TK_MINUSEQ, TK_STAREQ, TK_SLASHEQ,
    TK_LPAREN, TK_RPAREN, TK_LBRACKET, TK_RBRACKET, TK_COMMA, TK_COLON
};

typedef struct {
    int kind;
    long ival;
    double fval;
    char *sval;
    int line;
} Token;

typedef struct {
    Token *toks;
    int n, cap;
} TokList;

static void tl_push(TokList *tl, Token t) {
    if (tl->n >= tl->cap) {
        tl->cap = tl->cap ? tl->cap * 2 : 64;
        tl->toks = (Token *)xrealloc(tl->toks, sizeof(Token) * (size_t)tl->cap);
    }
    tl->toks[tl->n++] = t;
}

typedef struct {
    const char *word;
    int kind;
} KW;

static const KW KEYWORDS[] = {
    {"True", TK_TRUE}, {"False", TK_FALSE}, {"None", TK_NONE},
    {"if", TK_IF}, {"elif", TK_ELIF}, {"else", TK_ELSE},
    {"while", TK_WHILE}, {"for", TK_FOR}, {"in", TK_IN},
    {"def", TK_DEF}, {"return", TK_RETURN},
    {"break", TK_BREAK}, {"continue", TK_CONTINUE}, {"pass", TK_PASS},
    {"and", TK_AND}, {"or", TK_OR}, {"not", TK_NOT},
};
#define NKW (int)(sizeof(KEYWORDS) / sizeof(KEYWORDS[0]))

static int is_id_start(char c) { return (c == '_') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static int is_id_char(char c) { return is_id_start(c) || (c >= '0' && c <= '9'); }
static int is_digit(char c) { return c >= '0' && c <= '9'; }

/* Tokenizes the entire source up front (indentation-aware). Returns 1 on
 * success, 0 on lexical error (message left in g_error_msg). */
static int lex(const char *src, TokList *out) {
    int indent_stack[64];
    int indent_sp = 0;
    indent_stack[0] = 0;
    int i = 0;
    int line = 1;
    int paren_depth = 0;
    int at_line_start = 1;
    int last_was_newline = 1; /* suppress blank leading NEWLINEs */

    while (src[i]) {
        if (at_line_start && paren_depth == 0) {
            /* measure indentation */
            int col = 0;
            int j = i;
            while (src[j] == ' ' || src[j] == '\t') {
                col += (src[j] == '\t') ? 8 : 1;
                j++;
            }
            if (src[j] == '\n' || src[j] == '\r' || src[j] == 0 || src[j] == '#') {
                /* blank/comment-only line: skip without touching indent stack */
                i = j;
                while (src[i] && src[i] != '\n') i++;
                if (src[i] == '\n') { i++; line++; }
                continue;
            }
            if (col > indent_stack[indent_sp]) {
                if (indent_sp + 1 >= (int)(sizeof(indent_stack) / sizeof(indent_stack[0]))) {
                    rt_error("IndentationError: too deeply nested");
                    return 0;
                }
                indent_stack[++indent_sp] = col;
                Token t = {TK_INDENT, 0, 0, NULL, line};
                tl_push(out, t);
            } else {
                while (col < indent_stack[indent_sp]) {
                    indent_sp--;
                    Token t = {TK_DEDENT, 0, 0, NULL, line};
                    tl_push(out, t);
                }
                if (col != indent_stack[indent_sp]) {
                    rt_error("IndentationError: unindent does not match any outer indentation level");
                    return 0;
                }
            }
            i = j;
            at_line_start = 0;
            continue;
        }

        char c = src[i];

        if (c == '#') {
            while (src[i] && src[i] != '\n') i++;
            continue;
        }
        if (c == '\\' && src[i + 1] == '\n') { i += 2; line++; continue; }
        if (c == ' ' || c == '\t' || c == '\r') { i++; continue; }
        if (c == '\n') {
            i++;
            if (paren_depth == 0) {
                if (!last_was_newline && out->n > 0 &&
                    out->toks[out->n - 1].kind != TK_INDENT && out->toks[out->n - 1].kind != TK_DEDENT) {
                    Token t = {TK_NEWLINE, 0, 0, NULL, line};
                    tl_push(out, t);
                    last_was_newline = 1;
                }
                at_line_start = 1;
            }
            line++;
            continue;
        }

        last_was_newline = 0;

        if (is_digit(c) || (c == '.' && is_digit(src[i + 1]))) {
            int start = i;
            int is_float = 0;
            while (is_digit(src[i])) i++;
            if (src[i] == '.' && is_digit(src[i + 1])) {
                is_float = 1;
                i++;
                while (is_digit(src[i])) i++;
            }
            char *numstr = xstrndup(src + start, (size_t)(i - start));
            Token t;
            t.line = line;
            t.sval = NULL;
            if (is_float) {
                t.kind = TK_FLOAT;
                t.fval = my_atof(numstr);
                t.ival = 0;
            } else {
                t.kind = TK_INT;
                t.ival = my_atol(numstr);
                t.fval = 0;
            }
            free(numstr);
            tl_push(out, t);
            continue;
        }

        if (is_id_start(c)) {
            int start = i;
            while (is_id_char(src[i])) i++;
            char *word = xstrndup(src + start, (size_t)(i - start));
            int kind = TK_NAME;
            for (int k = 0; k < NKW; k++) {
                if (strcmp(word, KEYWORDS[k].word) == 0) { kind = KEYWORDS[k].kind; break; }
            }
            Token t;
            t.line = line;
            t.ival = 0;
            t.fval = 0;
            if (kind == TK_NAME) {
                t.sval = word;
            } else {
                free(word);
                t.sval = NULL;
            }
            t.kind = kind;
            tl_push(out, t);
            continue;
        }

        if (c == '"' || c == '\'') {
            char quote = c;
            i++;
            SB sb;
            sb_init(&sb);
            while (src[i] && src[i] != quote) {
                if (src[i] == '\n') { rt_error("SyntaxError: unterminated string literal"); return 0; }
                if (src[i] == '\\' && src[i + 1]) {
                    i++;
                    char e = src[i];
                    char out_c;
                    switch (e) {
                        case 'n': out_c = '\n'; break;
                        case 't': out_c = '\t'; break;
                        case 'r': out_c = '\r'; break;
                        case '\\': out_c = '\\'; break;
                        case '\'': out_c = '\''; break;
                        case '"': out_c = '"'; break;
                        case '0': out_c = '\0'; break;
                        default: out_c = e; break;
                    }
                    sb_putc(&sb, out_c);
                    i++;
                } else {
                    sb_putc(&sb, src[i]);
                    i++;
                }
            }
            if (src[i] != quote) { rt_error("SyntaxError: unterminated string literal"); return 0; }
            i++;
            Token t = {TK_STRING, 0, 0, sb.buf, line};
            tl_push(out, t);
            continue;
        }

        /* operators / punctuation */
        int kind = -1;
        int adv = 1;
        switch (c) {
            case '+': kind = (src[i + 1] == '=') ? (adv = 2, TK_PLUSEQ) : TK_PLUS; break;
            case '-': kind = (src[i + 1] == '=') ? (adv = 2, TK_MINUSEQ) : TK_MINUS; break;
            case '*':
                if (src[i + 1] == '*') { kind = TK_DSTAR; adv = 2; }
                else if (src[i + 1] == '=') { kind = TK_STAREQ; adv = 2; }
                else kind = TK_STAR;
                break;
            case '/':
                if (src[i + 1] == '/') { kind = TK_DSLASH; adv = 2; }
                else if (src[i + 1] == '=') { kind = TK_SLASHEQ; adv = 2; }
                else kind = TK_SLASH;
                break;
            case '%': kind = TK_PERCENT; break;
            case '=': kind = (src[i + 1] == '=') ? (adv = 2, TK_EQ) : TK_ASSIGN; break;
            case '!':
                if (src[i + 1] == '=') { kind = TK_NE; adv = 2; }
                break;
            case '<': kind = (src[i + 1] == '=') ? (adv = 2, TK_LE) : TK_LT; break;
            case '>': kind = (src[i + 1] == '=') ? (adv = 2, TK_GE) : TK_GT; break;
            case '(': kind = TK_LPAREN; paren_depth++; break;
            case ')': kind = TK_RPAREN; if (paren_depth > 0) paren_depth--; break;
            case '[': kind = TK_LBRACKET; paren_depth++; break;
            case ']': kind = TK_RBRACKET; if (paren_depth > 0) paren_depth--; break;
            case ',': kind = TK_COMMA; break;
            case ':': kind = TK_COLON; break;
        }
        if (kind < 0) {
            rt_error("SyntaxError: unexpected character");
            return 0;
        }
        Token t = {kind, 0, 0, NULL, line};
        tl_push(out, t);
        i += adv;
    }

    if (paren_depth == 0 && out->n > 0 && out->toks[out->n - 1].kind != TK_NEWLINE) {
        Token t = {TK_NEWLINE, 0, 0, NULL, line};
        tl_push(out, t);
    }
    while (indent_sp > 0) {
        indent_sp--;
        Token t = {TK_DEDENT, 0, 0, NULL, line};
        tl_push(out, t);
    }
    Token eof = {TK_EOF, 0, 0, NULL, line};
    tl_push(out, eof);
    return 1;
}

/* ============================================================================
 * AST
 * ==========================================================================*/

enum {
    NK_INT, NK_FLOAT, NK_STR, NK_BOOL, NK_NONE, NK_NAME, NK_LIST,
    NK_BINOP, NK_UNARYNEG, NK_NOT, NK_AND, NK_OR, NK_COMPARE, NK_IN,
    NK_CALL, NK_INDEX,
    NK_ASSIGN, NK_AUGASSIGN,
    NK_EXPRSTMT, NK_IF, NK_WHILE, NK_FOR, NK_FUNCDEF, NK_RETURN,
    NK_BREAK, NK_CONTINUE, NK_PASS, NK_BLOCK
};

typedef struct Node {
    int kind;
    int op;
    long ival;
    double fval;
    char *sval;
    struct Node *a, *b, *c;
    struct Node **items;
    int nitems, icap;
    int *iops;
    int niops, iopcap;
} Node;

static Node *node_new(int kind) {
    Node *n = (Node *)xmalloc(sizeof(Node));
    memset(n, 0, sizeof(Node));
    n->kind = kind;
    return n;
}

static void node_add_item(Node *n, Node *child) {
    if (n->nitems >= n->icap) {
        n->icap = n->icap ? n->icap * 2 : 4;
        n->items = (Node **)xrealloc(n->items, sizeof(Node *) * (size_t)n->icap);
    }
    n->items[n->nitems++] = child;
}

static void node_add_iop(Node *n, int op) {
    if (n->niops >= n->iopcap) {
        n->iopcap = n->iopcap ? n->iopcap * 2 : 4;
        n->iops = (int *)xrealloc(n->iops, sizeof(int) * (size_t)n->iopcap);
    }
    n->iops[n->niops++] = op;
}

/* ============================================================================
 * Parser (recursive descent)
 * ==========================================================================*/

typedef struct {
    Token *toks;
    int pos;
} Parser;

static Token *cur(Parser *p) { return &p->toks[p->pos]; }
static int at(Parser *p, int kind) { return cur(p)->kind == kind; }

static void advance(Parser *p) { if (cur(p)->kind != TK_EOF) p->pos++; }

static int parse_depth_check(void) {
    if (g_error) return 0;
    g_depth++;
    if (g_depth > MAX_DEPTH) {
        rt_error("SyntaxError: expression/statement nested too deeply");
        g_depth--;
        return 0;
    }
    return 1;
}
static void parse_depth_done(void) { g_depth--; }

static int expect(Parser *p, int kind, const char *what) {
    if (!at(p, kind)) {
        rt_errorv("SyntaxError: expected ", what);
        return 0;
    }
    advance(p);
    return 1;
}

static Node *parse_expr(Parser *p);
static Node *parse_block(Parser *p);

static Node *parse_atom(Parser *p) {
    if (!parse_depth_check()) return node_new(NK_NONE);
    Token *t = cur(p);
    Node *n = NULL;
    if (t->kind == TK_INT) { n = node_new(NK_INT); n->ival = t->ival; advance(p); }
    else if (t->kind == TK_FLOAT) { n = node_new(NK_FLOAT); n->fval = t->fval; advance(p); }
    else if (t->kind == TK_STRING) { n = node_new(NK_STR); n->sval = t->sval; advance(p); }
    else if (t->kind == TK_TRUE) { n = node_new(NK_BOOL); n->ival = 1; advance(p); }
    else if (t->kind == TK_FALSE) { n = node_new(NK_BOOL); n->ival = 0; advance(p); }
    else if (t->kind == TK_NONE) { n = node_new(NK_NONE); advance(p); }
    else if (t->kind == TK_NAME) {
        char *name = t->sval;
        advance(p);
        if (at(p, TK_LPAREN)) {
            advance(p);
            n = node_new(NK_CALL);
            n->sval = name;
            if (!at(p, TK_RPAREN)) {
                node_add_item(n, parse_expr(p));
                while (at(p, TK_COMMA)) { advance(p); node_add_item(n, parse_expr(p)); }
            }
            if (!expect(p, TK_RPAREN, "')'")) { parse_depth_done(); return n; }
        } else {
            n = node_new(NK_NAME);
            n->sval = name;
        }
    } else if (t->kind == TK_LPAREN) {
        advance(p);
        n = parse_expr(p);
        expect(p, TK_RPAREN, "')'");
    } else if (t->kind == TK_LBRACKET) {
        advance(p);
        n = node_new(NK_LIST);
        if (!at(p, TK_RBRACKET)) {
            node_add_item(n, parse_expr(p));
            while (at(p, TK_COMMA)) { advance(p); if (at(p, TK_RBRACKET)) break; node_add_item(n, parse_expr(p)); }
        }
        expect(p, TK_RBRACKET, "']'");
    } else if (t->kind == TK_MINUS) {
        advance(p);
        n = node_new(NK_UNARYNEG);
        n->a = parse_atom(p);
    } else {
        rt_error("SyntaxError: expected an expression");
        n = node_new(NK_NONE);
    }

    /* postfix: indexing and (rare) chained calls are not supported beyond one
     * level of foo(...)[i] etc. -- index chains are supported. */
    while (at(p, TK_LBRACKET)) {
        advance(p);
        if (at(p, TK_COLON)) {
            rt_error("SyntaxError: slicing ([a:b]) is not supported in this Python subset");
            return n;
        }
        Node *idx = parse_expr(p);
        if (at(p, TK_COLON)) {
            rt_error("SyntaxError: slicing ([a:b]) is not supported in this Python subset");
            return n;
        }
        expect(p, TK_RBRACKET, "']'");
        Node *ix = node_new(NK_INDEX);
        ix->a = n;
        ix->b = idx;
        n = ix;
    }

    parse_depth_done();
    return n;
}

static Node *parse_power(Parser *p) {
    Node *left = parse_atom(p);
    if (at(p, TK_DSTAR)) {
        advance(p);
        Node *right = parse_power(p); /* right-assoc */
        Node *n = node_new(NK_BINOP);
        n->op = TK_DSTAR;
        n->a = left; n->b = right;
        return n;
    }
    return left;
}

static Node *parse_term(Parser *p) {
    Node *n = parse_power(p);
    while (at(p, TK_STAR) || at(p, TK_SLASH) || at(p, TK_DSLASH) || at(p, TK_PERCENT)) {
        int op = cur(p)->kind;
        advance(p);
        Node *r = parse_power(p);
        Node *b = node_new(NK_BINOP);
        b->op = op; b->a = n; b->b = r;
        n = b;
    }
    return n;
}

static Node *parse_arith(Parser *p) {
    Node *n = parse_term(p);
    while (at(p, TK_PLUS) || at(p, TK_MINUS)) {
        int op = cur(p)->kind;
        advance(p);
        Node *r = parse_term(p);
        Node *b = node_new(NK_BINOP);
        b->op = op; b->a = n; b->b = r;
        n = b;
    }
    return n;
}

static int is_compare_op(int k) {
    return k == TK_EQ || k == TK_NE || k == TK_LT || k == TK_GT || k == TK_LE || k == TK_GE;
}

static Node *parse_comparison(Parser *p) {
    Node *first = parse_arith(p);
    if (!is_compare_op(cur(p)->kind) && !at(p, TK_IN) && !(at(p, TK_NOT) && p->toks[p->pos + 1].kind == TK_IN)) {
        return first;
    }
    /* membership test */
    if (at(p, TK_IN) || (at(p, TK_NOT) && p->toks[p->pos + 1].kind == TK_IN)) {
        int negate = 0;
        if (at(p, TK_NOT)) { negate = 1; advance(p); }
        advance(p); /* IN */
        Node *rhs = parse_arith(p);
        Node *n = node_new(NK_IN);
        n->op = negate;
        n->a = first; n->b = rhs;
        return n;
    }
    Node *n = node_new(NK_COMPARE);
    node_add_item(n, first);
    while (is_compare_op(cur(p)->kind)) {
        int op = cur(p)->kind;
        advance(p);
        Node *r = parse_arith(p);
        node_add_iop(n, op);
        node_add_item(n, r);
    }
    return n;
}

static Node *parse_not(Parser *p) {
    if (at(p, TK_NOT)) {
        advance(p);
        Node *n = node_new(NK_NOT);
        n->a = parse_not(p);
        return n;
    }
    return parse_comparison(p);
}

static Node *parse_and(Parser *p) {
    Node *n = parse_not(p);
    while (at(p, TK_AND)) {
        advance(p);
        Node *r = parse_not(p);
        Node *b = node_new(NK_AND);
        b->a = n; b->b = r;
        n = b;
    }
    return n;
}

static Node *parse_or(Parser *p) {
    Node *n = parse_and(p);
    while (at(p, TK_OR)) {
        advance(p);
        Node *r = parse_and(p);
        Node *b = node_new(NK_OR);
        b->a = n; b->b = r;
        n = b;
    }
    return n;
}

static Node *parse_expr(Parser *p) {
    if (!parse_depth_check()) return node_new(NK_NONE);
    Node *n = parse_or(p);
    parse_depth_done();
    return n;
}

static void skip_newlines(Parser *p) {
    while (at(p, TK_NEWLINE)) advance(p);
}

static Node *parse_simple_stmt(Parser *p) {
    if (at(p, TK_BREAK)) { advance(p); return node_new(NK_BREAK); }
    if (at(p, TK_CONTINUE)) { advance(p); return node_new(NK_CONTINUE); }
    if (at(p, TK_PASS)) { advance(p); return node_new(NK_PASS); }
    if (at(p, TK_RETURN)) {
        advance(p);
        Node *n = node_new(NK_RETURN);
        if (!at(p, TK_NEWLINE) && !at(p, TK_EOF)) n->a = parse_expr(p);
        return n;
    }
    Node *lhs = parse_expr(p);
    if (at(p, TK_ASSIGN)) {
        advance(p);
        if (lhs->kind != NK_NAME && lhs->kind != NK_INDEX) {
            rt_error("SyntaxError: invalid assignment target");
        }
        Node *n = node_new(NK_ASSIGN);
        n->a = lhs;
        n->b = parse_expr(p);
        return n;
    }
    if (at(p, TK_PLUSEQ) || at(p, TK_MINUSEQ) || at(p, TK_STAREQ) || at(p, TK_SLASHEQ)) {
        int op = cur(p)->kind;
        advance(p);
        if (lhs->kind != NK_NAME && lhs->kind != NK_INDEX) {
            rt_error("SyntaxError: invalid assignment target");
        }
        Node *n = node_new(NK_AUGASSIGN);
        n->op = op;
        n->a = lhs;
        n->b = parse_expr(p);
        return n;
    }
    Node *n = node_new(NK_EXPRSTMT);
    n->a = lhs;
    return n;
}

static Node *parse_suite(Parser *p) {
    /* either a single simple statement on the same line, or a NEWLINE INDENT
     * block* DEDENT */
    if (at(p, TK_COLON)) advance(p);
    else { rt_error("SyntaxError: expected ':'"); return node_new(NK_BLOCK); }

    if (!at(p, TK_NEWLINE)) {
        /* one-liner suite: possibly multiple simple statements separated by ; is
         * not supported -- keep it to a single statement, matches common usage */
        Node *blk = node_new(NK_BLOCK);
        node_add_item(blk, parse_simple_stmt(p));
        return blk;
    }
    advance(p); /* NEWLINE */
    skip_newlines(p);
    if (!at(p, TK_INDENT)) { rt_error("IndentationError: expected an indented block"); return node_new(NK_BLOCK); }
    return parse_block(p);
}

static Node *parse_compound_stmt(Parser *p) {
    if (at(p, TK_IF)) {
        advance(p);
        Node *n = node_new(NK_IF);
        n->a = parse_expr(p);
        n->b = parse_suite(p);
        skip_newlines(p);
        if (at(p, TK_ELIF)) {
            n->c = parse_compound_stmt(p); /* nested if, reuses IF/ELIF chain */
            return n;
        }
        if (at(p, TK_ELSE)) {
            advance(p);
            n->c = parse_suite(p);
        }
        return n;
    }
    if (at(p, TK_ELIF)) {
        /* only reached via recursive call above; parse like IF */
        advance(p);
        Node *n = node_new(NK_IF);
        n->a = parse_expr(p);
        n->b = parse_suite(p);
        skip_newlines(p);
        if (at(p, TK_ELIF)) {
            n->c = parse_compound_stmt(p);
            return n;
        }
        if (at(p, TK_ELSE)) {
            advance(p);
            n->c = parse_suite(p);
        }
        return n;
    }
    if (at(p, TK_WHILE)) {
        advance(p);
        Node *n = node_new(NK_WHILE);
        n->a = parse_expr(p);
        n->b = parse_suite(p);
        return n;
    }
    if (at(p, TK_FOR)) {
        advance(p);
        if (!at(p, TK_NAME)) { rt_error("SyntaxError: expected loop variable name"); return node_new(NK_PASS); }
        Node *n = node_new(NK_FOR);
        n->sval = cur(p)->sval;
        advance(p);
        if (!expect(p, TK_IN, "'in'")) return n;
        n->a = parse_expr(p);
        n->b = parse_suite(p);
        return n;
    }
    if (at(p, TK_DEF)) {
        advance(p);
        if (!at(p, TK_NAME)) { rt_error("SyntaxError: expected function name"); return node_new(NK_PASS); }
        Node *n = node_new(NK_FUNCDEF);
        n->sval = cur(p)->sval;
        advance(p);
        if (!expect(p, TK_LPAREN, "'('")) return n;
        if (!at(p, TK_RPAREN)) {
            do {
                if (!at(p, TK_NAME)) { rt_error("SyntaxError: expected parameter name"); return n; }
                Node *pn = node_new(NK_NAME);
                pn->sval = cur(p)->sval;
                advance(p);
                node_add_item(n, pn);
            } while (at(p, TK_COMMA) && (advance(p), 1));
        }
        if (!expect(p, TK_RPAREN, "')'")) return n;
        n->b = parse_suite(p);
        return n;
    }
    Node *s = parse_simple_stmt(p);
    if (!expect(p, TK_NEWLINE, "newline")) { /* be forgiving at EOF */ }
    return s;
}

static Node *parse_block(Parser *p) {
    if (!parse_depth_check()) return node_new(NK_BLOCK);
    Node *blk = node_new(NK_BLOCK);
    if (at(p, TK_INDENT)) {
        advance(p);
        skip_newlines(p);
        while (!at(p, TK_DEDENT) && !at(p, TK_EOF) && !g_error) {
            node_add_item(blk, parse_compound_stmt(p));
            skip_newlines(p);
        }
        if (at(p, TK_DEDENT)) advance(p);
    } else {
        while (!at(p, TK_EOF) && !g_error) {
            skip_newlines(p);
            if (at(p, TK_EOF) || at(p, TK_DEDENT)) break;
            node_add_item(blk, parse_compound_stmt(p));
        }
    }
    parse_depth_done();
    return blk;
}

static Node *parse_program(Token *toks) {
    Parser p = {toks, 0};
    skip_newlines(&p);
    Node *blk = node_new(NK_BLOCK);
    while (!at(&p, TK_EOF) && !g_error) {
        node_add_item(blk, parse_compound_stmt(&p));
        skip_newlines(&p);
    }
    return blk;
}

/* ============================================================================
 * Values
 * ==========================================================================*/

typedef enum { V_NONE, V_INT, V_FLOAT, V_BOOL, V_STR, V_LIST } VKind;
typedef struct Value Value;
typedef struct ListObj {
    Value *items;
    int nitems, cap;
} ListObj;
struct Value {
    VKind kind;
    long i;
    double f;
    char *s;
    ListObj *list;
};

static Value V_none(void) { Value v; memset(&v, 0, sizeof(v)); v.kind = V_NONE; return v; }
static Value V_int(long i) { Value v = V_none(); v.kind = V_INT; v.i = i; return v; }
static Value V_float(double f) { Value v = V_none(); v.kind = V_FLOAT; v.f = f; return v; }
static Value V_bool(int b) { Value v = V_none(); v.kind = V_BOOL; v.i = b ? 1 : 0; return v; }
static Value V_str(const char *s) { Value v = V_none(); v.kind = V_STR; v.s = xstrdup(s); return v; }
static Value V_str_own(char *s) { Value v = V_none(); v.kind = V_STR; v.s = s; return v; }
static Value V_list_new(void) {
    Value v = V_none();
    v.kind = V_LIST;
    v.list = (ListObj *)xmalloc(sizeof(ListObj));
    v.list->items = NULL;
    v.list->nitems = 0;
    v.list->cap = 0;
    return v;
}
static void list_append(ListObj *l, Value item) {
    if (l->nitems >= l->cap) {
        l->cap = l->cap ? l->cap * 2 : 4;
        l->items = (Value *)xrealloc(l->items, sizeof(Value) * (size_t)l->cap);
    }
    l->items[l->nitems++] = item;
}

static double as_float(Value v) {
    if (v.kind == V_INT) return (double)v.i;
    if (v.kind == V_BOOL) return (double)v.i;
    if (v.kind == V_FLOAT) return v.f;
    return 0.0;
}
static long as_int(Value v) {
    if (v.kind == V_FLOAT) return (long)v.f;
    return v.i;
}
static int is_numeric(Value v) { return v.kind == V_INT || v.kind == V_FLOAT || v.kind == V_BOOL; }

static int truthy(Value v) {
    switch (v.kind) {
        case V_NONE: return 0;
        case V_INT: return v.i != 0;
        case V_BOOL: return v.i != 0;
        case V_FLOAT: return v.f != 0.0;
        case V_STR: return v.s[0] != 0;
        case V_LIST: return v.list->nitems != 0;
    }
    return 0;
}

static void value_repr_sb(SB *sb, Value v, int quote_strings);

static void list_repr_sb(SB *sb, ListObj *l) {
    sb_putc(sb, '[');
    for (int i = 0; i < l->nitems; i++) {
        if (i) sb_puts(sb, ", ");
        value_repr_sb(sb, l->items[i], 1);
    }
    sb_putc(sb, ']');
}

static void value_repr_sb(SB *sb, Value v, int quote_strings) {
    switch (v.kind) {
        case V_NONE: sb_puts(sb, "None"); break;
        case V_BOOL: sb_puts(sb, v.i ? "True" : "False"); break;
        case V_INT: sb_putint(sb, v.i); break;
        case V_FLOAT: sb_putfloat(sb, v.f); break;
        case V_STR:
            if (quote_strings) { sb_putc(sb, '\''); sb_puts(sb, v.s); sb_putc(sb, '\''); }
            else sb_puts(sb, v.s);
            break;
        case V_LIST: list_repr_sb(sb, v.list); break;
    }
}

static char *value_to_cstr(Value v, int quote_strings) {
    SB sb;
    sb_init(&sb);
    value_repr_sb(&sb, v, quote_strings);
    return sb.buf; /* caller does not need to free for our usage pattern */
}

static const char *type_name(Value v) {
    switch (v.kind) {
        case V_NONE: return "NoneType";
        case V_INT: return "int";
        case V_FLOAT: return "float";
        case V_BOOL: return "bool";
        case V_STR: return "str";
        case V_LIST: return "list";
    }
    return "?";
}

static int values_equal(Value a, Value b) {
    if (is_numeric(a) && is_numeric(b)) {
        if (a.kind == V_FLOAT || b.kind == V_FLOAT) return as_float(a) == as_float(b);
        return as_int(a) == as_int(b);
    }
    if (a.kind != b.kind) return 0;
    switch (a.kind) {
        case V_NONE: return 1;
        case V_STR: return strcmp(a.s, b.s) == 0;
        case V_LIST:
            if (a.list->nitems != b.list->nitems) return 0;
            for (int i = 0; i < a.list->nitems; i++)
                if (!values_equal(a.list->items[i], b.list->items[i])) return 0;
            return 1;
        default: return 0;
    }
}

/* returns -1/0/1, sets g_error for uncomparable types */
static int values_cmp(Value a, Value b) {
    if (is_numeric(a) && is_numeric(b)) {
        double x = as_float(a), y = as_float(b);
        return (x < y) ? -1 : (x > y) ? 1 : 0;
    }
    if (a.kind == V_STR && b.kind == V_STR) {
        int c = strcmp(a.s, b.s);
        return (c < 0) ? -1 : (c > 0) ? 1 : 0;
    }
    rt_errorv("TypeError: cannot compare types ", type_name(a));
    return 0;
}

/* ============================================================================
 * Environment
 * ==========================================================================*/

typedef struct {
    char *name;
    Value val;
} EnvEntry;

typedef struct {
    EnvEntry *entries;
    int n, cap;
} Env;

static void env_init(Env *e) { e->entries = NULL; e->n = 0; e->cap = 0; }

static int env_find(Env *e, const char *name) {
    for (int i = 0; i < e->n; i++)
        if (strcmp(e->entries[i].name, name) == 0) return i;
    return -1;
}

static void env_set(Env *e, const char *name, Value v) {
    int idx = env_find(e, name);
    if (idx >= 0) { e->entries[idx].val = v; return; }
    if (e->n >= e->cap) {
        e->cap = e->cap ? e->cap * 2 : 8;
        e->entries = (EnvEntry *)xrealloc(e->entries, sizeof(EnvEntry) * (size_t)e->cap);
    }
    e->entries[e->n].name = xstrdup(name);
    e->entries[e->n].val = v;
    e->n++;
}

static int env_get(Env *e, const char *name, Value *out) {
    int idx = env_find(e, name);
    if (idx < 0) return 0;
    *out = e->entries[idx].val;
    return 1;
}

static Env g_global;

typedef struct {
    char *name;
    Node *def;
} FuncEntry;
static FuncEntry *g_funcs = NULL;
static int g_nfuncs = 0, g_funcs_cap = 0;

static void func_define(const char *name, Node *def) {
    for (int i = 0; i < g_nfuncs; i++) {
        if (strcmp(g_funcs[i].name, name) == 0) { g_funcs[i].def = def; return; }
    }
    if (g_nfuncs >= g_funcs_cap) {
        g_funcs_cap = g_funcs_cap ? g_funcs_cap * 2 : 8;
        g_funcs = (FuncEntry *)xrealloc(g_funcs, sizeof(FuncEntry) * (size_t)g_funcs_cap);
    }
    g_funcs[g_nfuncs].name = xstrdup(name);
    g_funcs[g_nfuncs].def = def;
    g_nfuncs++;
}

static Node *func_lookup(const char *name) {
    for (int i = 0; i < g_nfuncs; i++)
        if (strcmp(g_funcs[i].name, name) == 0) return g_funcs[i].def;
    return NULL;
}

/* ============================================================================
 * Interpreter
 * ==========================================================================*/

typedef enum { SIG_NONE, SIG_BREAK, SIG_CONTINUE, SIG_RETURN } Signal;
static Value g_return_value;

static Value eval_expr(Node *n, Env *env);
static Signal exec_block(Node *blk, Env *env);
static Signal exec_stmt(Node *n, Env *env);

static int normalize_index(int idx, int len) {
    if (idx < 0) idx += len;
    return idx;
}

static Value call_function(const char *name, Node *def, Value *args, int nargs) {
    int nparams = def->nitems;
    if (nargs != nparams) {
        char msg[160];
        snprintf(msg, sizeof(msg), "TypeError: %s() takes %d argument(s) but %d given", name, nparams, nargs);
        rt_error(msg);
        return V_none();
    }
    g_depth++;
    if (g_depth > MAX_DEPTH) {
        rt_error("RecursionError: maximum recursion depth exceeded");
        g_depth--;
        return V_none();
    }
    Env local;
    env_init(&local);
    for (int i = 0; i < nparams; i++) env_set(&local, def->items[i]->sval, args[i]);
    Signal sig = exec_block(def->b, &local);
    Value ret = V_none();
    if (sig == SIG_RETURN) ret = g_return_value;
    g_depth--;
    return ret;
}

/* ---- builtins ---- */

static Value builtin_print(Value *args, int nargs) {
    SB sb;
    sb_init(&sb);
    for (int i = 0; i < nargs; i++) {
        if (i) sb_putc(&sb, ' ');
        value_repr_sb(&sb, args[i], 0);
    }
    sb_putc(&sb, '\n');
    fputs(sb.buf, 1);
    free(sb.buf);
    return V_none();
}

static Value builtin_len(Value *args, int nargs) {
    if (nargs != 1) { rt_error("TypeError: len() takes exactly 1 argument"); return V_none(); }
    if (args[0].kind == V_STR) return V_int((long)strlen(args[0].s));
    if (args[0].kind == V_LIST) return V_int(args[0].list->nitems);
    rt_errorv("TypeError: object of this type has no len(): ", type_name(args[0]));
    return V_none();
}

static Value builtin_range(Value *args, int nargs) {
    long start = 0, stop = 0, step = 1;
    if (nargs == 1) { stop = as_int(args[0]); }
    else if (nargs == 2) { start = as_int(args[0]); stop = as_int(args[1]); }
    else if (nargs == 3) { start = as_int(args[0]); stop = as_int(args[1]); step = as_int(args[2]); }
    else { rt_error("TypeError: range() takes 1 to 3 arguments"); return V_none(); }
    if (step == 0) { rt_error("ValueError: range() step argument must not be zero"); return V_none(); }
    Value lst = V_list_new();
    if (step > 0) for (long v = start; v < stop; v += step) list_append(lst.list, V_int(v));
    else for (long v = start; v > stop; v += step) list_append(lst.list, V_int(v));
    return lst;
}

static Value builtin_str(Value *args, int nargs) {
    if (nargs != 1) { rt_error("TypeError: str() takes exactly 1 argument"); return V_none(); }
    return V_str_own(value_to_cstr(args[0], 0));
}

static Value builtin_int(Value *args, int nargs) {
    if (nargs != 1) { rt_error("TypeError: int() takes exactly 1 argument"); return V_none(); }
    Value v = args[0];
    if (v.kind == V_STR) return V_int(my_atol(v.s));
    if (is_numeric(v)) return V_int(as_int(v));
    rt_error("TypeError: int() argument must be a string or a number");
    return V_none();
}

static Value builtin_float(Value *args, int nargs) {
    if (nargs != 1) { rt_error("TypeError: float() takes exactly 1 argument"); return V_none(); }
    Value v = args[0];
    if (v.kind == V_STR) return V_float(my_atof(v.s));
    if (is_numeric(v)) return V_float(as_float(v));
    rt_error("TypeError: float() argument must be a string or a number");
    return V_none();
}

static Value builtin_bool(Value *args, int nargs) {
    if (nargs != 1) { rt_error("TypeError: bool() takes exactly 1 argument"); return V_none(); }
    return V_bool(truthy(args[0]));
}

static Value builtin_abs(Value *args, int nargs) {
    if (nargs != 1 || !is_numeric(args[0])) { rt_error("TypeError: abs() takes exactly 1 numeric argument"); return V_none(); }
    if (args[0].kind == V_FLOAT) { double f = args[0].f; return V_float(f < 0 ? -f : f); }
    long i = as_int(args[0]);
    return V_int(i < 0 ? -i : i);
}

static Value builtin_minmax(Value *args, int nargs, int want_min) {
    if (nargs == 1 && args[0].kind == V_LIST) {
        ListObj *l = args[0].list;
        if (l->nitems == 0) { rt_error("ValueError: min()/max() arg is an empty sequence"); return V_none(); }
        Value best = l->items[0];
        for (int i = 1; i < l->nitems; i++) {
            int c = values_cmp(l->items[i], best);
            if (g_error) return V_none();
            if ((want_min && c < 0) || (!want_min && c > 0)) best = l->items[i];
        }
        return best;
    }
    if (nargs < 1) { rt_error("TypeError: min()/max() expected at least 1 argument"); return V_none(); }
    Value best = args[0];
    for (int i = 1; i < nargs; i++) {
        int c = values_cmp(args[i], best);
        if (g_error) return V_none();
        if ((want_min && c < 0) || (!want_min && c > 0)) best = args[i];
    }
    return best;
}

static Value builtin_input(Value *args, int nargs) {
    if (nargs >= 1) {
        char *prompt = value_to_cstr(args[0], 0);
        fputs(prompt, 1);
        free(prompt);
    }
    char buf[512];
    if (!fgets(buf, (int)sizeof(buf), 0)) return V_str("");
    int n = (int)strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = 0;
    return V_str(buf);
}

static Value builtin_append(Value *args, int nargs) {
    if (nargs != 2 || args[0].kind != V_LIST) { rt_error("TypeError: append() expects a list and a value"); return V_none(); }
    list_append(args[0].list, args[1]);
    return V_none();
}

/* dispatch: returns 1 and sets *out if `name` is a builtin, else 0 */
static int call_builtin(const char *name, Value *args, int nargs, Value *out) {
    if (strcmp(name, "print") == 0) { *out = builtin_print(args, nargs); return 1; }
    if (strcmp(name, "len") == 0) { *out = builtin_len(args, nargs); return 1; }
    if (strcmp(name, "range") == 0) { *out = builtin_range(args, nargs); return 1; }
    if (strcmp(name, "str") == 0) { *out = builtin_str(args, nargs); return 1; }
    if (strcmp(name, "int") == 0) { *out = builtin_int(args, nargs); return 1; }
    if (strcmp(name, "float") == 0) { *out = builtin_float(args, nargs); return 1; }
    if (strcmp(name, "bool") == 0) { *out = builtin_bool(args, nargs); return 1; }
    if (strcmp(name, "abs") == 0) { *out = builtin_abs(args, nargs); return 1; }
    if (strcmp(name, "min") == 0) { *out = builtin_minmax(args, nargs, 1); return 1; }
    if (strcmp(name, "max") == 0) { *out = builtin_minmax(args, nargs, 0); return 1; }
    if (strcmp(name, "input") == 0) { *out = builtin_input(args, nargs); return 1; }
    if (strcmp(name, "append") == 0) { *out = builtin_append(args, nargs); return 1; }
    return 0;
}

/* ---- expression evaluation ---- */

static Value eval_binop(int op, Value a, Value b) {
    if (op == TK_PLUS) {
        if (a.kind == V_STR && b.kind == V_STR) {
            SB sb; sb_init(&sb); sb_puts(&sb, a.s); sb_puts(&sb, b.s);
            return V_str_own(sb.buf);
        }
        if (a.kind == V_LIST && b.kind == V_LIST) {
            Value r = V_list_new();
            for (int i = 0; i < a.list->nitems; i++) list_append(r.list, a.list->items[i]);
            for (int i = 0; i < b.list->nitems; i++) list_append(r.list, b.list->items[i]);
            return r;
        }
        if (is_numeric(a) && is_numeric(b)) {
            if (a.kind == V_FLOAT || b.kind == V_FLOAT) return V_float(as_float(a) + as_float(b));
            return V_int(as_int(a) + as_int(b));
        }
        rt_errorv("TypeError: unsupported operand type(s) for +: ", type_name(a));
        return V_none();
    }
    if (op == TK_MINUS) {
        if (is_numeric(a) && is_numeric(b)) {
            if (a.kind == V_FLOAT || b.kind == V_FLOAT) return V_float(as_float(a) - as_float(b));
            return V_int(as_int(a) - as_int(b));
        }
        rt_errorv("TypeError: unsupported operand type(s) for -: ", type_name(a));
        return V_none();
    }
    if (op == TK_STAR) {
        if (a.kind == V_STR && (b.kind == V_INT || b.kind == V_BOOL)) {
            long times = as_int(b);
            SB sb; sb_init(&sb);
            for (long k = 0; k < times; k++) sb_puts(&sb, a.s);
            return V_str_own(sb.buf);
        }
        if (b.kind == V_STR && (a.kind == V_INT || a.kind == V_BOOL)) return eval_binop(TK_STAR, b, a);
        if (a.kind == V_LIST && (b.kind == V_INT || b.kind == V_BOOL)) {
            long times = as_int(b);
            Value r = V_list_new();
            for (long k = 0; k < times; k++)
                for (int i = 0; i < a.list->nitems; i++) list_append(r.list, a.list->items[i]);
            return r;
        }
        if (is_numeric(a) && is_numeric(b)) {
            if (a.kind == V_FLOAT || b.kind == V_FLOAT) return V_float(as_float(a) * as_float(b));
            return V_int(as_int(a) * as_int(b));
        }
        rt_errorv("TypeError: unsupported operand type(s) for *: ", type_name(a));
        return V_none();
    }
    if (op == TK_SLASH) {
        if (!is_numeric(a) || !is_numeric(b)) { rt_errorv("TypeError: unsupported operand type(s) for /: ", type_name(a)); return V_none(); }
        double bv = as_float(b);
        if (bv == 0.0) { rt_error("ZeroDivisionError: division by zero"); return V_none(); }
        return V_float(as_float(a) / bv);
    }
    if (op == TK_DSLASH) {
        if (!is_numeric(a) || !is_numeric(b)) { rt_errorv("TypeError: unsupported operand type(s) for //: ", type_name(a)); return V_none(); }
        if (a.kind == V_FLOAT || b.kind == V_FLOAT) {
            double bv = as_float(b);
            if (bv == 0.0) { rt_error("ZeroDivisionError: division by zero"); return V_none(); }
            double q = as_float(a) / bv;
            long fl = (long)q;
            if (q < 0 && (double)fl != q) fl -= 1;
            return V_float((double)fl);
        }
        long bi = as_int(b);
        if (bi == 0) { rt_error("ZeroDivisionError: division by zero"); return V_none(); }
        long ai = as_int(a);
        long q = ai / bi;
        if ((ai % bi != 0) && ((ai < 0) != (bi < 0))) q -= 1;
        return V_int(q);
    }
    if (op == TK_PERCENT) {
        if (a.kind == V_FLOAT || b.kind == V_FLOAT) {
            rt_error("TypeError: % with float operands is not supported in this Python subset");
            return V_none();
        }
        if (!is_numeric(a) || !is_numeric(b)) { rt_errorv("TypeError: unsupported operand type(s) for %%: ", type_name(a)); return V_none(); }
        long bi = as_int(b);
        if (bi == 0) { rt_error("ZeroDivisionError: modulo by zero"); return V_none(); }
        long ai = as_int(a);
        long r = ai % bi;
        if (r != 0 && ((r < 0) != (bi < 0))) r += bi;
        return V_int(r);
    }
    if (op == TK_DSTAR) {
        if (!is_numeric(a) || !is_numeric(b)) { rt_errorv("TypeError: unsupported operand type(s) for **: ", type_name(a)); return V_none(); }
        long exp = as_int(b);
        int want_float = (a.kind == V_FLOAT) || (exp < 0);
        double base = as_float(a);
        double result = 1.0;
        long e = exp < 0 ? -exp : exp;
        for (long k = 0; k < e; k++) result *= base;
        if (exp < 0) {
            if (result == 0.0) { rt_error("ZeroDivisionError: 0.0 cannot be raised to a negative power"); return V_none(); }
            result = 1.0 / result;
        }
        if (want_float) return V_float(result);
        return V_int((long)result);
    }
    rt_error("InternalError: unknown operator");
    return V_none();
}

static Value eval_expr(Node *n, Env *env) {
    if (g_error) return V_none();
    g_depth++;
    if (g_depth > MAX_DEPTH) {
        rt_error("RecursionError: expression too deeply nested");
        g_depth--;
        return V_none();
    }
    Value result = V_none();
    switch (n->kind) {
        case NK_INT: result = V_int(n->ival); break;
        case NK_FLOAT: result = V_float(n->fval); break;
        case NK_STR: result = V_str(n->sval); break;
        case NK_BOOL: result = V_bool((int)n->ival); break;
        case NK_NONE: result = V_none(); break;
        case NK_NAME: {
            Value v;
            if (env->entries && env_get(env, n->sval, &v)) { result = v; break; }
            if (env_get(&g_global, n->sval, &v)) { result = v; break; }
            rt_errorv("NameError: name is not defined: ", n->sval);
            break;
        }
        case NK_LIST: {
            Value lst = V_list_new();
            for (int i = 0; i < n->nitems && !g_error; i++) {
                Value item = eval_expr(n->items[i], env);
                if (g_error) break;
                list_append(lst.list, item);
            }
            result = lst;
            break;
        }
        case NK_UNARYNEG: {
            Value v = eval_expr(n->a, env);
            if (g_error) break;
            if (!is_numeric(v)) { rt_error("TypeError: bad operand type for unary -"); break; }
            if (v.kind == V_FLOAT) result = V_float(-v.f);
            else result = V_int(-as_int(v));
            break;
        }
        case NK_NOT: {
            Value v = eval_expr(n->a, env);
            if (g_error) break;
            result = V_bool(!truthy(v));
            break;
        }
        case NK_AND: {
            Value v = eval_expr(n->a, env);
            if (g_error) break;
            if (!truthy(v)) { result = v; break; }
            result = eval_expr(n->b, env);
            break;
        }
        case NK_OR: {
            Value v = eval_expr(n->a, env);
            if (g_error) break;
            if (truthy(v)) { result = v; break; }
            result = eval_expr(n->b, env);
            break;
        }
        case NK_IN: {
            Value needle = eval_expr(n->a, env);
            if (g_error) break;
            Value hay = eval_expr(n->b, env);
            if (g_error) break;
            int found = 0;
            if (hay.kind == V_LIST) {
                for (int i = 0; i < hay.list->nitems; i++)
                    if (values_equal(needle, hay.list->items[i])) { found = 1; break; }
            } else if (hay.kind == V_STR && needle.kind == V_STR) {
                found = str_find(hay.s, needle.s) >= 0;
            } else {
                rt_error("TypeError: argument of type is not iterable for 'in'");
                break;
            }
            result = V_bool(n->op ? !found : found);
            break;
        }
        case NK_COMPARE: {
            Value left = eval_expr(n->items[0], env);
            if (g_error) break;
            int ok = 1;
            for (int i = 0; i < n->niops && ok; i++) {
                Value right = eval_expr(n->items[i + 1], env);
                if (g_error) { ok = 0; break; }
                int op = n->iops[i];
                if (op == TK_EQ) ok = values_equal(left, right);
                else if (op == TK_NE) ok = !values_equal(left, right);
                else {
                    int c = values_cmp(left, right);
                    if (g_error) { ok = 0; break; }
                    if (op == TK_LT) ok = c < 0;
                    else if (op == TK_GT) ok = c > 0;
                    else if (op == TK_LE) ok = c <= 0;
                    else if (op == TK_GE) ok = c >= 0;
                }
                left = right;
            }
            result = V_bool(ok);
            break;
        }
        case NK_BINOP: {
            Value a = eval_expr(n->a, env);
            if (g_error) break;
            Value b = eval_expr(n->b, env);
            if (g_error) break;
            result = eval_binop(n->op, a, b);
            break;
        }
        case NK_INDEX: {
            Value target = eval_expr(n->a, env);
            if (g_error) break;
            Value idxv = eval_expr(n->b, env);
            if (g_error) break;
            if (target.kind != V_LIST && target.kind != V_STR) {
                rt_errorv("TypeError: object is not subscriptable: ", type_name(target));
                break;
            }
            if (!is_numeric(idxv)) { rt_error("TypeError: list/string indices must be integers"); break; }
            int idx = (int)as_int(idxv);
            if (target.kind == V_LIST) {
                int norm = normalize_index(idx, target.list->nitems);
                if (norm < 0 || norm >= target.list->nitems) { rt_error("IndexError: list index out of range"); break; }
                result = target.list->items[norm];
            } else {
                int len = (int)strlen(target.s);
                int norm = normalize_index(idx, len);
                if (norm < 0 || norm >= len) { rt_error("IndexError: string index out of range"); break; }
                char buf[2] = {target.s[norm], 0};
                result = V_str(buf);
            }
            break;
        }
        case NK_CALL: {
            Value argbuf[32];
            int nargs = n->nitems;
            if (nargs > 32) { rt_error("TypeError: too many arguments (max 32)"); break; }
            for (int i = 0; i < nargs && !g_error; i++) argbuf[i] = eval_expr(n->items[i], env);
            if (g_error) break;
            Value out;
            if (call_builtin(n->sval, argbuf, nargs, &out)) { result = out; break; }
            Node *def = func_lookup(n->sval);
            if (!def) { rt_errorv("NameError: function is not defined: ", n->sval); break; }
            result = call_function(n->sval, def, argbuf, nargs);
            break;
        }
        default:
            rt_error("InternalError: not an expression");
            break;
    }
    g_depth--;
    return result;
}

/* ---- assignment targets ---- */

static void assign_to(Node *target, Value v, Env *env) {
    if (g_error) return;
    if (target->kind == NK_NAME) {
        if (env->entries || env == &g_global) env_set(env, target->sval, v);
        else env_set(env, target->sval, v);
        return;
    }
    if (target->kind == NK_INDEX) {
        Value base = eval_expr(target->a, env);
        if (g_error) return;
        Value idxv = eval_expr(target->b, env);
        if (g_error) return;
        if (base.kind != V_LIST) { rt_error("TypeError: only list items support assignment"); return; }
        if (!is_numeric(idxv)) { rt_error("TypeError: list indices must be integers"); return; }
        int idx = normalize_index((int)as_int(idxv), base.list->nitems);
        if (idx < 0 || idx >= base.list->nitems) { rt_error("IndexError: list assignment index out of range"); return; }
        base.list->items[idx] = v;
        return;
    }
    rt_error("SyntaxError: invalid assignment target");
}

/* ---- statement execution ---- */

static Signal exec_block(Node *blk, Env *env) {
    if (g_error) return SIG_NONE;
    g_depth++;
    if (g_depth > MAX_DEPTH) {
        rt_error("RecursionError: block nested too deeply");
        g_depth--;
        return SIG_NONE;
    }
    Signal sig = SIG_NONE;
    for (int i = 0; i < blk->nitems && !g_error; i++) {
        sig = exec_stmt(blk->items[i], env);
        if (sig != SIG_NONE) break;
    }
    g_depth--;
    return sig;
}

static Signal exec_stmt(Node *n, Env *env) {
    if (g_error) return SIG_NONE;
    g_depth++;
    if (g_depth > MAX_DEPTH) {
        rt_error("RecursionError: statement nested too deeply");
        g_depth--;
        return SIG_NONE;
    }
    Signal sig = SIG_NONE;
    switch (n->kind) {
        case NK_PASS: break;
        case NK_BREAK: sig = SIG_BREAK; break;
        case NK_CONTINUE: sig = SIG_CONTINUE; break;
        case NK_RETURN: {
            Value v = n->a ? eval_expr(n->a, env) : V_none();
            if (!g_error) { g_return_value = v; sig = SIG_RETURN; }
            break;
        }
        case NK_EXPRSTMT: eval_expr(n->a, env); break;
        case NK_ASSIGN: {
            Value v = eval_expr(n->b, env);
            if (!g_error) assign_to(n->a, v, env);
            break;
        }
        case NK_AUGASSIGN: {
            Value cur_v = eval_expr(n->a, env);
            if (g_error) break;
            Value rhs = eval_expr(n->b, env);
            if (g_error) break;
            int op = (n->op == TK_PLUSEQ) ? TK_PLUS : (n->op == TK_MINUSEQ) ? TK_MINUS
                    : (n->op == TK_STAREQ) ? TK_STAR : TK_SLASH;
            Value nv = eval_binop(op, cur_v, rhs);
            if (!g_error) assign_to(n->a, nv, env);
            break;
        }
        case NK_IF: {
            Value cond = eval_expr(n->a, env);
            if (g_error) break;
            if (truthy(cond)) sig = exec_block(n->b, env);
            else if (n->c) sig = (n->c->kind == NK_BLOCK) ? exec_block(n->c, env) : exec_stmt(n->c, env);
            break;
        }
        case NK_WHILE: {
            int iterations = 0;
            for (;;) {
                Value cond = eval_expr(n->a, env);
                if (g_error) break;
                if (!truthy(cond)) break;
                Signal s = exec_block(n->b, env);
                if (g_error) break;
                if (s == SIG_BREAK) break;
                if (s == SIG_RETURN) { sig = s; break; }
                if (++iterations > 50000000) { rt_error("RuntimeError: while loop exceeded iteration limit"); break; }
            }
            break;
        }
        case NK_FOR: {
            Value iterable = eval_expr(n->a, env);
            if (g_error) break;
            if (iterable.kind != V_LIST && iterable.kind != V_STR) {
                rt_errorv("TypeError: object is not iterable: ", type_name(iterable));
                break;
            }
            int len = (iterable.kind == V_LIST) ? iterable.list->nitems : (int)strlen(iterable.s);
            for (int i = 0; i < len; i++) {
                Value item;
                if (iterable.kind == V_LIST) item = iterable.list->items[i];
                else { char buf[2] = {iterable.s[i], 0}; item = V_str(buf); }
                env_set(env, n->sval, item);
                Signal s = exec_block(n->b, env);
                if (g_error) break;
                if (s == SIG_BREAK) break;
                if (s == SIG_RETURN) { sig = s; break; }
            }
            break;
        }
        case NK_FUNCDEF:
            func_define(n->sval, n);
            break;
        case NK_BLOCK:
            sig = exec_block(n, env);
            break;
        default:
            rt_error("InternalError: not a statement");
            break;
    }
    g_depth--;
    return sig;
}

/* ============================================================================
 * Driver: script runner + REPL
 * ==========================================================================*/

static void reset_error(void) { g_error = 0; g_error_msg[0] = 0; }

static void run_source(const char *src, Env *env) {
    TokList tl;
    memset(&tl, 0, sizeof(tl));
    reset_error();
    g_depth = 0;
    if (!lex(src, &tl)) {
        fputs("python: ", 1);
        fputs(g_error_msg, 1);
        fputs("\n", 1);
        free(tl.toks);
        return;
    }
    g_depth = 0;
    Node *prog = parse_program(tl.toks);
    if (g_error) {
        fputs("python: ", 1);
        fputs(g_error_msg, 1);
        fputs("\n", 1);
        free(tl.toks);
        return;
    }
    g_depth = 0;
    exec_block(prog, env);
    if (g_error) {
        fputs("python: ", 1);
        fputs(g_error_msg, 1);
        fputs("\n", 1);
    }
    free(tl.toks);
}

/* Hard ceiling on script size -- generous, but this is a cap, not the
 * amount we actually allocate. The real allocation below is sized to the
 * file's real length (via fstat), not this ceiling: the whole per-process
 * arena (code+data+bss+heap) only has E32_RUNTIME_MAX (256KB) to begin
 * with, so unconditionally xmalloc'ing a fixed 256KB read buffer left no
 * room for the interpreter's own image and failed with "out of memory" on
 * every script, even a one-liner. */
#define SCRIPT_MAX (256 * 1024)
#define SCRIPT_MIN_BUF 4096

static int run_script(const char *path) {
    e32_stat_t st;
    size_t want = SCRIPT_MIN_BUF;
    if (e32_fstat(path, &st) == 0 && !st.is_dir && st.size > 0) {
        size_t sz = (size_t)st.size;
        if (sz > SCRIPT_MAX - 1) sz = SCRIPT_MAX - 1;
        want = sz + 1; /* +1 for the trailing NUL we add below */
        if (want < SCRIPT_MIN_BUF) want = SCRIPT_MIN_BUF;
    }

    char *buf = (char *)xmalloc(want);
    size_t cap = want;

    int fd = e32_open(path, O_RDONLY);
    if (fd < 0) {
        fputs("python: cannot open '", 1);
        fputs(path, 1);
        fputs("'\n", 1);
        free(buf);
        return 1;
    }
    int total = 0;
    for (;;) {
        if ((size_t)total >= cap - 1) {
            if (cap >= SCRIPT_MAX) break;
            size_t ncap = cap * 2;
            if (ncap > SCRIPT_MAX) ncap = SCRIPT_MAX;
            buf = (char *)xrealloc(buf, ncap);
            cap = ncap;
        }
        int n = e32_read(fd, buf + total, (int)(cap - 1 - (size_t)total));
        if (n <= 0) break;
        total += n;
    }
    e32_close(fd);
    buf[total] = 0;

    env_init(&g_global);
    run_source(buf, &g_global);
    free(buf);
    return g_error ? 1 : 0;
}

/* Reads one logical REPL "chunk": a single line, or (if it opens a block)
 * keeps reading continuation lines -- prompted with "... " -- until a blank
 * line closes the block. This mirrors the real Python REPL closely enough
 * for interactive use. */
static int line_opens_block(const char *line) {
    /* naive but effective: a top-level line ending in ':' (ignoring trailing
     * whitespace/comment) opens a block */
    int len = (int)strlen(line);
    int i = len - 1;
    while (i >= 0 && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r' || line[i] == '\n')) i--;
    return i >= 0 && line[i] == ':';
}

static int line_is_blank(const char *line) {
    for (int i = 0; line[i]; i++)
        if (line[i] != ' ' && line[i] != '\t' && line[i] != '\r' && line[i] != '\n') return 0;
    return 1;
}

static void run_repl(void) {
    env_init(&g_global);
    fputs("Eclipse32 Python subset -- interactive mode.\n", 1);
    fputs("Type an expression/statement and press enter. Ctrl+C-free: type exit() has no effect, use 'q' if your shell supports it, or just close the app.\n", 1);

    SB chunk;
    for (;;) {
        sb_init(&chunk);
        fputs(">>> ", 1);
        char line[1024];
        if (!fgets(line, (int)sizeof(line), 0)) break;
        sb_puts(&chunk, line);
        if (line_opens_block(line)) {
            for (;;) {
                fputs("... ", 1);
                char cont[1024];
                if (!fgets(cont, (int)sizeof(cont), 0)) break;
                if (line_is_blank(cont)) { sb_putc(&chunk, '\n'); break; }
                sb_puts(&chunk, cont);
                if (chunk.buf[chunk.len - 1] != '\n') sb_putc(&chunk, '\n');
            }
        }
        run_source(chunk.buf, &g_global);
        free(chunk.buf);
    }
    fputs("\n", 1);
}

int main(int argc, char **argv) {
    if (argc >= 2) {
        return run_script(argv[1]);
    }
    run_repl();
    return 0;
}
