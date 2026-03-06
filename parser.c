#include "simpsh.h"
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

/* cmd_tok toks[MAX_CMDS]; */

int
scan_input(char *line, cmd_tok *toks[MAX_CMDS], int *cnt) {
  int success = 0, err = -1;
  int i, start = 0;
  cnt = 0;

  for (i = 0; i < strlen(line); i++) {
    if (line[i] == '&' && i + 1 < strlen(line) && line[i + 1] == '&') {
      toks[cnt].opp = AND;
      toks[cnt].cmd = strndup(&line[start], i - start);
      cnt++;
      i++;
    } else if (line[i] == '|' && i + 1 < strlen(line) && line[i + 1] == '|') {
      toks[cnt].opp = OR;
      toks[cnt].cmd = strndup(&line[start], i - start);
      cnt++;
      i++;
    } else if (line[i] == ';') {
      toks[cnt].opp = SEMICOLON;
      toks[cnt].cmd = strndup(&line[start], i - start);
      cnt++;
      i++;
    }

    if (line[i] != '\0') {
      toks[cnt].opp = 0;
      toks[cnt].cmd = strndup(&line[start], strlen(line) - start);
    }
  }

  return success;
}
