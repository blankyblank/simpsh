/* exec.c - functions surrounding running external programs or builtins */
#define _POSIX_C_SOURCE 200809L
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "env.h"
#include "exec.h"
#include "expand.h"
#include "job.h"
#include "main.h"
#include "alloc.h"
#include "parse.h"
#include "path.h"
#include "sig.h"
#include "utils.h"
#include "var.h"

#define MAX_TMP_VARS 40
#define xpnd(a) (join_wf(exp_word(a, NULL)))
#define builtin_launch(i, a) ((*builtin_funcs[i])(a))

int func_depth = 0;
redir *predir = NULL;

typedef struct fdlist {
  int orig;
  int saved;
} fdlist;

static const struct {
  int flags;
  mode_t mode;
} redir_tab[] = {
  [RDIN] = { O_RDONLY },
  [RDOUT] = { O_WRONLY | O_CREAT | O_TRUNC, 0666 },
  [RDCLOB] = { O_WRONLY | O_CREAT | O_TRUNC, 0666 },
  [RDAPP] = { O_WRONLY | O_CREAT | O_APPEND, 0666 },
  [RDRW] = { O_RDWR | O_CREAT, 0666 },
};

static int findbuiltin(char *);
static void poptmpvars(tmp_var *, size_t);
static int save_fd(redir *, fdlist *, size_t * restrict);
static char *bg_cmd(const cmd_tree *);
static __attribute__((noreturn)) void shexec(char **restrict, char **restrict, redir *);
static int shfexec(char **restrict, const cmd_tree *restrict, char **restrict, redir *);
static pid_t forkrun(sigset_t *, int);
static int run_if(const cmd_tree *);
static int run_while(const cmd_tree *);
static int run_for(const cmd_tree *);
static int run_func(const cmd_tree *restrict, char **restrict);
static int run_pipe(const cmd_tree *);
static int run_bg(const cmd_tree *);
static int run_subsh(const cmd_tree *, int);
static int run_cmd(const cmd_tree *, int);


static inline int
restore_fd(fdlist *sfd, size_t sfdc)
{
  for (size_t i = sfdc; i-- > 0;) {
    if (dup2(sfd[i].saved, sfd[i].orig) < 0) {
      perror("dup2");
      return 1;
    }
  }
  for (size_t i = sfdc; i-- > 0;) {
    if (close(sfd[i].saved) < 0) {
      perror("close");
      return 1;
    }
  }
  return 0;
}

static int
apply_redir(redir *r)
{
  int qs;
  while (r) {
    char *name;
    int fd;
    if (!(name = xpnd(r->name)))
      return 1;
    switch (r->type) {

      case RDIN:
      case RDAPP:
      case RDCLOB:
      case RDRW:
        OPENFD(name, redir_tab[r->type].flags, fd)
        goto dupfall;
        break;
      case RDOUT:
        if (Cflag) {
          if ((fd = open(name, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC, 0666)) <
              0) {
            warn("%s", name);
            return 1;
          }
        } else {
          OPENFD(name, O_WRONLY | O_CREAT | O_TRUNC, fd)
        }
        goto dupfall;
        /* fall through */
dupfall:
        DUPFD(fd, r->fd)
        CLOSEFD(fd)
        break;
      case RDDUPO:
      case RDDUPI:
        if (name[0] == '-' && name[1] == '\0') {
          CLOSEFD(r->fd)
        } else {
          char *p = name;
          while (*p && isdigit_(*p))
            p++;
          if (*p) {
            err(1, "simpsh");
          }
          fd = atoi_(name);
          DUPFD(fd, r->fd);
        }
        break;
      case RDHERE_D:
      case RDHERE:
        if (!r->heredoc) {
          warn("heredoc not found");
          return 1;
        }
        {
          wf b;
          char *body;
          int p[2];
          size_t blen;

          qs = 0;
          for (wf *f = r->name; f; f = f->next)
            if (f->qs != QNONE) {
              qs = 1;
              break;
            }
          if (qs) {
            body = r->heredoc;
            blen = strlen(body);
          } else {
            b.word = r->heredoc;
            b.len = strlen(r->heredoc);
            b.qs = QHEREDOC;
            b.next = NULL;

            body = xpnd(&b);
            blen = strlen(body);
          }
          if (pipe(p) < 0)
            err(1, "pipe");
          if (write(p[1], body, blen) < 0)
            err(1, "write");
          CLOSEFD(p[1]);
          DUPFD(p[0], r->fd);
          CLOSEFD(p[0]);
          break;
        }
    }
    r = r->next;
  }
  return 0;
}


