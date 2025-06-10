#include <ctype.h> /* for isspace */
#include <limits.h>
#include <math.h> /* for pow */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "expr.h"


/*
 * Static buffer declaration
 */

struct tokens_t tokens;
struct parsing_t parsing;
struct variables_t variables;

struct operations_t {
    struct expr_string expr_strs[10];
    int len;
};

struct args_t {
    struct expr_arg expr_args[EXPR_ARG_SIZE];
    int len;
};

struct macro_tokens_t {
    struct expr tokens[EXPR_ARG_SIZE];
    uint8_t len;
};

struct operations_t ops = {0};
struct args_t args = {0};
//struct macro_tokens_t mtks;


/*
 * Simple expandable vector implementation
 */
//int vec_expand(char **buf, int *length, int *cap, int memsz) {
//    if (*length + 1 > *cap) {
//        void *ptr;
//        int n = (*cap == 0) ? 1 : *cap << 1;
//        ptr = realloc(*buf, n * memsz);
//        if (ptr == NULL) {
//            return -1; /* allocation failed */
//        }
//        *buf = (char *) ptr;
//        *cap = n;
//    }
//    return 0;
//}

/*
 * Expression data types
 */


static int prec[] = {0, 1, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 5,
                     5, 5, 5, 5, 6, 7, 8, 9, 10, 11, 12, 0, 0, 0};


#define expr_init()                                                            \
  { .type = (enum expr_type)0 }


typedef vec(struct expr_string) vec_str_t;
typedef vec(struct expr_arg) vec_arg_t;

static int expr_is_unary(enum expr_type op) {
    return op == OP_UNARY_MINUS || op == OP_UNARY_LOGICAL_NOT ||
           op == OP_UNARY_BITWISE_NOT;
}

static int expr_is_binary(enum expr_type op) {
    return !expr_is_unary(op) && op != OP_CONST && op != OP_VAR &&
           op != OP_FUNC && op != OP_UNKNOWN;
}

static int expr_prec(enum expr_type a, enum expr_type b) {
    int left =
            expr_is_binary(a) && a != OP_ASSIGN && a != OP_POWER && a != OP_COMMA;
    return (left && prec[a] >= prec[b]) || (prec[a] > prec[b]);
}

#define isfirstvarchr(c)                                                       \
  (((unsigned char)c >= '@' && c != '^' && c != '|') || c == '$')
#define isvarchr(c)                                                            \
  (((unsigned char)c >= '@' && c != '^' && c != '|') || c == '$' ||            \
   c == '#' || (c >= '0' && c <= '9'))

static struct {
    const char *s;
    const enum expr_type op;
} OPS[] = {
        {"-u", OP_UNARY_MINUS},
        {"!u", OP_UNARY_LOGICAL_NOT},
        {"^u", OP_UNARY_BITWISE_NOT},
        {"**", OP_POWER},
        {"*",  OP_MULTIPLY},
        {"/",  OP_DIVIDE},
        {"%",  OP_REMAINDER},
        {"+",  OP_PLUS},
        {"-",  OP_MINUS},
        {"<<", OP_SHL},
        {">>", OP_SHR},
        {"<",  OP_LT},
        {"<=", OP_LE},
        {">",  OP_GT},
        {">=", OP_GE},
        {"==", OP_EQ},
        {"!=", OP_NE},
        {"&",  OP_BITWISE_AND},
        {"|",  OP_BITWISE_OR},
        {"^",  OP_BITWISE_XOR},
        {"&&", OP_LOGICAL_AND},
        {"||", OP_LOGICAL_OR},
        {"=",  OP_ASSIGN},
        {",",  OP_COMMA},

        /* These are used by lexer and must be ignored by parser, so we put
           them at the end */
        {"-",  OP_UNARY_MINUS},
        {"!",  OP_UNARY_LOGICAL_NOT},
        {"^",  OP_UNARY_BITWISE_NOT},
};

