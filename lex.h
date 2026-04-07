#ifndef LEX_H
#define LEX_H

#include "utils.h"

#define BUF_S 64

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
/* refactoring to use word fragments from tokenization */
typedef struct wf wf;
struct wf {
  char *word;
  quoted qs;
  wf *next;
};

/* store torkens before building argv */
typedef struct {
  wf *cmd;
  token type;
} sh_tok;

/* tree struct to use for command parsing */
typedef struct cmd_tree cmd_tree;
struct cmd_tree {
  enum {
    CMD,
    OP
  } type;
  wf **args;
  char **sh_vars;
  cmd_false negate;
  token op_t;
  cmd_tree *left;
  cmd_tree *right;
};

/* new parsing */
extern wf *get_wf(char *, size_t *);
extern sh_tok *tokenize(char *, int *);
extern cmd_tree *build_tree(const sh_tok *, size_t);
extern char *expand_alias(char *);

static inline cmd_tree *
newcmdnode(wf **args, cmd_false negate, char **sh_vars) {
  cmd_tree *ct = malloc(sizeof(cmd_tree));
  if (!ct) {
    perror("malloc failed");
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
newoppnode(token opp_t, cmd_tree *left, cmd_tree *right) {
  cmd_tree *ot = malloc(sizeof(cmd_tree));
  if (!ot) {
    fprintf(stderr, "malloc failed");
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
freewf(wf *f) {
  wf *t;
  while (f) {
    t = f->next;
    free(f->word);
    free(f);
    f = t;
  }
}

static inline void
free_argv(wf **args) {
  if (!args)
    return;
  for (int i = 0; args[i]; i++) {
    wf *f = args[i];
    freewf(f);
  }
  free(args);
}

static inline void
freetoks(sh_tok *toks, int c) {
  int i;
  for (i = 0; i < c; i++) {
    toks[i].cmd = NULL;
  }
  free(toks);
}

static inline void
freectree(cmd_tree *cmd_tree) {
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
