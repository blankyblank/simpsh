#include "simpsh.h"
#include <readline/history.h>
#include <string.h>

#define MAX_CMDS 256

/* store commands before putting them in ast */
typedef struct {
  char *cmd;
  cntrl opp;
} cmd_tok;
/* tree struct to use for command parsing */
typedef struct cmd_tree cmd_tree;
struct cmd_tree {
  enum {
    CMD,
    OP
  } type;

  char **args;
  int c_false;
  cntrl opp_t;
  cmd_tree *left;
  cmd_tree *right;
};

cmd_tree *newcmdnode(char **, int);
cmd_tree *newoppnode(cntrl, cmd_tree *, cmd_tree *);
void freectree(cmd_tree *);
int scan_input(char *, cmd_tok *, int *);
cntrl chk_op(char *);
cmd_tree *build_tree(cmd_tok *, int, int);
int run_commands(cmd_tree *);

cmd_tree *
newcmdnode(char **args, int c_false) {
  cmd_tree *ct = malloc(sizeof(cmd_tree));
  if (!ct) {
    perror("malloc failed");
    return NULL;
  }

  ct->type = CMD;
  ct->args = args;
  ct->c_false = c_false;

  /* cmd tree has no children, and doesn't doesn't have an opperator in it */
  ct->left = NULL;
  ct->right = NULL;
  ct->opp_t = 0;

  return ct;
}

cmd_tree *
newoppnode(cntrl opp_t, cmd_tree *left, cmd_tree *right) {
  cmd_tree *ot = malloc(sizeof(cmd_tree));
  if (!ot) {
    perror("malloc failed");
    return NULL;
  }

  ot->type = OP;
  ot->opp_t = opp_t;
  ot->left = left;
  ot->right = right;

  /* opp tree doesn't have a command stored and doesn't use the ! opperator */
  ot->args = NULL;
  ot->c_false = 0;

  return ot;
}

void
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

int
scan_input(char *line, cmd_tok *toks, int *cnt) {
  unsigned int i, s = 0;
  *cnt = 0;

  if (line == NULL)
    return -1;
  if (strlen(line) == 0)
    return 1;

  for (i = 0; i < strlen(line); i++) {
    if (line[i] == '&' && i + 1 < strlen(line) && line[i + 1] == '&') {
      toks[*cnt].opp = AND;
      toks[*cnt].cmd = strndup(&line[s], i - s);
      (*cnt)++;
      i++;
    } else if (line[i] == '|' && i + 1 < strlen(line) && line[i + 1] == '|') {
      toks[*cnt].opp = OR;
      toks[*cnt].cmd = strndup(&line[s], i - s);
      (*cnt)++;
      i++;
    } else if (line[i] == ';') {
      toks[*cnt].opp = SEMICOLON;
      toks[*cnt].cmd = strndup(&line[s], i - s);
      (*cnt)++;
      i++;
    }
  }

  if (s < strlen(line)) {
    toks[*cnt].opp = NONE;
    toks[*cnt].cmd = strndup(&line[s], strlen(line) - s);
    (*cnt)++;
  }

  return 0;
}

cmd_tree *
build_tree(cmd_tok *tokens, int cnt, int i) {
  cmd_tok c;
  char **args;

  if (i > cnt) {
    return NULL;
  }

  c = tokens[i];
  args = getinput(c.cmd, " \n");
  cmd_tree *left = newcmdnode(args, 0);

  if (i == cnt - 1) {
    return left;
  }

  cmd_tree *right = build_tree(tokens, cnt, i + 1);

  cmd_tree *root = newoppnode(c.opp, left, right);
  return root;
}

int
run_commands(cmd_tree *n) {
  int l_status = -1, r_status = -1;
  int status;

  if (n == NULL) {
    return 0;
  }

  if (n->type == CMD) {
    if (getbuiltin(n->args) == 1) {
      status = builtin_launch(n->args);
    } else {
      status = shexec(n->args);
    }
  }

  if (n->type == OP) {
    l_status = run_commands(n->left);
    r_status = run_commands(n->right);
  }

  if (r_status > -1)
    return r_status;
  else
    return l_status;

  /* note to self need to work out the logic on how to handle exist status, and
     what the return value on this function should actually be still
  */
}
