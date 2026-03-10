#include "simpsh.h"

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

  /* cmd tree has no children, and doesn't have an operator in it */
  ct->left = NULL;
  ct->right = NULL;
  ct->op_t = 0;

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
  ot->op_t = opp_t;
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
      toks[*cnt].op = AND;
      toks[*cnt].cmd = strndup(&line[s], i - s);
      (*cnt)++;
      i += 2;
      s = i;
    } else if (line[i] == '|' && i + 1 < strlen(line) && line[i + 1] == '|') {
      toks[*cnt].op = OR;
      toks[*cnt].cmd = strndup(&line[s], i - s);
      (*cnt)++;
      i += 2;
      s = i;
    } else if (line[i] == ';') {
      toks[*cnt].op = SEMICOLON;
      toks[*cnt].cmd = strndup(&line[s], i - s);
      (*cnt)++;
      i++;
      s = i;
    }
  }

  if (s < strlen(line)) {
    toks[*cnt].op = NONE;
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
  if (args == NULL || args[0] == NULL) {
    fprintf(stderr, "unexpected operator\n");
    free(args);
    return NULL;
  }

  cmd_tree *left = newcmdnode(args, 0);

  if (i == cnt - 1) {
    return left;
  }

  cmd_tree *right = build_tree(tokens, cnt, i + 1);

  cmd_tree *root = newoppnode(c.op, left, right);
  return root;
}

int
run_commands(cmd_tree *n) {
  int l_status, r_status;

  if (n == NULL) {
    return 0;
  }

  if (n->type == CMD) {
    if (getbuiltin(n->args) == 1)
      return builtin_launch(n->args);
    else
      return shexec(n->args);
  }

  if (n->type == OP) {
    l_status = run_commands(n->left);

    switch (n->op_t) {
    case SEMICOLON:
      r_status = run_commands(n->right);
      return r_status;
    case AND:
      if (l_status != 0) {
        return l_status;
      }
      r_status = run_commands(n->right);
      return r_status;
    case OR:
      if (l_status == 0) {
        return l_status;
      }
      r_status = run_commands(n->right);
      return r_status;
    case NONE:
      return l_status;
    default:
      fprintf(stderr, "Unknown Operator\n");
      return 1;
    }
  }

  return 0;
}
