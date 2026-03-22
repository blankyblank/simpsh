#include "simpsh.h"
#include "parser.h"


char **
get_argv(sh_tok *tokens, int cnt, size_t *i) {
  // char **argv = malloc(MAX_CMDS * sizeof(char *));
  char **argv = NULL;
  size_t t = *i, j = 0;
  
  while (tokens[t].type == TWORD) {
    if (tokens[t].type == TAND || tokens[t].type == TOR || tokens[t].type == TSEMI || tokens[t].type == TEOF)
      break;
    if (tokens[t].type == TWORD) {
      argv[j] = strdup(tokens[t].cmd);
      j++;
    }
    t++;
  }
  
  *i = t;
  return argv;
}

/* build command tree recursively */
cmd_tree *
build_tree(sh_tok *tokens, size_t cnt) {
  size_t i = 0;
  char **args = NULL;
  cmd_tree *n, *r;
  token t;

  if (cnt < 1 || tokens[i].type != TWORD)
    return NULL;

  args = get_argv(&tokens[i], cnt, &i);
  // args = getinput(tokens[i].cmd, " \n");

  if (!args || !args[0]) {    /* handle empty input */
    fprintf(stderr, "unexpected operator\n");
    free(args);
    return NULL;
  }

  r = newcmdnode(args, 0);
  i = 1;

  while (i < cnt && tokens[i].type != TEOF) {
    t = tokens[i].type;
    i++;

    if (i >= cnt || tokens[i].type != TWORD)
      return NULL;

    args = getinput(tokens[i].cmd, " \n");
    if (!args || !args[0]) {
      freeptr(args);
      return NULL;
    }
    n = newcmdnode(args, 0);
    r = newoppnode(tokens[i - 1].type, r, n);
    i++;
  }
  return r;
}

int
run_commands(cmd_tree *n) {
  int l_status, r_status;

  if (!n)
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
    case TSEMI:
      r_status = run_commands(n->right);
      return r_status;
    case TAND:
      if (l_status != 0)
        return l_status;
      r_status = run_commands(n->right);
      return r_status;
    case TOR:
      if (l_status == 0)
        return l_status;
      r_status = run_commands(n->right);
      return r_status;
    case TEOF:
      return l_status;
    default:
      fprintf(stderr, "Unknown Operator\n");
      return 1;
    }
  }

  return 0;
}
