/* lex.h - tokenizer and parser functions and declarations */
#ifndef LEX_H
#define LEX_H

#define _POSIX_C_SOURCE 200809L

#include <stddef.h>

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
  TLPAREN,
  TRPAREN,
  TBKGRND,
} token;

typedef enum {
  FALSE,
  TRUE,
} cmd_false;

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
} sh_tok;

#define SHTOK(t) ((sh_tok){ .type = t})
#define SHWORD(w) ((sh_tok){ .type = TWORD, .cmd = w})

typedef struct strpush strpush;
struct strpush {
    strpush *prev;
    char *saved_nchar;
    size_t saved_nleft;
    int saved_unget;
    char saved_ungetbuf[4];
    int alias;
};

/**
 * AST node for commands
 * @field node left
 * @field node right
 * @field enum node type
 * @field word fragment array args
 * @field cmd_false negate
 * @field token op_t
 */
typedef struct cmd_tree cmd_tree;
struct cmd_tree {
  cmd_tree *left;
  cmd_tree *right;
  enum {
    CMD,
    OP,
    SUBSHELL,
  } type;
  wf **args;
  char **sh_vars;
  cmd_false negate;
  token op_t;
};

extern int alias_depth;
extern int notclosed;
extern sh_tok last_tok;
extern int chkwd;
#define CHKALIAS (1<<0)
#define CHKNL (1<<1)

/** Get word fragment */
extern wf *get_wf(int);
/** create sh_toks out of line */
extern sh_tok tokenize(void);
void pushstring(char *, size_t, int);
void popstring(void);

/** build ast tree */
extern cmd_tree *parse_list(token s);
cmd_tree *parse_cmd(void);

#endif /* LEX_H */
