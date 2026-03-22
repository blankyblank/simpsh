// vim: set filetype=c:
#ifndef PARSER_H
#define PARSER_H

#include "malloc.h"

#define BUF_S 64

// NOTE: need to decide how to structure header after tok.c is done
typedef enum {
  TWORD,
  TAND,
  TOR,
  TSEMI,
  TEOF,
} token;

typedef enum {
  QNONE,
  QSINGLE,
  QDOUBLE,
} quoted;

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
  int c_false;
  token op_t;
  cmd_tree *left;
  cmd_tree *right;
};

/* new parsing */
char *get_word(char *, size_t *);
sh_tok *tokenize(char *, int *);

/* old parser scan input at least is being removed */
extern int scan_input(char *, sh_tok *, int *);
extern token chk_op(char *);
extern cmd_tree *build_tree(sh_tok *, size_t);
extern int run_commands(cmd_tree *);

static inline cmd_tree *
newcmdnode(char **args, int c_false) {
  cmd_tree *ct = malloc(sizeof(cmd_tree));
  if (!ct) {
    perror("malloc failed");
    return NULL;
  }

  ct->type = CMD;
  ct->args = args;
  ct->c_false = c_false;

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
  ot->c_false = 0;

  return ot;
}

static inline void
freetoks(sh_tok *toks, int c) {
  int i;
  for (i = 0; i < c; i++) {
    if (toks[i].cmd != NULL)
      free(toks[i].cmd);
  }
}

static inline void
freectree(cmd_tree *cmd_tree) {
  if (cmd_tree == NULL)
    return;

  if (cmd_tree->left != NULL) {
    freectree(cmd_tree->left);
  }
  if (cmd_tree->right != NULL) {
    freectree(cmd_tree->right);
  }
  if (cmd_tree->type == CMD && cmd_tree->args != NULL) {
    freeptr(cmd_tree->args);
  }

  free(cmd_tree);
}
#endif /* PARSER_H */
