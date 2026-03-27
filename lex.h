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

/* store torkens before building argv */
typedef struct {
  char *cmd;
  token type;
} sh_tok;

/* tree struct to use for command parsing */
typedef struct cmd_tree cmd_tree;
struct cmd_tree {
  enum {
    CMD,
    OP
  } type;
  char **args;
  char **sh_vars;
  cmd_false negate;
  token op_t;
  cmd_tree *left;
  cmd_tree *right;
};

/* new parsing */
extern char *get_word(char *, size_t *, int *);
extern sh_tok *tokenize(char *, int *);
extern cmd_tree *build_tree(const sh_tok *, size_t);
extern int run_commands(const cmd_tree *);
extern char *expand_alias(char *);
extern char *exp_var(char *, size_t *, size_t *);

static inline cmd_tree *
newcmdnode(char **args, cmd_false negate) {
  cmd_tree *ct = malloc(sizeof(cmd_tree));
  if (!ct) {
    perror("malloc failed");
    return NULL;
  }

  ct->type = CMD;
  ct->args = args;
  ct->negate = negate;

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
    perror("malloc failed");
    return NULL;
  }

  ot->type = OP;
  ot->op_t = opp_t;
  ot->left = left;
  ot->right = right;

  /* opp tree doesn't have a command stored and doesn't use the ! opperator */
  ot->args = NULL;
  ot->negate = 0;

  return ot;
}

static inline void
freetoks(sh_tok *toks, int c) {
  int i;
  for (i = 0; i < c; i++) {
    if (toks[i].cmd != NULL)
      free(toks[i].cmd);
  }
  free(toks);
}

static inline void
freectree(cmd_tree *cmd_tree) {
  if (!cmd_tree)
    return;
  if (cmd_tree->left != NULL)
    freectree(cmd_tree->left);
  if (cmd_tree->right != NULL)
    freectree(cmd_tree->right);
  if (cmd_tree->type == CMD && cmd_tree->args != NULL)
    freeptr(cmd_tree->args);
  free(cmd_tree);
}

// vim: set filetype=c:
#endif /* LEX_H */
