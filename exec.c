/* exec.c - functions surrounding running external programs or builtins */
#define _POSIX_C_SOURCE 200809L
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "env.h"
#include "exec.h"
#include "expand.h"
#include "job.h"
#include "lex.h"
#include "main.h"
#include "malloc.h"
#include "sig.h"
#include "utils.h"

#define MAX_TMP_VARS 40
#define builtin_launch(i, a) ((*builtin_funcs[i])(a))

typedef struct tmp_var {
  char *name;
  char *val;
  shvar_flags oldflags;
  int set;
} tmp_var;

typedef struct fdlist {
  int orig;
  int saved;
} fdlist;

static int getbuiltin(char *);
static void poptmpvars(tmp_var *, size_t);
static int pushtmpvar(char **, tmp_var *);
static int restore_fd(fdlist *, size_t);
static void save_fd(redir *, fdlist *, size_t *);
static char *bg_cmd(const cmd_tree *);
static int shexec(char **, char **);
static int run_funcdef(const cmd_tree *);
static int run_func(const cmd_tree *, char **);
static int run_pipe(const cmd_tree *);
static int run_bg(const cmd_tree *);
static int run_redir(const cmd_tree *);
static int run_subsh(const cmd_tree *);
static int run_cmd(const cmd_tree *);


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

static int
pushtmpvar(char **shvars, tmp_var *tmp)
{
  shvar *v;
  size_t i, vc;

  vc = 0;
  for (i = 0; shvars[i]; i++) {
    char *name, *val;
    st_read_assn(shvars[i], &name, &val);
    v = findvar(name);
    if (v) {
      tmp[vc].set = 1;
      tmp[vc].name = st_strdup(name);
      tmp[vc].val = st_strdup(shvar_val(v));
      tmp[vc].oldflags = v->flags;
      vc++;
    } else {
      tmp[vc].set = 0;
      tmp[vc].name = st_strdup(name);
      tmp[vc].val = NULL;
      vc++;
    }
    setvar(shvars[i], val, 0);
  }
  return vc;
}