/**  get builtin command  */
static inline int
findbuiltin(char *args)
{
  size_t idx;
  idx = hash(args, BUILTIN_BUCKETS);
  for (; builtin_tab[idx] >= 0; idx = (idx + 1) & (BUILTIN_BUCKETS - 1))
    if (builtins[builtin_tab[idx]][0] == args[0] &&
        strcmp(builtins[builtin_tab[idx]], args) == 0)
      return builtin_tab[idx];
  return -1;
}

static inline void
poptmpvars(tmp_var *tmp, size_t vc)
{
  for (size_t i = 0; i < vc; i++) {
    if (tmp[i].set)
      setvar(tmp[i].name, tmp[i].val, tmp[i].oldflags);
    else
      rmvar(tmp[i].name);
  }
}

static inline int
save_fd(redir *r, fdlist *sfd, size_t * restrict sfdc)
{
  redir *t;
  t = r;
  while (t) {
    sfd[*sfdc].orig = t->fd;
    if ((sfd[(*sfdc)++].saved = dup(t->fd)) < 0) {
      shwarn("redirection", "save fd failed");
      return 1;
    }
    t = t->next;
  }
  return 0;
}

static pid_t
forkrun(sigset_t *old, int bg)
{
  pid_t pid;

  job_lock(old);
  switch (pid = fork()) {
    case -1:
      perror("fork");
      job_unlock(old);
      return -1;
    case 0:
      job_unlock(old);
      if (bg)
        child_setup_bg();
      else
        child_setup_fg(0);
      return pid;
    default:
      return pid;
  }
}

static char *
bg_cmd(const cmd_tree *n)
{
  switch (n->type) {
    case CMD:
      {
        char **argv;
        size_t len;
        argv = expand_argv(CARGS(n), &len);
        return join_strn(argv, len);
      }
    case OP:
      return "(pipeline)";
    default:
      return "(command)";
  }
}

void
shexec(char **restrict args, char **restrict env, redir *r)
{
  char *fpath;
  if (!(fpath = getpath(args[0]))) {
    fprintf(stderr, "%s: %s: command not found\n", sh_argv0, args[0]);
    _exit(127);
  }

  if (r && apply_redir(r))
    _exit(1);
  if (execve(fpath, args, env) < 0) {
    perror(args[0]);
    _exit(1);
  }
  _exit(0);
}

/** fork and exec external command */
static int
shfexec(char **restrict args, const cmd_tree *restrict n, char **restrict env, redir *r)
{
  pid_t pid;
  sigset_t old;
  char *fullpath;

  /* get full command path */
  fullpath = getpath(args[0]);
  if (!fullpath) {
    fprintf(stderr, "%s: %s: command not found\n", sh_argv0, args[0]); /*NOLINT*/
    return 1;
  }

  job_lock(&old);
  pid = fork();
  switch (pid) {
    case -1:
      perror("failed to create");
      job_unlock(&old);
      return 1;
    case 0:
      job_unlock(&old);
      child_setup_fg(0);
      shexec(args, env, r);
    /* fall through */
    default:
      // XXX: i really want to clean up all this jobcontrol stuff
      if (mflag && getpid() == sh_pgid) {
        job *j;
        setpgid(pid, pid);
        j = newjob(pid, bg_cmd(n));
        j->flags |= JFG;
        startjob(pid);
        job_unlock(&old);
        return fgwait(j);
      }
      /* no job control */
      job_unlock(&old);
      return _wait_(pid);
  }
}

