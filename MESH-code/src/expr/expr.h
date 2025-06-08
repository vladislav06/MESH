#ifndef EXPR_H
#define EXPR_H

#include "stdint.h"
#include <stdio.h>

#define EXPR_TOKEN_COUNT    50
#define EXPR_VAR_COUNT      10
#define EXPR_ARG_SIZE        4
#define EXPR_VAR_NAME_SIZE   3
#define EXPR_FN_NAME_SIZE    4
#define EXPR_PARSE_SIZE      10


#define vec(T)                                                                 \
  struct {                                                                     \
    T *buf;                                                                    \
    int len;                                                                   \
    int cap;                                                                   \
  }

struct expr;
struct expr_func;

enum expr_type {
    OP_UNKNOWN,
    OP_UNARY_MINUS,
    OP_UNARY_LOGICAL_NOT,
    OP_UNARY_BITWISE_NOT,

    OP_POWER,
    OP_DIVIDE,
    OP_MULTIPLY,
    OP_REMAINDER,

    OP_PLUS,
    OP_MINUS,

    OP_SHL,
    OP_SHR,

    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE,
    OP_EQ,
    OP_NE,

    OP_BITWISE_AND,
    OP_BITWISE_OR,
    OP_BITWISE_XOR,

    OP_LOGICAL_AND,
    OP_LOGICAL_OR,

    OP_ASSIGN,
    OP_COMMA,

    OP_CONST,
    OP_VAR,
    OP_FUNC,
};

//typedef vec(struct expr) expr_ptr_arr_t;
struct expr_ptr_arr_t {
    uint8_t ptrs[EXPR_ARG_SIZE];
    uint8_t len;
};

#define vec_init()                                                             \
  { NULL, 0, 0 }
#define vec_len(v) ((v)->len)
#define vec_unpack(v)                                                          \
  (char **)&(v)->buf, &(v)->len, &(v)->cap, sizeof(*(v)->buf)
#define vec_push(v, val)                                                       \
  vec_expand(vec_unpack(v)) ? -1 : ((v)->buf[(v)->len++] = (val), 0)
#define vec_nth(v, i) (v)->buf[i]


#define nth_token(v, i) tokens.tokens[(v).ptrs[i]]

//#define arg_pop(v) tokens.tokens[(v)->ptrs[--(v)->len]]
// saves argument into tokens stack and saves address into argument stack
#define arg_push(v, val)  ((v)->len>=EXPR_ARG_SIZE-1? -1 : (tokens.tokens[tokens.len++]=(val),(v)->ptrs[(v)->len++] = (tokens.len-1), 0))
#define tokens_push(val) tokens.tokens[tokens.len++]=(val)
#define arg_foreach(v, var, iter)   \
  if ((v)->len > 0)                                                            \
    for ((iter) = 0; (iter) < (v)->len && (((var) = tokens.tokens[(v)->ptrs[(iter)]]), 1);     \
         ++(iter))


//#define nth_expr(v) tokens.tokens[(v)]
//#define expr_push(val)  (tokens.len<EXPR_TOKEN_COUNT-1? -1 : ((tokens.tokens[tokens.len++] = (val), 0)))

#define parsing_pop() parsing.tokens[--parsing.len]
#define parsing_push(val) (parsing.len>=EXPR_PARSE_SIZE-1? -1 : ((parsing.tokens[parsing.len++] = (val), 0)))

#define ops_push(v) ops.expr_strs[ops.len++]=(v)
#define ops_pop() ops.expr_strs[--ops.len]
#define ops_peek() ops.expr_strs[ops.len -1]

#define gargs_push(v) args.expr_args[args.len++]=(v)
#define gargs_pop() args.expr_args[--args.len]
#define gargs_peek() args.expr_args[args.len-1]

#define vec_free(v) (free((v)->buf), (v)->buf = NULL, (v)->len = (v)->cap = 0)
#define vec_foreach(v, var, iter)                                              \
  if ((v)->len > 0)                                                            \
    for ((iter) = 0; (iter) < (v)->len && (((var) = (v)->buf[(iter)]), 1);     \
         ++(iter))
#define vec_foreacha(v, var, iter)                                              \
  if ((v)->len > 0)                                                            \
    for ((iter) = 0; (iter) < (v)->len && (((var) =tokens.tokens[ (v)->ptrs[(iter)]]), 1);     \
         ++(iter))

#define vec_foreachm(v, var, iter)                                              \
  if ((v)->len > 0)                                                            \
    for ((iter) = 0; (iter) < (v)->len && (((var) = (v)->ptr[(iter)]), 1);     \
         ++(iter))

#define EXPR_TOP (1 << 0)
#define EXPR_TOPEN (1 << 1)
#define EXPR_TCLOSE (1 << 2)
#define EXPR_TNUMBER (1 << 3)
#define EXPR_TWORD (1 << 4)
#define EXPR_TDEFAULT (EXPR_TOPEN | EXPR_TNUMBER | EXPR_TWORD)

#define EXPR_UNARY (1 << 5)
#define EXPR_COMMA (1 << 6)

struct expr {
    enum expr_type type;
    union {
        struct {
            float value;
        } num;
        struct {
            float *value;
        } var;
        struct {
            struct expr_ptr_arr_t args;
        } op;
        struct {
            struct expr_func *f;
            struct expr_ptr_arr_t args;
            void *context;
        } func;
    } param;
};

struct expr_string {
    const char *s;
    int n;
};
struct expr_arg {
    int oslen;

    int eslen;
    struct expr_ptr_arr_t args;
};
struct expr_var {
    float value;
//    struct expr_var *next;
    char name[EXPR_VAR_NAME_SIZE];
};


typedef void (*exprfn_cleanup_t)(struct expr_func *f, void *context);

typedef float (*exprfn_t)(struct expr_func *f, struct expr_ptr_arr_t *args, void *context);

struct expr_func {
    char name[EXPR_FN_NAME_SIZE];
    exprfn_t f;
//    exprfn_cleanup_t cleanup;
//    size_t ctxsz;
};


struct tokens_t {
    struct expr tokens[EXPR_TOKEN_COUNT];
    uint8_t len;
};

struct variables_t {
    struct expr_var variables[EXPR_VAR_COUNT];
    uint8_t len;
};

struct parsing_t {
    struct expr tokens[EXPR_PARSE_SIZE];
    uint8_t len;
};

extern struct tokens_t tokens;
extern struct variables_t variables;


//void expr_destroy(struct expr *e);
void expr_destroy_expr();

void expr_destroy_vars();

float expr_eval(struct expr *e);

struct expr *expr_create(const char *str, size_t len,
                         struct expr_func *funcs);

struct expr_var *expr_var(const char *s,
                          size_t len);

int expr_next_token(const char *s, size_t len, int *flags);

int vec_expand(char **buf, int *length, int *cap, int memsz);

#endif /* EXPR_H */