static void
poptmpvars(tmp_var *tmp, size_t vc)
{
  for (size_t i = 0; i < vc; i++) {
    if (tmp[i].set)
      setvar(tmp[i].name, tmp[i].val, tmp[i].oldflags);
    else
      rmvar(tmp[i].name);
  }
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

char *
tildepath(const char *s, size_t dirlen, size_t *seg)
{
  size_t lseg;
  char *hm;

  lseg = 1;
  while (s[lseg] != '/' && lseg < dirlen)
    lseg++;

  if (lseg == 1 || (lseg == dirlen && dirlen == 1)) {
    hm = getvar("HOME");
  } else {
    char tildbuf[PATH_MAX];
    nmemcpy(tildbuf, s + 1, lseg - 1);
    hm = homedir(tildbuf);
  }
  *seg = lseg;
  return hm;
}

/** check in path for name stop at the first one with proper permissions if
 * cdmode 1 check for dir */
char *
chkpath(const char *path, const char *name, unsigned int cdmode)
{
  size_t flen, seg;
  char *e, buf[PATH_MAX], expbuf[PATH_MAX];
  const char *s;
  struct stat statbuf;

  flen = strlen(name);
  s = path;

  for (e = s_strchrnul(s, ':'); e; e = s_strchrnul(s, ':')) {
    size_t dirlen, complen;
    char *end, *comp;

    dirlen = e - s;
    comp = (char *)s;
    complen = dirlen;
    if (s[0] == '~') {
      char  *hm;
      seg = 0;
      hm = tildepath(s, dirlen, &seg);
      if (hm) {
        size_t hmlen;
        hmlen = strlen(hm);
        memcpy(expbuf, hm, hmlen);
        if (seg < dirlen)
          memcpy(expbuf + hmlen, s + seg, dirlen - seg);
        comp = expbuf;
        complen = hmlen + (seg < dirlen ? dirlen - seg : 0);
      }
    }

    end = s_mempcpy(buf, comp, complen); /* copy current path segment from s to e */
    *end++ = '/';                    /* add / and then file to the end of it */
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
  int wstatus, jc;
  pid_t pid;
  sigset_t sigold;
  char *fullpath;

  /* test if the command has a /, if not return the first executable on PATH with the command name given */
  fullpath = getpath(args[0]);
  if (!fullpath) {
    fprintf(stderr, "%s: %s: command not found\n", sh_argv0, args[0]);
    return 1;
  }

  jc = 0;
  jobsig_blk(&sigold);
  pid = fork();
  switch (pid) {
    case -1:
      perror("failed to create");
      jobsig_unblk(&sigold);
      return 1;
    case 0:
      /* if fork was successful run the command */
      signal(SIGINT, SIG_DFL);
      signal(SIGQUIT, SIG_DFL);
      chld_setpgid(0);
      if (execve(fullpath, args, env) < 0) {
        perror(args[0]);
        _exit(1);
      }
    /* fall through */
    default:
      setpgid(pid, pid);
      if (getpid() == sh_pgid) {
        jc = 1;
        startjob(pid);
      }
      jobsig_unblk(&sigold);
      waitpid(pid, &wstatus, 0);
      if (jc)
        endjob();
      return WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : 1;
  }
}

static inline int
run_funcdef(const cmd_tree *n)
{
  char *name;
  name = st_strndup(CFUNC(n)->word, CFUNC(n)->len);
  setfunc(name, n->left);
  return 0;
}

static int
run_func(const cmd_tree *n, char **args)
{
  char **oldargv;
  int oldargc;
  int status;

  oldargc = sh_argc;
  oldargv = sh_argv;
  sh_argc = array_len(args) - 1;
  sh_argv = args + 1;

  if (func_depth >= MAX_FUNC_DEPTH) {
    fprintf(stderr, "function: too many levels of recursion\n");
    sh_argv = oldargv;
    sh_argc = oldargc;
    return 1;
  }
  func_depth++;

  status = run_commands(n);
  sh_argv = oldargv;
  sh_argc = oldargc;
  func_depth--;
  return status;
}

static int
run_bg(const cmd_tree *n)
{
  pid_t pid;
  sigset_t sigold;
  job *j;
  int status;

  jobsig_blk(&sigold);
  pid = fork();
  switch (pid) {
    case -1:
      perror("failed to create subshell");
      jobsig_unblk(&sigold);
      return 1;
    case 0:
      setpgid(0, 0);
      signal(SIGINT, SIG_IGN);
      signal(SIGQUIT, SIG_IGN);
      jobsig_unblk(&sigold);
      status = run_commands(n->left);
      fflush(NULL);
      _exit(status);
    default:
      setpgid(pid, pid);
      j = newjob(pid, bg_cmd(n->left));
      jobsig_unblk(&sigold);
      printf("[%d] %d\n", j->num, pid);
      return run_commands(n->right);
  }
}

static int
run_subsh(const cmd_tree *n)
{
  int status, wstatus, jc;
  pid_t pid;
  sigset_t sigold;

  if (!n->left) {
    fprintf(stderr, "empty subshell\n");
    return 1;
  }

  jc = 0;
  jobsig_blk(&sigold);
  pid = fork();
  switch (pid) {
    case -1:
      perror("failed to create subshell");
      jobsig_unblk(&sigold);
      return 1;
    case 0:
      signal(SIGINT, SIG_DFL);
      signal(SIGQUIT, SIG_DFL);
      chld_setpgid(0);
      status = run_commands(n->left);
      fflush(NULL);
      _exit(status);
    default:
      setpgid(pid, pid);
      if (getpid() == sh_pgid) {
        jc = 1;
        startjob(pid);
      }
      jobsig_unblk(&sigold);
      waitpid(pid, &wstatus, 0);
      status = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : 1;
      if (CNEG(n))
        status = !status;
      if (jc)
        endjob();
      return status;
  }
}

static int
run_pipe(const cmd_tree *n)
{
  int status, lw_status, rw_status, l_status, r_status;
  int pipefd[2], outer;
  static int pipedepth;
  pid_t lpid, rpid;
  sigset_t sigold;

  pipedepth++;
  outer = (pipedepth == 1);
  jobsig_blk(&sigold);

  if (pipe(pipefd) < 0)
    err(1, "pipe");
  lpid = fork();
  switch (lpid) {
    case -1:
      CLOSEFD(pipefd[0]);
      CLOSEFD(pipefd[1]);
      jobsig_unblk(&sigold);
      err(1, "fork");
    case 0:
      signal(SIGINT, SIG_DFL);
      signal(SIGQUIT, SIG_DFL);
      chld_setpgid(0);
      CLOSEFD(pipefd[0]);
      DUPFD(pipefd[1], STDOUT_FILENO);
      CLOSEFD(pipefd[1]);
      l_status = run_commands(n->left);
      fflush(NULL);
      _exit(l_status);
    default:
      setpgid(lpid, lpid);
      break;
  }

  rpid = fork();
  switch (rpid) {
    case -1:
      CLOSEFD(pipefd[1]);
      kill(lpid, SIGTERM);
      waitpid(lpid, NULL, 0);
      jobsig_unblk(&sigold);
      err(1, "fork");
    case 0:
      signal(SIGINT, SIG_DFL);
      signal(SIGQUIT, SIG_DFL);
      chld_setpgid(lpid);
      CLOSEFD(pipefd[1]);
      DUPFD(pipefd[0], STDIN_FILENO);
      CLOSEFD(pipefd[0]);
      r_status = run_commands(n->right);
      fflush(NULL);
      _exit(r_status);
    default:
      setpgid(rpid, lpid);
      break;
  }

  if (outer)
    startjob(lpid);
  jobsig_unblk(&sigold);
  CLOSEFD(pipefd[0]);
  CLOSEFD(pipefd[1]);
  waitpid(lpid, &lw_status, 0);
  waitpid(rpid, &rw_status, 0);
  status = WIFEXITED(rw_status) ? WEXITSTATUS(rw_status) : 1;
  if (outer)
    endjob();

  pipedepth--;
  return status;
}

static void
save_fd(redir *r, fdlist *sfd, size_t *sfdc)
{
  redir *t;
  t = r;
  while (t) {
    sfd[*sfdc].orig = t->fd;
    sfd[(*sfdc)++].saved = dup(t->fd);
    t = t->next;
  }
  return;
}

static int
restore_fd(fdlist *sfd, size_t sfdc)
{
  for (size_t i = sfdc; i-- > 0;)
    DUPFD(sfd[i].saved, sfd[i].orig)
  for (size_t i = sfdc; i-- > 0;)
    CLOSEFD(sfd[i].saved)
  return 0;
}

char *
bg_cmd(const cmd_tree *n)
{
  switch (n->type) {
    case CMD:
      return CARGS(n)[0] ? CARGS(n)[0]->word : "(null)";
    case OP:
      return "(pipeline)";
    default:
      return "(command)";
  }
}

int
run_cmd(const cmd_tree *n)
{
  int status;
  char **final = NULL;
  char **env = NULL;
  shvar *v;
  shfunc *f;
  size_t i, vc;
  int b;

  vc = 0;
  final = expand_argv(CARGS(n));
  if (!final || !final[0]) {
    /*  if no command only name=value  */
    if (CVARS(n) && CVARS(n)[0]) {
      for (i = 0; CVARS(n)[i]; i++) {
        char *name, *val;
        shvar_flags flags;
        st_read_assn(CVARS(n)[i], &name, &val);
        v = findvar(name);
        if (v)
          flags = v->flags;
        else
          flags = 0;
        setvar(name, val, flags);
      }
      return 0;
    } else {
      return 0;
    }
  }

  if ((f = findfunc(final[0]))) {
    if (CVARS(n) && CVARS(n)[0]) {
      tmp_var tmp[MAX_TMP_VARS];
      vc = pushtmpvar(CVARS(n), tmp);
      status = run_func(f->body, final);
      poptmpvars(tmp, vc);
    } else {
      status = run_func(f->body, final);
    }
  } else if ((b = getbuiltin(*final)) >= 0) {
    /*  handle name=value cmd  */
    if (CVARS(n) && CVARS(n)[0]) {
      tmp_var tmp[MAX_TMP_VARS];
      vc = pushtmpvar(CVARS(n), tmp);
      status = builtin_launch(b, final);
      poptmpvars(tmp, vc);
    } else {
      status = builtin_launch(b, final);
    }
  } else {
    env = build_env(CVARS(n));
    if (!env) {
      perror("idk build_env failed I guess");
      return 1;
    }
    status = shexec(final, env);
    free(env);
  }
  if (CNEG(n))
    status = !status;
  return status;
}

int
run_redir(const cmd_tree *n)
{
  fdlist sfd[10];
  size_t sfdc;
  redir *r;

  sfdc = 0;
  r = CREDR(n);
  save_fd(CREDR(n), sfd, &sfdc);
  
  while (r) {
    char *name;
    int fd;
    if (!(name = expand_word(r->name)))
      return 1;
    switch (r->type) {
      case RDIN:
        OPENFD(name, O_RDONLY, fd)
        DUPFD(fd, r->fd)
        CLOSEFD(fd)
        break;
      case RDOUT:
      case RDCLOB:
        OPENFD(name, O_WRONLY | O_CREAT | O_TRUNC, fd)
        DUPFD(fd, r->fd)
        CLOSEFD(fd)
        break;
      case RDAPP:
        OPENFD(name, O_WRONLY | O_CREAT | O_APPEND, fd)
        DUPFD(fd, r->fd)
        CLOSEFD(fd)
        break;
      case RDRW:
        OPENFD(name, O_RDWR | O_CREAT, fd)
        DUPFD(fd, r->fd)
        CLOSEFD(fd)
        break;
      case RDDUPO:
      case RDDUPI:
        if (strcmp(name, "-") == 0) {
          CLOSEFD(r->fd)
        } else {
          long fdl;
          char *end;
          errno = 0;
          fdl = strtol(name, &end, 10);
          if (*end != '\0' || errno == ERANGE) {
            perror("simpsh");
            return 1;
          }
          fd = (int)fdl;
          DUPFD(fd, r->fd);
        }
        break;
    }
    r = r->next;
  }

  lstatus = run_commands(n->left);
  if (!(n->left && n->left->type == CMD &&
        CARGS(n->left) &&
        CARGS(n->left)[0] &&
        !CARGS(n->left)[1] &&
        !strcmp(CARGS(n->left)[0]->word, "exec"))) {
    fflush(NULL);
    restore_fd(sfd, sfdc);
  }
  return lstatus;
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
    case FUNC:
      return lstatus = run_funcdef(n);
    case REDIR:
      return lstatus = run_redir(n);
    case OP:
      if (COPP(n) != TPIPE && COPP(n) != TBKGRND)
        lstatus = run_commands(n->left);
      switch (COPP(n)) {
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
        case TBKGRND:
          return lstatus = run_bg(n);
        case TEOF:
          return lstatus;
        default:
          fprintf(stderr, "Unknown Operator\n");
          return 1;
      }
  }
  return 0;
}