static enum expr_type expr_op(const char *s, size_t len, int unary) {
    for (unsigned int i = 0; i < sizeof(OPS) / sizeof(OPS[0]); i++) {
        if (strlen(OPS[i].s) == len && strncmp(OPS[i].s, s, len) == 0 &&
            (unary == -1 || expr_is_unary(OPS[i].op) == unary)) {
            return OPS[i].op;
        }
    }
    return OP_UNKNOWN;
}

static float expr_parse_number(const char *s, size_t len) {
    float num = 0;
    unsigned int frac = 0;
    unsigned int digits = 0;
    for (unsigned int i = 0; i < len; i++) {
        if (s[i] == '.' && frac == 0) {
            frac++;
            continue;
        }
        if (isdigit(s[i])) {
            digits++;
            if (frac > 0) {
                frac++;
            }
            num = num * 10 + (s[i] - '0');
        } else {
            return NAN;
        }
    }
    while (frac > 1) {
        num = num / 10;
        frac--;
    }
    return (digits > 0 ? num : NAN);
}

/*
 * Functions
 */


static struct expr_func *expr_func(struct expr_func *funcs, const char *s,
                                   size_t len) {
    for (struct expr_func *f = funcs; *f->name; f++) {
        if (strlen(f->name) == len && strncmp(f->name, s, len) == 0) {
            return f;
        }
    }
    return NULL;
}

/*
 * Variables
 */

struct expr_var *expr_var(const char *s,
                          size_t len) {
    struct expr_var *v = NULL;
    if (len == 0 || !isfirstvarchr(*s)) {
        return NULL;
    }
    //search for variable
    int i = 0;
    for (; i < variables.len; i++) {
        v = &variables.variables[i];
        if (strlen(v->name) == len && strncmp(v->name, s, len) == 0) {
            return v;
        }
    }

    //create new
//    v = (struct expr_var *) calloc(1, sizeof(struct expr_var) + len + 1);
    if (variables.len == EXPR_VAR_COUNT - 1) {
        return NULL; /* allocation failed */
    }
    v = &variables.variables[i];
    variables.len += 1;
    v->value = 0;
    strncpy(v->name, s, len);
    v->name[len] = '\0';
    return v;
}

static int to_int(float x) {
    if (isnan(x)) {
        return 0;
    } else if (isinf(x) != 0) {
        return INT_MAX * isinf(x);
    } else {
        return (int) x;
    }
}

float expr_eval(struct expr *e) {
    float n;
    switch (e->type) {
        case OP_UNARY_MINUS:
            return -(expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]]));
        case OP_UNARY_LOGICAL_NOT:
            return !(expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]]));
        case OP_UNARY_BITWISE_NOT:
            return ~(to_int(expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]])));
        case OP_POWER:
            return powf(expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]]),
                        expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]));
        case OP_MULTIPLY:
            return expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]]) *
                   expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]);
        case OP_DIVIDE:
            return expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]]) /
                   expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]);
        case OP_REMAINDER:
            return fmodf(expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]]),
                         expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]));
        case OP_PLUS:
            return expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]]) +
                   expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]);
        case OP_MINUS:
            return expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]]) -
                   expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]);
        case OP_SHL:
            return to_int(expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]]))
                    << to_int(expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]));
        case OP_SHR:
            return to_int(expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]])) >>
                                                                               to_int(expr_eval(
                                                                                       &tokens.tokens[e->param.op.args.ptrs[1]]));
        case OP_LT:
            return expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]]) <
                   expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]);
        case OP_LE:
            return expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]]) <=
                   expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]);
        case OP_GT:
            return expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]]) >
                   expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]);
        case OP_GE:
            return expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]]) >=
                   expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]);
        case OP_EQ:
            return expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]]) ==
                   expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]);
        case OP_NE:
            return expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]]) !=
                   expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]);
        case OP_BITWISE_AND:
            return to_int(expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]])) &
                   to_int(expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]));
        case OP_BITWISE_OR:
            return to_int(expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]])) |
                   to_int(expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]));
        case OP_BITWISE_XOR:
            return to_int(expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]])) ^
                   to_int(expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]));
        case OP_LOGICAL_AND:
            n = expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]]);
            if (n != 0) {
                n = expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]);
                if (n != 0) {
                    return n;
                }
            }
            return 0;
        case OP_LOGICAL_OR:
            n = expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]]);
            if (n != 0 && !isnan(n)) {
                return n;
            } else {
                n = expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]);
                if (n != 0) {
                    return n;
                }
            }
            return 0;
        case OP_ASSIGN:
            n = expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]);
            if (nth_token(e->param.op.args, 0).type == OP_VAR) {
                *nth_token(e->param.op.args, 0).param.var.value = n;
//                *e->param.op.args.buf[0].param.var.value = n;
            }
            return n;
        case OP_COMMA:
            expr_eval(&tokens.tokens[e->param.op.args.ptrs[0]]);
            return expr_eval(&tokens.tokens[e->param.op.args.ptrs[1]]);
        case OP_CONST:
            return e->param.num.value;
        case OP_VAR:
            return *e->param.var.value;
        case OP_FUNC:
            return e->param.func.f->f(e->param.func.f, &e->param.func.args,
                                      e->param.func.context);
        default:
            return NAN;
    }
}


