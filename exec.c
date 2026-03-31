#include "simpsh.h"
#include "lex.h"
#include "expand.h"
#include "exec.h"
#include "builtins.h"
#include "env.h"

int
getbuiltin(char **args) {
  /* check if string matches builtin command */
  int n = builtinnum();
  int i;

  for (i = 0; i < n; i++) {
    if (!strcmp(args[0], builtins[i])) {
      return 1;
    }
  }
  return 0;
}

int
builtin_launch(char **args) {
  /* launch builtin */
  int n = builtinnum();
  int i;
  for (i = 0; i < n; i++) {
    if (!strcmp(*args, builtins[i])) {
      return (*builtin_funcs[i])(args);
    }
  }
  printf("something didn't work");
  return 1;
}

int
run_commands(const cmd_tree *n) {
  int l_status, r_status, status;
  char **final = NULL;
  char *name, *val;
  int i;

  if (!n)
    return 0;

  if (n->sh_vars && (!n->args || !n->args[0])) {
    for (i = 0; n->sh_vars[i]; i++) {
      read_assn(n->sh_vars[i], &name, &val);
      setvar(name, val);
    }
    free(name);
    free(val);
    return 0;
  }

  if (n->type == CMD) {
    final = expand_argv(n->args);
    if (!final)
      return 1;
    if (getbuiltin(final) == 1)
      status = builtin_launch(final);
    else
      status = shexec(final);

    if (n->negate == TRUE)
      status = !status;
    freeptr(final);
    return status;
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
} /* run the commands previously built into the tree  */

int
shexec(char **args) {
  int wstatus, estatus;
  pid_t pid;
  char *fullpath;

  /* test if the command has a /, if not return the first executable
     on PATH with the command name given */
  fullpath = getpath(&args[0]);
  if (!fullpath) {
    fprintf(stderr, "%s: %s: command not found\n", sh_argv0, args[0]);
    goto done;
  }

  pid = fork();
  if (pid == -1) {
    perror("failed to create");
    goto done;
  }

  if (pid == 0) { /* if fork was successful run the command */
    if (execve(fullpath, args, environ) == -1) {
      perror(args[0]);
      goto done;
    }
  } else {
    waitpid(-1, &wstatus, 0);
    estatus = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : 1;
    free(fullpath);
    return estatus;
  }

  goto done;

done:
  if (fullpath)
    free(fullpath);
  estatus = 1;
  return estatus;
} /* fork and exec the command passed to the shell */
