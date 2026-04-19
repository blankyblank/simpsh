/* lex.h - tokenizer and parser functions and declarations */
#ifndef LEX_H
#define LEX_H

#include "utils.h"
#include "malloc.h"

#define BUF_S 32

typedef enum {
  TNOT,
  TWORD,
  TAND,
  TOR,
  TSEMI,
  TEOF,
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
    OP
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
extern cmd_tree *build_tree(const sh_tok *, size_t);

static inline cmd_tree *
newcmdnode(wf **args, cmd_false negate, char **sh_vars)
{
  cmd_tree *ct = st_alloc(sizeof(cmd_tree));
  if (!ct) {
    perror("st_alloc failed");
    return NULL;
  }
  ct->type = CMD;
  ct->args = args;
  ct->negate = negate;
  ct->sh_vars = sh_vars;
  /* cmd tree has no children, and doesn't have an operator in it */
  ct->left = NULL;
  ct->right = NULL;
  ct->op_t = 0;

  return ct;
}

static inline cmd_tree *
newoppnode(token opp_t, cmd_tree *left, cmd_tree *right)
{
  cmd_tree *ot = st_alloc(sizeof(cmd_tree));
  if (!ot) {
    fprintf(stderr, "st_alloc failed");
    return NULL;
  }
  ot->type = OP;
  ot->op_t = opp_t;
  ot->left = left;
  ot->right = right;
  /* opp tree doesn't have a command stored and doesn't use the ! opperator */
  ot->sh_vars = NULL;
  ot->args = NULL;
  ot->negate = 0;

  return ot;
}

static inline void
freewf(void)
{
}

static inline void
free_argv(wf **args)
{
  if (!args)
    return;
  for (int i = 0; args[i]; i++) {
    freewf();
  }
  free(args);
}

static inline void
freetoks(sh_tok *toks, int c)
{
  int i;
  for (i = 0; i < c; i++) {
    toks[i].cmd = NULL;
  }
  free(toks);
}

static inline void
freectree(cmd_tree *cmd_tree)
{
  if (!cmd_tree)
    return;
  if (cmd_tree->left)
    freectree(cmd_tree->left);
  if (cmd_tree->right)
    freectree(cmd_tree->right);
  if (cmd_tree->type == CMD) {
    if (cmd_tree->args)
      free_argv(cmd_tree->args);
    if (cmd_tree->sh_vars)
      freeptr(cmd_tree->sh_vars);
  }
  free(cmd_tree);
}

/* vim: set filetype=c: */
#endif /* LEX_H */