int expr_next_token(const char *s, size_t len, int *flags) {
    unsigned int i = 0;
    if (len == 0) {
        return 0;
    }
    char c = s[0];
    if (c == '#') {
        for (; i < len && s[i] != '\n'; i++);
        return i;
    } else if (c == '\n') {
        for (; i < len && isspace(s[i]); i++);
        if (*flags & EXPR_TOP) {
            if (i == len || s[i] == ')') {
                *flags = *flags & (~EXPR_COMMA);
            } else {
                *flags = EXPR_TNUMBER | EXPR_TWORD | EXPR_TOPEN | EXPR_COMMA;
            }
        }
        return i;
    } else if (isspace(c)) {
        while (i < len && isspace(s[i]) && s[i] != '\n') {
            i++;
        }
        return i;
    } else if (isdigit(c)) {
        if ((*flags & EXPR_TNUMBER) == 0) {
            return -1; // unexpected number
        }
        *flags = EXPR_TOP | EXPR_TCLOSE;
        while ((c == '.' || isdigit(c)) && i < len) {
            i++;
            c = s[i];
        }
        return i;
    } else if (isfirstvarchr(c)) {
        if ((*flags & EXPR_TWORD) == 0) {
            return -2; // unexpected word
        }
        *flags = EXPR_TOP | EXPR_TOPEN | EXPR_TCLOSE;
        while ((isvarchr(c)) && i < len) {
            i++;
            c = s[i];
        }
        return i;
    } else if (c == '(' || c == ')') {
        if (c == '(' && (*flags & EXPR_TOPEN) != 0) {
            *flags = EXPR_TNUMBER | EXPR_TWORD | EXPR_TOPEN | EXPR_TCLOSE;
        } else if (c == ')' && (*flags & EXPR_TCLOSE) != 0) {
            *flags = EXPR_TOP | EXPR_TCLOSE;
        } else {
            return -3; // unexpected parenthesis
        }
        return 1;
    } else {
        if ((*flags & EXPR_TOP) == 0) {
            if (expr_op(&c, 1, 1) == OP_UNKNOWN) {
                return -4; // missing expected operand
            }
            *flags = EXPR_TNUMBER | EXPR_TWORD | EXPR_TOPEN | EXPR_UNARY;
            return 1;
        } else {
            int found = 0;
            while (!isvarchr(c) && !isspace(c) && c != '(' && c != ')' && i < len) {
                if (expr_op(s, i + 1, 0) != OP_UNKNOWN) {
                    found = 1;
                } else if (found) {
                    break;
                }
                i++;
                c = s[i];
            }
            if (!found) {
                return -5; // unknown operator
            }
            *flags = EXPR_TNUMBER | EXPR_TWORD | EXPR_TOPEN;
            return i;
        }
    }
}