static int
run_if(const cmd_tree *n)
{
  int status;
  status = run_commands(n->left, 0);
  if (status == 0)
    return run_commands(n->right, 0);
  else if (CELSE(n))
    return run_commands(CELSE(n), 0);
  else
    return status;
}

static int
run_for(const cmd_tree *n)
{
  int status;
  size_t wrdc;
  stmark f;
  char **wrdv;

  status = 0;
  wrdc = 0;
  if (CFOR(n).words) {
    wrdv = expand_argv(CFOR(n).words, &wrdc);
  } else {
    wrdc = sh_argc;
    wrdv = st_alloc((wrdc + 1) * sizeof(char *));
    for (size_t i = 0; i < wrdc; i++)
      wrdv[i] = st_strdup(sh_argv[i]);
    wrdv[wrdc] = NULL;
  }

  for (size_t i = 0; wrdv[i]; i++) {
    if (retnow)
      break;
    f = stack_mark();
    setvar(CFOR(n).name->word, wrdv[i], 0);
    status = run_commands(n->right, 0);
    stack_restore(f);
  }
  return status;
}

static int
run_while(const cmd_tree *n)
{
  int status, cond;
  stmark w;
  status = 0;
  for (;;) {
    if (retnow)
      break;
    w = stack_mark();
    cond = run_commands(n->left, 0);
    if ((n->flags & UNTIL) ? cond == 0 : cond != 0)
      break;
    status = run_commands(n->right, 0);
    stack_restore(w);
  }
  return status;
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
  int oldargc, status, oldalloced;
  tmp_var *loc;
  stmark fmark;
  size_t savedsp;

  loc = localvars;
  fmark = stack_mark();
  savedsp = localsp;
  oldargc = sh_argc;
  oldargv = sh_argv;
  oldalloced = alloc_sh_argv;
  sh_argc = 0;
  array_len(args, sh_argc);
  sh_argc--;
  sh_argv = args + 1;
  alloc_sh_argv = 0;


  if (func_depth >= MAX_FUNC_DEPTH) {
    fprintf(stderr, "function: too many levels of recursion\n");
    status = 1;
    goto done;
  }
  func_depth++;

  status = run_commands(n, 0);
  func_depth--;
  if (retnow) {
    retnow = 0;
    status = retval;
  }
  goto done;

done:

  while (localsp > savedsp) {
    localsp--;
    if (loc[localsp].set)
      setvar(loc[localsp].name, loc[localsp].val, loc[localsp].oldflags);
    else
      rmvar(loc[localsp].name);
  }

  stack_restore(fmark);
  freeshargv();
  alloc_sh_argv = oldalloced;
  sh_argv = oldargv;
  sh_argc = oldargc;
  return status;
}

static int
run_bg(const cmd_tree *n)
{
  pid_t pid;
  sigset_t sigold;
  job *j;
  int status;

  switch (pid = forkrun(&sigold, 1)) {
    case -1:
      return 1;
    case 0:
      status = run_commands(n->left, _INCHLD);
      fflush(NULL);
      _exit(status);
    default:
      if (mflag)
        setpgid(pid, pid);
      j = newjob(pid, bg_cmd(n->left));
      job_unlock(&sigold);
      jobmsg(j);
      return run_commands(n->right, 0);
  }
}

