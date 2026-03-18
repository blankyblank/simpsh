#include "simpsh.h"
#include "parser.h"

int
scan_input(char *line, cmd_tok *toks, int *cnt) {
  unsigned int i, s = 0;
  *cnt = 0;

  /* check if input  is null, or empy */
  if (line == NULL)
    return -1;
  if (strlen(line) == 0)
    return 1;

  /* check for operators, save the type, extract the command
   * then increment the correct number of spaces to match their lenght */

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

  /* no operator */
  if (s < strlen(line)) {
    toks[*cnt].op = NONE;
    toks[*cnt].cmd = strndup(&line[s], strlen(line) - s);
    (*cnt)++;
  }

  return 0;
}

/* build command tree recursively */
cmd_tree *
build_tree(cmd_tok *tokens, int cnt, int i) {
  cmd_tok c;
  char **args;

  if (i > cnt) {
    return NULL;
  }

  c = tokens[i];
  args = getinput(c.cmd, " \n");

  /* handle empty input */
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

  if (n == NULL)
    return 0;

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
      if (l_status != 0)
        return l_status;
      r_status = run_commands(n->right);
      return r_status;
    case OR:
      if (l_status == 0)
        return l_status;
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