#define EXPR_PAREN_ALLOWED 0
#define EXPR_PAREN_EXPECTED 1
#define EXPR_PAREN_FORBIDDEN 2

/// Creates unary or binary expression from string representation and top post 2 tokens from parsing stack
static int expr_bind(const char *s, size_t len) {
    enum expr_type op = expr_op(s, len, -1);
    if (op == OP_UNKNOWN) {
        return -1;
    }

    if (expr_is_unary(op)) {
        if (parsing.len < 1) {
            return -1;
        }
        struct expr arg = parsing_pop();
        struct expr unary = expr_init();
        unary.type = op;
        arg_push(&unary.param.op.args, arg);
        parsing_push(unary);
    } else {
        if (parsing.len < 2) {
            return -1;
        }
        struct expr b = parsing_pop();
        struct expr a = parsing_pop();
        struct expr binary = expr_init();
        binary.type = op;
        if (op == OP_ASSIGN && a.type != OP_VAR) {
            return -1; /* Bad assignment */
        }
        arg_push(&binary.param.op.args, a);
        arg_push(&binary.param.op.args, b);
        parsing_push(binary);
    }
    return 0;
}

static struct expr expr_const(float value) {
    struct expr e = expr_init();
    e.type = OP_CONST;
    e.param.num.value = value;
    return e;
}

static struct expr expr_varref(struct expr_var *v) {
    struct expr e = expr_init();
    e.type = OP_VAR;
    e.param.var.value = &v->value;
    return e;
}


