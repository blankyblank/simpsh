#include "simpsh.h"
#include "parser.h"


char **
get_argv(sh_tok *tokens, size_t cnt, size_t *i) {
  char **argv = malloc(MAX_CMDS * sizeof(char *));
  // char **argv = NULL;
  size_t t = *i, j = 0;
  
  while (t < cnt && tokens[t].type == TWORD) {
    argv[j] = strdup(tokens[t].cmd);
    if (!argv[j]) {
      freeptr(argv);
      return NULL;
    }
    j++;
    t++;
  }
  argv[j] = NULL;
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

  args = get_argv(tokens, cnt, &i);
  if (!args || !args[0]) {    /* handle empty input */
    fprintf(stderr, "somethings wrong \n");
    freeptr(args);
    return NULL;
  }

  r = newcmdnode(args, 0);
  // i = 1;

  while (i < cnt && tokens[i].type != TEOF) {
    t = tokens[i].type;

    if (t != TAND && t != TOR && t != TSEMI) {
      fprintf(stderr, "Expected Operator\n");
      return NULL;
    }
    i++; //move to next command

    // Must have a command after operator
    if (i >= cnt || tokens[i].type != TWORD)
      return NULL;
    // Get next command's arguments
    args = get_argv(tokens, cnt, &i);
          // args = getinput(tokens[i].cmd, " \n");
    if (!args || !args[0]) {
      freeptr(args);
      return NULL;
    }
    // fprintf(stderr, "Creating operator node with type: %d\n", t);
    n = newcmdnode(args, 0);
    r = newoppnode(t, r, n);
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
