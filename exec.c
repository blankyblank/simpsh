/* exec.c - functions surrounding running external programs or builtins */
#include "simpsh.h"
#include "lex.h"
#include "expand.h"
#include "exec.h"
#include "builtins.h"
#include "env.h"

#define MAX_TMP_VARS 40

typedef struct {
  char *name;
  char *oval;
  shvar_flag oflags;
  int set;
} tmp_var;

/**sh_env aware free*/
static inline void
free_env(char **env)
{
  char *buf = *((char **)env - 1);
  free(buf);
  free(env - 1);
}

int
getbuiltin(char **args)
{
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
builtin_launch(char **args)
{
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
run_commands(const cmd_tree *n)
{
  int l_status, r_status, status;
  char *name, *val;
  char **final = NULL;
  char **env = NULL;
  shvar *v;
  int i, vc = 0;

  if (!n)
    return 0;

  if (n->type == CMD) {
    final = expand_argv(n->args);
    if (!final || !final[0]) {
      /*  if no command only name=value  */
      if (n->sh_vars && n->sh_vars[0]) {
        for (i = 0; n->sh_vars[i]; i++) {
          read_assn_stack(n->sh_vars[i], &name, &val);
          shvar_flag flags = {
            .exported = 0,
            .readonly = 0,
            .null = (val[0] == '\0'),
          };
          setvar(name, val, flags);
        }
        return 0;
      } else {
        return 1;
      }
    }

    if (getbuiltin(final) == 1) {
      /*  handle name=value cmd  */
      if (n->sh_vars && n->sh_vars[0]) {
        tmp_var tmp_vars[MAX_TMP_VARS];
        for (i = 0; n->sh_vars[i]; i++) {
          read_assn_stack(n->sh_vars[i], &name, &val);
          v = find_var(name);
          if (v) {
            tmp_vars[vc].set = 1;
            tmp_vars[vc].name = v->name;
            tmp_vars[vc].oval = st_strdup(v->value);
            tmp_vars[vc].oflags = v->flags;
            vc++;
          } else {
            tmp_vars[vc].set = 0;
            tmp_vars[vc].name = st_strdup(name);
            vc++;
          }
          shvar_flag flags = {
            .exported = 0,
            .readonly = 0,
            .null = (val[0] == '\0'),
          };
          setvar(name, val, flags);
        }
        status = builtin_launch(final);
        for (i = 0; i < vc; i++) {
          if (tmp_vars[i].set)
            setvar(tmp_vars[i].name, tmp_vars[i].oval, tmp_vars[i].oflags);
          else
            unset_var(tmp_vars[i].name);
        }
      } else {
        status = builtin_launch(final);
      }
    } else {
      env = build_env(n->sh_vars);
      status = shexec(final, env);
      free_env(env);
    }
    if (n->negate == TRUE)
      status = !status;
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
shexec(char **args, char **env)
{
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
    if (execve(fullpath, args, env) == -1) {
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