static int
run_subsh(const cmd_tree *n, int chld)
{
  int status;
  int efl, ifl, mfl;
  pid_t pid;
  sigset_t old;

  if (!n->left) {
    fprintf(stderr, "empty subshell\n");
    return 1;
  }

  if ((chld & _INCHLD) && !mflag) {
    if (predir && apply_redir(predir))
      _exit(1);
    status = run_commands(n->left, _INCHLD);
    fflush(NULL);
    _exit(status);
  }
  mfl = mflag;
  efl = eflag;
  ifl = iflag;

  switch (pid = forkrun(&old, 0)) {
    case -1:
      return 1;
    case 0:
      if (predir && apply_redir(predir))
        _exit(1);
      status = run_commands(n->left, _INCHLD);
      if (efl && status != 0 && !ifl) // XXX: make sure i should keep
        _exit(status);
      fflush(NULL);
      _exit(status);
    default:
      if (mfl && getpid() == sh_pgid) {
        job *j;
        setpgid(pid, pid);
        j = newjob(pid, bg_cmd(n));
        j->flags |= JFG;
        startjob(pid);
        job_unlock(&old);
        status = fgwait(j);
        if (CNEG(n))
          status = !status;
        if (efl && status != 0 && !ifl)
          exit(status);
        return status;
      }
      job_unlock(&old);
      status = _wait_(pid);
      if (CNEG(n))
        status = !status;
      if (efl && status != 0 && !ifl && !(n->flags & EFLAG_SAFE))
        exit(status);
      return status;
  }
}

int
run_cmdsub(const cmd_tree *n)
{
  int wstatus, ret, pipefd[2];
  pid_t pid;
  sigset_t old;
  static char buf[MINSTACK_S];
  size_t len;

  if (!n)
    return -1;
  job_lock(&old);
  pipefd[0] = pipefd[1] = -1;
  if (pipe(pipefd) < 0) {
    perror("pipe");
    job_unlock(&old);
    ret = -1;
    goto cleanup;
  }

  pid = fork();
  switch (pid) {
    case -1:
      job_unlock(&old);
      warn("fork");
      ret = -1;
      goto cleanup;
    case 0:
      job_unlock(&old);
      if (close(pipefd[0]) < 0)
        perror("close");
      if (dup2(pipefd[1], STDOUT_FILENO) < 0)
        warn("dup");
      if (close(pipefd[1]) < 0)
        warn("close");
      child_setup_fg(0);
      lstatus = run_commands(n, _INCHLD);
      fflush(NULL);
      _exit(lstatus);
    default:
      {
        int n;
        len = 0;

        job_unlock(&old);
        if (close(pipefd[1]) < 0) {
          perror("close");
          ret = -1;
          goto cleanup;
        }

        while ((n = read(pipefd[0], buf, sizeof(buf)))) {
          if (n > 0) {
            if (stleft <= (size_t)n)
              grow_stack(2048);
            memcpy(stnext, buf, n);
            len += n;
            stnext += n;
            stleft -= n;
            continue;
          } else if (n == 0) {
            break;
          } else if (n == -1 && errno == EINTR) {
            continue;
          } else if (n == -1 && errno != EINTR) {
            ret = -1;
            goto cleanup;
          }
        }

        while (len > 0 && *(stnext - 1) == '\n') {
          stleft++;
          stnext--;
          len--;
        }
        *stnext = '\0';
        ret = len;

        if (waitpid(pid, &wstatus, 0) > 0)
          lstatus = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : 1;
        else
          lstatus = 1;
      }
  }

cleanup:
  if (pipefd[0] >= 0)
    close(pipefd[0]);
  if (pipefd[1] >= 0)
    close(pipefd[1]);
  return ret;
}

