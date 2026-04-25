/* lex.h - tokenizer and parser functions and declarations */
#ifndef LEX_H
#define LEX_H

#include <stddef.h>

#define BUF_S 32

typedef enum {
  TNOT,
  TPIPE,
  TWORD,
  TAND,
  TOR,
  TSEMI,
  TEOF,
  TLPAREN,
  TRPAREN,
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

typedef struct wf wf;
/**
 * word fragment
 * @field wf pointer to next node
 * @field string word
 * @field enum quoted qs
 */
struct wf {
  wf *next;
  char *word;
  quoted qs;
};

/** store tokens before building argv */
typedef struct {
  wf *cmd;
  token type;
} sh_tok;

typedef struct cmd_tree cmd_tree;
/**
 * AST node for commands
 * @field node left
 * @field node right
 * @field enum node type
 * @field word fragment array args
 * @field cmd_false negate
 * @field token op_t
 */
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

/** Get word fragment */
extern wf *get_wf(char *, size_t *);
/** create sh_toks out of line */
extern sh_tok *tokenize(char *, int *);
/** build ast tree */
extern cmd_tree *build_tree(const sh_tok *, size_t, token);
cmd_tree *parse_cmd(const sh_tok *, size_t, token, size_t *);

#endif /* LEX_H */
