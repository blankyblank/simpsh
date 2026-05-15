/* exec.c - functions surrounding running external programs or builtins */
#define _POSIX_C_SOURCE 200809L
#include <err.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "builtins.h"
#include "env.h"
#include "exec.h"
#include "expand.h"
#include "main.h"
#include "utils.h"

#define MAX_TMP_VARS 40
#define builtin_launch(i,a) ((*builtin_funcs[i])(a))

static int getbuiltin(char *);
static int run_subsh(const cmd_tree *);
static int run_cmd(const cmd_tree *);
static int run_pipe(const cmd_tree *);
static int shexec(char **, char **);

typedef struct {
  char *name;
  char *val;
  shvar_flags oldflags;
  int set;
} tmp_var;

/**  initialize builtin hash table  */
void
init_builtins(void)
{
  size_t i, n;
  size_t idx;

  n = builtinnum();
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
    if (builtins[builtin_tab[idx]][0] == args[0] &&
        strcmp(builtins[builtin_tab[idx]], args) == 0)
      return builtin_tab[idx];
  return -1;
}

/** get full path to executable */
char *
getpath(char *file)
{
  char *fullpath;

  if ((strchr(file, '/')) && access(file, X_OK) == 0)
    return (st_strdup(file));

  const char *path = getvar("PATH");
  if (path)
    fullpath = chkpath(path, file, 0);
  else
    fullpath = chkpath(defpath, file, 0);

  if (!fullpath)
    return NULL;
  return fullpath;
}

/** check in path for name stop at the first one with proper permissions if cdmode 1 check for dir */
char *
chkpath(const char *path, const char *name, unsigned int cdmode)
{
  size_t flen, seg, tlen, blen;
  char *end, *e, *exp, *bdir;
  char buf[PATH_MAX], expbuf[PATH_MAX], tildbuf[PATH_MAX];
  unsigned int noex;
  const char *s;
  struct stat statbuf;

  flen = strlen(name);
  s = path;

  for (e = s_strchrnul(s, ':'); e; e = s_strchrnul(s, ':')) {
    size_t dirlen, complen;
    char *comp;

    dirlen = e - s;
    comp = (char *)s;
    complen = dirlen;

    noex = 0;
    if (s[0] == '~') {
      seg = 0;
      while (s[seg] != '/' && seg < dirlen)
        seg++;

      if (seg == 1 || (seg == dirlen && dirlen == 1)) {
        exp = getvar("HOME");
        if (exp) {
          blen = strlen(exp);
          bdir = exp;
        } else {
          noex = 1;
        }
      } else {
        nmemcpy(tildbuf, s + 1, seg - 1);
        exp = homedir(tildbuf);
        if (exp) {
          blen = strlen(exp);
          bdir = exp;
        } else {
          noex = 1;
        }
      }
      if (!noex) {
        memcpy(expbuf, bdir, blen);
        if (seg < dirlen)
          memcpy(expbuf + blen, s + seg, dirlen - seg);
        tlen = blen + (seg < dirlen ? dirlen - seg : 0);
        comp = expbuf;
        complen = tlen;
      }
    }

    end = s_mempcpy(buf, comp, complen); /* copy current path segment from s to e */
    *end++ = '/';                                /* add / and then file to the end of it */
    memcpy(end, name, flen + 1);
    if (access(buf, X_OK) == 0) {
      if (cdmode) {
        if (!stat(buf, &statbuf) && S_ISDIR(statbuf.st_mode))
          return st_strdup(buf);
      } else {
        return st_strdup(buf);
      }
    }
    if (!*e)
      break;
    s = e + 1;
  }
  return NULL;
}

/** fork and exec external command */
int
shexec(char **args, char **env)
{
  int wstatus;
  pid_t pid;
  char *fullpath;

  /* test if the command has a /, if not return the first executable
     on PATH with the command name given */
  fullpath = getpath(args[0]);
  if (!fullpath) {
    fprintf(stderr, "%s: %s: command not found\n", sh_argv0, args[0]);
    return 1;
  }

  pid = fork();
  switch (pid) {
    case -1:
      perror("failed to create");
      return 1;
    case 0:
      /* if fork was successful run the command */
      if (execve(fullpath, args, env) < 0) {
        perror(args[0]);
        _exit(1);
      }
    /* fall through */
    default:
      waitpid(pid, &wstatus, 0);
      return WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : 1;
  }
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
        char *name, *val;
        shvar_flags flags;
        st_read_assn(n->sh_vars[i], &name, &val);
        v = find_var(name);
        if (v) 
          flags = v->flags;
        else
          flags = 0;
        setvar(name, val, flags);
      }
      return 0;
    } else {
      return 1;
    }
  }

  b = getbuiltin(*final);
  if (b >= 0) {
    /*  handle name=value cmd  */
    if (n->sh_vars && n->sh_vars[0]) {
      tmp_var tmp_vars[MAX_TMP_VARS];
      for (i = 0; n->sh_vars[i]; i++) {
        char *name, *val;
        st_read_assn(n->sh_vars[i], &name, &val);
        v = find_var(name);
        if (v) {
          tmp_vars[vc].set = 1;
          tmp_vars[vc].name = st_strdup(name);
          tmp_vars[vc].val = st_strdup(shvar_val(v));
          tmp_vars[vc].oldflags = v->flags;
          vc++;
        } else {
          tmp_vars[vc].set = 0;
          tmp_vars[vc].name = st_strdup(name);
          tmp_vars[vc].val = NULL;
          vc++;
        }
        setvar(n->sh_vars[i], val, 0);
      }
      status = builtin_launch(b, final);
      for (i = 0; i < vc; i++) {
        if (tmp_vars[i].set)
          setvar(tmp_vars[i].name, tmp_vars[i].val, tmp_vars[i].oldflags);
        else
          unset_var(tmp_vars[i].name);
      }

    } else {
      status = builtin_launch(b, final);
    }
  } else {
    env = build_env(n->sh_vars);
    if (!env) {
      perror("idk build_env failed I guess");
      return 1;
    }
    status = shexec(final, env);
    free(env);
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
  int status, lw_status, rw_status, l_status, r_status;
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
      l_status = run_commands(n->left);
      _exit(l_status);
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
      r_status = run_commands(n->right);
      _exit(r_status);
  }

  if (close(pipefd[0]) < 0)
    err(1, "close");
  if (close(pipefd[1]) < 0)
    err(1, "close");
  waitpid(lpid, &lw_status, 0);
  waitpid(rpid, &rw_status, 0);
  status = WIFEXITED(rw_status) ? WEXITSTATUS(rw_status) : 1;

  return status;
}

/**  run command tree  */
int
run_commands(const cmd_tree *n)
{
  if (!n)
    return 0;

  switch (n->type) {
    case CMD:
      return lstatus = run_cmd(n);
    case SUBSHELL:
      return lstatus = run_subsh(n);
    case OP:
      if (n->op_t != TPIPE)
        lstatus = run_commands(n->left);
      switch (n->op_t) {
        case TSEMI:
        case TNL:
          return lstatus = run_commands(n->right);
        case TAND:
          if (lstatus != 0)
            return lstatus;
          return lstatus = run_commands(n->right);
        case TOR:
          if (lstatus == 0)
            return lstatus;
          return lstatus = run_commands(n->right);
        case TPIPE:
          return lstatus = run_pipe(n);
        case TEOF:
          return lstatus;
        default:
          fprintf(stderr, "Unknown Operator\n");
          return 1;
      }
  }
  return 0;
}