struct expr *expr_create(const char *str, size_t len,
                         struct expr_func *funcs) {
    memset(&parsing, 0, sizeof(struct parsing_t));

    float num;
    struct expr_var *v;
    const char *id = NULL;
    size_t idn = 0;

    struct expr *result = NULL;

    // expression stack
//    expr_ptr_arr_t es = vec_init();
    // operation stack
//    vec_str_t os = vec_init();
    // argument stack
//    vec_arg_t as = vec_init();

//    struct macro {
//        char *name;
//        struct expr_ptr_arr_t body;
//    };
//    vec(struct macro) macros = vec_init();

    int flags = EXPR_TDEFAULT;
    int paren = EXPR_PAREN_ALLOWED;

    //foreach token in string
    for (;;) {
        int n = expr_next_token(str, len, &flags);
        if (n == 0) {
            break;
        } else if (n < 0) {
            goto cleanup;
        }
        const char *tok = str;
        str = str + n;
        len = len - n;
        if (*tok == '#') {
            continue;
        }
        // prefix unary ops with u
        if (flags & EXPR_UNARY) {
            if (n == 1) {
                switch (*tok) {
                    case '-':
                        tok = "-u";
                        break;
                    case '^':
                        tok = "^u";
                        break;
                    case '!':
                        tok = "!u";
                        break;
                    default:
                        goto cleanup;
                }
                n = 2;
            }
        }
        if (*tok == '\n' && (flags & EXPR_COMMA)) {
            flags = flags & (~EXPR_COMMA);
            n = 1;
            tok = ",";
        }
        if (isspace(*tok)) {
            continue;
        }
        int paren_next = EXPR_PAREN_ALLOWED;

        if (idn > 0) {
            if (n == 1 && *tok == '(') {
                int i;
                int has_macro = 0;
//                struct macro m;
//                vec_foreach(&macros, m, i) {
//                        if (strlen(m.name) == idn && strncmp(m.name, id, idn) == 0) {
//                            has_macro = 1;
//                            break;
//                        }
//                    }
                if ((idn == 1 && id[0] == '$') || has_macro ||
                    expr_func(funcs, id, idn) != NULL) {
                    struct expr_string str = {id, (int) idn};
                    ops_push(str);
                    paren = EXPR_PAREN_EXPECTED;
                } else {
                    goto cleanup; /* invalid function name */
                }
            } else if ((v = expr_var(id, idn)) != NULL) {
                parsing_push(expr_varref(v));
                paren = EXPR_PAREN_FORBIDDEN;
            }
            id = NULL;
            idn = 0;
        }

        if (n == 1 && *tok == '(') {
            if (paren == EXPR_PAREN_EXPECTED) {
                struct expr_string str = {"{", 1};
                ops_push(str);
                struct expr_arg arg = {ops.len, parsing.len, 0};
                gargs_push(arg);
            } else if (paren == EXPR_PAREN_ALLOWED) {
                struct expr_string str = {"(", 1};
                ops_push(str);
            } else {
                goto cleanup; // Bad call
            }
        } else if (paren == EXPR_PAREN_EXPECTED) {
            goto cleanup; // Bad call
        } else if (n == 1 && *tok == ')') {
            //function usage end
            int minlen = (args.len > 0 ? gargs_peek().oslen : 0);
            while (ops.len > minlen && *ops_peek().s != '(' &&
                   *ops_peek().s != '{') {
                struct expr_string str = ops_pop();
                if (expr_bind(str.s, str.n) == -1) {
                    goto cleanup;
                }
            }
            if (ops.len == 0) {
                goto cleanup; // Bad parens
            }
            struct expr_string str = ops_pop();
            if (str.n == 1 && *str.s == '{') {
                str = ops_pop();
                struct expr_arg arg = gargs_pop();
                if (parsing.len > arg.eslen) {
                    //arguments for macro or function
                    tokens_push(parsing_pop());
                    arg.args.ptrs[arg.args.len++] = tokens.len - 1;
//                    vec_push(&arg.args, vec_pop(&es));
                }
                if (str.n == 1 && str.s[0] == '$') {
                    if (vec_len(&arg.args) < 1) {
                        *(uint32_t *) arg.args.ptrs = 0;
                        arg.args.len = 0;
//                        vec_free(&arg.args);
                        goto cleanup; /* too few arguments for $() function */
                    }
                    struct expr *u = &tokens.tokens[arg.args.ptrs[0]];
                    if (u->type != OP_VAR) {
                        *(uint32_t *) arg.args.ptrs = 0;
                        arg.args.len = 0;
//                        vec_free(&arg.args);
                        goto cleanup; /* first argument is not a variable */
                    }
                    //for (struct expr_var *v = vars->head; v; v = v->next) {
                    for (int i = 0; i < variables.len; i++) {
                        struct expr_var *v = &variables.variables[i];
                        if (&v->value == u->param.var.value) {
//                            struct macro m = {v->name, arg.args};
//                            vec_push(&macros, m);
                            break;
                        }
                    }
                    parsing_push(expr_const(0));
//                    vec_push(&es, expr_const(0));
                } else {
                    int i = 0;
                    int found = -1;
//                    struct macro m;
//                    vec_foreach(&macros, m, i) {
//                            if (strlen(m.name) == (size_t) str.n &&
//                                strncmp(m.name, str.s, str.n) == 0) {
//                                found = i;
//                            }
//                        }
                    if (found != -1) {
//                        m = vec_nth(&macros, found);
                        struct expr root = expr_const(0);
                        struct expr *p = &root;
                        /* Assign macro parameters */
//                        for (int j = 0; j < vec_len(&arg.args); j++) {
//                            char varname[4];
//                            snprintf(varname, sizeof(varname) - 1, "$%d", (j + 1));
//                            struct expr_var *v = expr_var(varname, strlen(varname));
//                            struct expr ev = expr_varref(v);
//                            struct expr assign =
//                                    expr_binary(OP_ASSIGN, ev, tokens.tokens[arg.args.ptrs[j]]);
//                            *p = expr_binary(OP_COMMA, assign, expr_const(0));
//                            p = &nth_token(p->param.op.args, 1);
//                        }
                        /* Expand macro body */
//                        for (int j = 1; j < vec_len(&m.body); j++) {
//                            if (j < vec_len(&m.body) - 1) {
//                                *p = expr_binary(OP_COMMA, expr_const(0), expr_const(0));
//                                expr_copy(&nth_token(p->param.op.args, 0), &tokens.tokens[m.body.ptrs[j]]);
//                            } else {
//                                expr_copy(p, &tokens.tokens[m.body.ptrs[j]]);
//                            }
//                            p = &nth_token(p->param.op.args, 1);
////                            p = &vec_nth(&, 1);
//                        }
                        parsing_push(root);
//                        vec_push(&es,);
//                        vec_free(&arg.args);
                        *(uint32_t *) arg.args.ptrs = 0;
                        arg.args.len = 0;
                    } else {
                        struct expr_func *f = expr_func(funcs, str.s, str.n);
                        struct expr bound_func = expr_init();
                        bound_func.type = OP_FUNC;
                        bound_func.param.func.f = f;
                        bound_func.param.func.args = arg.args;
//                        if (f->ctxsz > 0) {
//                            void *p = calloc(1, f->ctxsz);
//                            if (p == NULL) {
//                                goto cleanup; /* allocation failed */
//                            }
//                            bound_func.param.func.context = p;
//                        }
                        parsing_push(bound_func);
//                        vec_push(&es,);
                    }
                }
            }
            paren_next = EXPR_PAREN_FORBIDDEN;
        } else if (!isnan(num = expr_parse_number(tok, n))) {
            parsing_push(expr_const(num));
//            vec_push(&es, expr_const(num));
            paren_next = EXPR_PAREN_FORBIDDEN;
        } else if (expr_op(tok, n, -1) != OP_UNKNOWN) {
            enum expr_type op = expr_op(tok, n, -1);
            struct expr_string o2 = {NULL, 0};
            if (ops.len > 0) {
                o2 = ops_peek();
            }
            for (;;) {
                if (n == 1 && *tok == ',' && ops.len > 0) {
                    struct expr_string str = ops_peek();
                    if (str.n == 1 && *str.s == '{') {
                        struct expr e = parsing_pop();
                        arg_push(&gargs_peek().args, e);
//                        vec_push(&.args, e);
                        break;
                    }
                }
                enum expr_type type2 = expr_op(o2.s, o2.n, -1);
                if (!(type2 != OP_UNKNOWN && expr_prec(op, type2))) {
                    struct expr_string str = {tok, n};
                    ops_push(str);
//                    vec_push(&os, str);
                    break;
                }

                if (expr_bind(o2.s, o2.n) == -1) {
                    goto cleanup;
                }
                (void) ops_pop();
                if (ops.len > 0) {
                    o2 = ops_peek();
                } else {
                    o2.n = 0;
                }
            }
        } else {
            if (n > 0 && !isdigit(*tok)) {
                /* Valid identifier, a variable or a function */
                id = tok;
                idn = n;
            } else {
                goto cleanup; // Bad variable name, e.g. '2.3.4' or '4ever'
            }
        }
        paren = paren_next;
    }

    if (idn > 0) {
        parsing_push(expr_varref(expr_var(id, idn)));
//        vec_push(&es, expr_varref(expr_var(vars, id, idn)));
    }

    while (ops.len > 0) {
        struct expr_string rest = ops_pop();
        if (rest.n == 1 && (*rest.s == '(' || *rest.s == ')')) {
            goto cleanup; // Bad paren
        }
        if (expr_bind(rest.s, rest.n) == -1) {
            goto cleanup;
        }
    }

    if (parsing.len > 0) {
        result = &parsing.tokens[parsing.len - 1];
    } else {
        result = nullptr;
    }

    cleanup:
    memset(&ops, 0, sizeof(struct operations_t));
    memset(&args, 0, sizeof(struct args_t));
    return result;
}

void expr_destroy_expr() {
    memset(&tokens, 0, sizeof(struct tokens_t));
}

void expr_destroy_vars() {
    memset(&variables, 0, sizeof(struct variables_t));
}