static int
run_pipe(const cmd_tree *n)
{
  int status, lwstatus;
  int lstatus, rstatus;
  int pipefd[2], outer;
  static int pipedepth;
  int mfl;
  pid_t lpid, rpid;
  sigset_t old;

  pipedepth++;
  outer = (pipedepth == 1);
  job_lock(&old);
  mfl = mflag;

  if (pipe(pipefd) < 0)
    err(1, "pipe");
  lpid = fork();
  switch (lpid) {
    case -1:
      CLOSEFD(pipefd[0]);
      CLOSEFD(pipefd[1]);
      job_unlock(&old);
      err(1, "fork");
    case 0:
      job_unlock(&old);
      child_setup_fg(0);
      CLOSEFD(pipefd[0]);
      DUPFD(pipefd[1], STDOUT_FILENO);
      CLOSEFD(pipefd[1]); 
      int _st = run_commands(n->left, _INCHLD);
      fflush(NULL);
      _exit(_st);
    default:
      if (mfl)
        setpgid(lpid, lpid);
      break;
  }

  rpid = fork();
  switch (rpid) {
    case -1:
      CLOSEFD(pipefd[1]);
      kill(lpid, SIGTERM);
      waitpid(lpid, NULL, 0);
      job_unlock(&old);
      err(1, "fork");
    case 0:
      job_unlock(&old);
      child_setup_fg(lpid);
      CLOSEFD(pipefd[1]);
      DUPFD(pipefd[0], STDIN_FILENO);
      CLOSEFD(pipefd[0]);
      int _st = run_commands(n->right, _INCHLD);
      fflush(NULL);
      _exit(_st);
    default:
      if (mfl)
        setpgid(rpid, lpid);
      break;
  }

  CLOSEFD(pipefd[0]);
  CLOSEFD(pipefd[1]);

  if (mfl && outer && getpid() == sh_pgid) {
    job *j;
    j = newjob(lpid, bg_cmd(n));
    j->nlive = 2;
    j->status_pid = rpid;
    j->flags |= JFG;
    startjob(lpid);
    job_unlock(&old);
    status = fgwait(j);
  } else {
    int wstatus;
    job_unlock(&old);
    if (!mfl) {
      waitpid(rpid, &wstatus, 0);
      waitpid(lpid, &lwstatus, 0);
      rstatus = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : 1;
      lstatus = WIFEXITED(lwstatus) ? WEXITSTATUS(lwstatus) : 1;
      status = pipeflag ? (rstatus ? rstatus : lstatus) : rstatus;
    } else {
      for (;;) {
        if (intsig) {
          intsig = 0;
          kill(lpid, SIGINT);
          kill(rpid, SIGINT);
        }
        if (waitpid(rpid, &wstatus, WNOHANG) > 0)
          break;
        sigsuspend(&emptyset);
      }
      waitpid(lpid, &lwstatus, 0);
      rstatus = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : 1;
      lstatus = WIFEXITED(lwstatus) ? WEXITSTATUS(lwstatus) : 1;
      status = pipeflag ? (rstatus ? rstatus : lstatus) : rstatus;
    }
  }

  if (CNEG(n))
    status = !status;
  pipedepth--;
  if (eflag && status != 0 && !iflag && !(n->flags & EFLAG_SAFE))
    exit(status);
  return status;
}

__attribute__((hot)) static int
run_cmd(const cmd_tree *n, int inchld)
{
  int status;
  int ifl, efl;
  char **final = NULL;
  char **env = NULL;
  wf **vars;
  shvar *v;
  shfunc *f = NULL;
  size_t i, vc, len;
  int b = 0;

  ifl = iflag;
  efl = eflag;
  vars = CVARS(n);

  final = expand_argv(CARGS(n), &len);
  if (nounseterr) {
    nounseterr = 0;
    if (!ifl)
      exit(1);
    return 1;
  }

  if (xflag) {
    char *xline;
    sh_ps4 = getvar("PS4");
    sh_ps4 = sh_ps4 ? sh_ps4 : "+";
    xline = join_strn(final, len);
    printf("%s %s\n", sh_ps4, xline);
  }

  if (!final || !final[0]) {
    if (vars && vars[0]) { /*  if no command only name=value  */
      for (i = 0; vars[i]; i++) {
        char *name, *val /*, *evar*/;
        shvar_flags flags;
        char *evar;
        evar = xpnd(vars[i]);
        st_read_assn(evar, &name, &val);
        v = findvar(name);
        if (v)
          flags = v->flags;
        else
          flags = 0;
        setvar(name, val, flags);
      }
    }
    if (predir)
      return apply_redir(predir);
    return 0;
  }

  if ((f = findfunc(final[0])) || (b = findbuiltin(*final)) >= 0) {
    fdlist sfd[10];
    size_t sfdc = 0;
    if (predir)
      if (save_fd(predir, sfd, &sfdc) || apply_redir(predir))
        return 1;
    if (vars && vars[0]) {
      static tmp_var tmp[MAX_TMP_VARS];
      for (vc = 0, i = 0; vars[i]; i++) {
        char *name, *val;
        st_read_assn(xpnd(vars[i]), &name, &val);
        tmp[i] = grabvar(name);
        vc++;
        setvar(name, val, 0);
      }
      status = f ? run_func(f->body, final) : builtin_launch(b, final);
      poptmpvars(tmp, vc);
    } else {
      status = f ? run_func(f->body, final) : builtin_launch(b, final);
    }
    if (predir) {
      fflush(NULL);
      restore_fd(sfd, sfdc);
    }

  } else { /* if this is a external command */
    char **evars;
    if ((vc = CVARC(n))) {
      evars = st_alloc((vc + 1) * sizeof(char *));
      for (i = 0; i < vc; i++) {
        evars[i] = xpnd(vars[i]);
        evars[vc] = NULL;
      }
    } else {
      evars = NULL;
    }
    env = build_env(evars);
    if (!env) {
      perror("no environment found");
      return 1;
    }
    if (inchld & _INCHLD)
      shexec(final, env, predir);
    else
      status = shfexec(final, n, env, predir);
    if (evars)
      slfree(env);
  }
  if (CNEG(n))
    status = !status;
  if (efl && status != 0 && !ifl && !(n->flags & EFLAG_SAFE) && !CNEG(n))
    exit(status);
  return status;
}

