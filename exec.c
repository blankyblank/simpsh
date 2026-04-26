/* exec.c - functions surrounding running external programs or builtins */
#include "simpsh.h"
#include "expand.h"
#include "exec.h"
#include "builtins.h"
#include "env.h"
#include "utils.h"
#include <err.h>
#include <sys/wait.h>

#define MAX_TMP_VARS 40

static int getbuiltin(char *);
static int builtin_launch(int, char **);
static int run_subsh(const cmd_tree *);
static int run_cmd(const cmd_tree *);
static int run_pipe(const cmd_tree *);
static int shexec(char **, char **);

typedef struct {
  char *name;
  char *var;
  shvar_flag oflags;
  int set;
} tmp_var;

/**  sh_env aware free  */
static inline void
free_env(char **env)
{
  char *buf = *((char **)env - 1);
  free(buf);
  free(env - 1);
}

/**  initialize builtin hash table  */
void
init_builtins(void)
{
  size_t i, n = builtinnum();
  size_t idx;

  for (i = 0; i < BUILTIN_BUCKETS; i++)
    builtin_tab[i] = -1;

  for (i = 0; i < n; i++) {
    idx = hash(builtins[i], BUILTIN_BUCKETS);
    while (builtin_tab[idx] >= 0)
      idx = (idx + 1) % BUILTIN_BUCKETS;
    builtin_tab[idx] = i;
  }
}

/**  get builtin command  */
int
getbuiltin(char *args)
{
  size_t idx;
  idx = hash(args, BUILTIN_BUCKETS);
  for (; builtin_tab[idx] >= 0; idx = (idx + 1) % BUILTIN_BUCKETS)
    if (strcmp(builtins[builtin_tab[idx]], args) == 0)
      return builtin_tab[idx];
  return -1;
}

/** launch builtin */
int
builtin_launch(int idx, char **args)
{
  return (*builtin_funcs[idx])(args);

  fprintf(stderr, "simpsh: builtins you failed me!");
  return 1;
}

/** fork and exec external command */
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
  switch (pid) {
  case -1:
    perror("failed to create");
    goto done;
  case 0:
    /* if fork was successful run the command */
    if (execve(fullpath, args, env) == -1) {
      perror(args[0]);
      goto done;
    }
    /* fall through */
  default:
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
}

int
run_cmd(const cmd_tree *n)
{
  int status;
  char **final = NULL;
  char **env = NULL;
  shvar *v;
  int i, b, vc = 0;

  final = expand_argv(n->args);
  if (!final || !final[0]) {
    /*  if no command only name=value  */
    if (n->sh_vars && n->sh_vars[0]) {
      for (i = 0; n->sh_vars[i]; i++) {
        char *name;
        shvar_flag flags;
        name = st_getname(n->sh_vars[i]);
        v = find_var(name);
        if (v) {
          flags = v->flags;
        } else {
          flags.exported = 0;
          flags.readonly = 0;
        }
        setvar(n->sh_vars[i], flags);
      }
      return 0;
    } else {
      return 1;
    }
  }
  b = getbuiltin(*final);
  if (b >= 0) {
    /*  NOTE: handle name=value cmd  */
    if (n->sh_vars && n->sh_vars[0]) {
      tmp_var tmp_vars[MAX_TMP_VARS];
      for (i = 0; n->sh_vars[i]; i++) {
        char *name;
        name = st_getname(n->sh_vars[i]);
        v = find_var(name);
        if (v) {
          tmp_vars[vc].set = 1;
          tmp_vars[vc].name = st_strdup(name);
          tmp_vars[vc].var = st_strdup(v->var);
          tmp_vars[vc].oflags = v->flags;
          vc++;
        } else {
          tmp_vars[vc].set = 0;
          tmp_vars[vc].name = st_strdup(name);
          tmp_vars[vc].var = st_strdup(n->sh_vars[i]);
          vc++;
        }
        shvar_flag flags = {
          .exported = 0,
          .readonly = 0,
        };
        setvar(n->sh_vars[i], flags);
      }
      status = builtin_launch(b, final);
      for (i = 0; i < vc; i++) {
        if (tmp_vars[i].set)
          setvar(tmp_vars[i].var, tmp_vars[i].oflags);
        else
          unset_var(tmp_vars[i].name);
      }
    } else {
      status = builtin_launch(b, final);
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

int
run_subsh(const cmd_tree *n)
{
  int status, wstatus;
  pid_t pid;

  if (!n->left) {
    fprintf(stderr, "empty subshell\n");
    exit(1);
  }

  pid = fork();
  switch (pid) {
  case -1:
    perror("failed to create subshell");
    return 1;
  case 0:
    status = run_commands(n->left);
    exit(status);
  default:
    waitpid(pid, &wstatus, 0);
    status = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : 1;
    if (n->negate == TRUE)
      status = !status;
    return status;
  }
}

int
run_pipe(const cmd_tree *n)
{
  int status, lwstatus, rwstatus, lstatus, rstatus;
  int pipefd[2];
  pid_t lpid, rpid;

  if (pipe(pipefd) < 0)
    err(1, "pipe");
  lpid = fork();
  switch (lpid) {
  case -1:
    close(pipefd[0]);
    close(pipefd[1]);
    err(1, "fork");
  case 0:
    if (close(pipefd[0]) < 0)
      err(1, "close");
    if (dup2(pipefd[1], STDOUT_FILENO) < 0)
      err(1, "dup2");
    if (close(pipefd[1]) < 0)
      err(1, "close");
    lstatus = run_commands(n->left);
    _exit(lstatus);
  }

  rpid = fork();
  switch (rpid) {
  case -1:
    close(pipefd[0]);
    close(pipefd[1]);
    kill(lpid, SIGTERM);
    waitpid(lpid, NULL, 0);
    err(1, "fork");
  case 0:
    if (close(pipefd[1]) < 0)
      err(1, "close");
    if (dup2(pipefd[0], STDIN_FILENO) < 0)
      err(1, "dup2");
    if (close(pipefd[0]) < 0)
      err(1, "close");
    rstatus = run_commands(n->right);
    _exit(rstatus);
  }

  if (close(pipefd[0]) < 0)
    err(1, "close");
  if (close(pipefd[1]) < 0)
    err(1, "close");
  waitpid(lpid, &lwstatus, 0);
  waitpid(rpid, &rwstatus, 0);
  status = WIFEXITED(rwstatus) ? WEXITSTATUS(rwstatus) : 1;

  return status;
}

/**  run command tree  */
int
run_commands(const cmd_tree *n)
{
  if (!n)
    return 0;
  int l_status = 0;

  switch (n->type) {
  case CMD:
    return run_cmd(n);
  case SUBSHELL:
    return run_subsh(n);
  case OP:
    if (n->op_t != TPIPE)
      l_status = run_commands(n->left);
    switch (n->op_t) {
    case TSEMI:
      return run_commands(n->right);
    case TAND:
      if (l_status != 0)
        return l_status;
      return run_commands(n->right);
    case TOR:
      if (l_status == 0)
        return l_status;
      return run_commands(n->right);
    case TPIPE:
        return run_pipe(n);
    case TEOF:
      return l_status;
    default:
      fprintf(stderr, "Unknown Operator\n");
      return 1;
    }
  }
  return 0;
}
