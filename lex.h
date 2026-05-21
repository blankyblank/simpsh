/* lex.h - tokenizer and parser functions and declarations */
#ifndef LEX_H
#define LEX_H

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stddef.h>
#include "utils.h"

typedef enum {
  TNONE,
  TNOT,
  TPIPE,
  TWORD,
  TAND,
  TOR,
  TSEMI,
  TNL,
  TEOF,
  TLP,
  TRP,
  TLB,
  TRB,
  TBKGRND,
  TREDIR,
} token;

typedef enum {
  QNONE,
  QDOUBLE,
  QSINGLE,
} quoted;

/**
 * word fragment
 * @field wf pointer to next node
 * @field string word
 * @field enum quoted qs
 * @field word length
 */
typedef struct wf wf;
struct wf {
  wf *next;
  char *word;
  quoted qs;
  size_t len;
};

/** store tokens before building argv */
typedef struct {
  wf *cmd;
  token type;
  int sub;
} sh_tok;

typedef struct strpush strpush;
struct strpush {
    strpush *prev;
    char *saved_nchar;
    size_t saved_nleft;
    int saved_unget;
    char saved_ungetbuf[4];
    int alias;
};

typedef struct redir redir;
struct redir {
  redir *next;
  int fd;
  int type;
  wf *name;
};

/*
 * AST node for commands
 * @field node left
 * @field node right
 * @field enum node type
 * @field union of ast structs
 * @field enum node type
 * @field word fragment array args
 * @field int flags (bg, negate)
 */
typedef struct cmd_tree cmd_tree;
struct cmd_tree {
  cmd_tree *left;
  cmd_tree *right;
  enum {
    CMD,
    OP,
    FUNC,
    SUBSHELL,
    REDIR,
  } type;
  union {
    struct { wf **args; char **sh_vars; } cmd;
    struct { token op_t; } op;
    struct { redir *redirs;} redir;
    struct { wf *name; } func;
  } t;
  int flags;
};


#define NEG (1 << 1)

#define CARGS(n)  ((n)->t.cmd.args)
#define CVARS(n)  ((n)->t.cmd.sh_vars)
#define COPP(n)   ((n)->t.op.op_t)
#define CREDR(n)    ((n)->t.redir.redirs)
#define CFUNC(n) ((n)->t.func.name)
// #define CBG(n)        ((n)->flags & BG) /* check if bg set */
#define CNEG(n)       ((n)->flags & NEG) /* check if NEGATE set */

extern int alias_depth;
extern int func_depth;
extern int notclosed;
extern sh_tok last_tok;
extern int chkwd;
#define SHTOK(t) ((sh_tok){ .type = t, .sub = 0 })
#define SHREDIR(s) ((sh_tok){ .type = TREDIR, .sub = (s) })
#define SHWORD(w) ((sh_tok) { .type = TWORD, .cmd = w, .sub = 0 })

#define CHKALIAS (1 << 0)
#define CHKNL (1 << 1)

#define RDIN 1
#define RDOUT 2
#define RDAPP 3
#define RDDUPO 4
#define RDDUPI 5
#define RDRW 6
#define RDCLOB 7

/** Get word fragment */
extern wf *get_wf(int);
/** create sh_toks out of line */
extern sh_tok tokenize(void);
void pushstring(char *, size_t, int);
void popstring(void);

/** build ast tree */
extern cmd_tree *parse_list(token s);
cmd_tree *parse_cmd(void);

static inline wf *
wfdup(wf *s)
{
  wf *n;

  if (!s)
    return NULL;
  n = malloc(sizeof(wf));
  n->word = s_strndup(s->word, s->len);
  n->len = s->len;
  n->qs = s->qs;
  n->next = wfdup(s->next);
  return n;
}

#endif /* LEX_H */