static inline int
run_redir(const cmd_tree *n, int nchld)
{
  fdlist sfd[10];
  size_t sfdc;
  int status;
  redir *r;

  r = CREDR(n);

  if (n->left->type == CMD || n->left->type == SUBSHELL) {
    redir *prev = predir;
    predir = r;
    status = run_commands(n->left, nchld);
    predir = prev;
    return status;
  }

  sfdc = 0;
  if ((save_fd(CREDR(n), sfd, &sfdc)))
    return 1;
  if (apply_redir(r))
    return 1;
  status = run_commands(n->left, nchld);
  fflush(NULL);
  if (restore_fd(sfd, sfdc))
    return 1;

  if (eflag && lstatus != 0 && !iflag && !(n->flags & EFLAG_SAFE))
    exit(lstatus);
  return status;
}



/**  run command tree  */
__attribute__((hot)) int
run_commands(const cmd_tree *n, int nchld)
{
  int l_status;
  l_status = lstatus;

  if (!n)
    return 0;
  if (retnow)
    return lstatus = retval;

  while (n->type == OP && (COPP(n) == TSEMI || COPP(n) == TNL)) {
    lstatus = run_commands(n->left, 0);
    if (retnow || !n->right)
      return lstatus;
    n = n->right;
  }

  switch (n->type) {
    case CMD:
      return lstatus = run_cmd(n, nchld);
    case SUBSHELL:
      return lstatus = run_subsh(n, nchld);
    case FUNC:
      return lstatus = run_funcdef(n);
    case REDIR:
      return lstatus = run_redir(n, nchld);
    case WHILE:
      return lstatus = run_while(n);
    case FOR:
      return lstatus = run_for(n);
    case IF:
      return lstatus = run_if(n);
    case OP:
      if (COPP(n) != TPIPE && COPP(n) != TBKGRND)
        l_status = run_commands(n->left, 0);
      switch (COPP(n)) {
        case TAND:
          if (l_status != 0)
            return lstatus;
          return lstatus = run_commands(n->right, nchld);
        case TOR:
          if (l_status == 0)
            return lstatus;
          return lstatus = run_commands(n->right, nchld);
        case TPIPE:
          return lstatus = run_pipe(n);
        case TBKGRND:
          return lstatus = run_bg(n);
        case TEOF:
          return lstatus;
        default:
          fprintf(stderr, "Unknown Operator\n"); /*NOLINT*/
          return 1;
      }
    default:
      return 1;
  }
  return 0;
}
