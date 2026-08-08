/* exec.c - functions surrounding running external programs or builtins */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "alloc.h"
#include "arith.h"
#include "builtins.h"
#include "env.h"
#include "errmsg.h"
#include "exec.h"
#include "expand.h"
#include "glob.h"
#include "job.h"
#include "lex.h"
#include "main.h"
#include "parse.h"
#include "path.h"
#include "pipe.h"
#include "sig.h"
#include "utils.h"
#include "var.h"

#define LOOPERR 130
#define MAX_TMP_VARS 40
#define FD_MAX 10
#define FD_CACHE_MAX 4
#define HEREDOC_LIMIT 65536
#define OPENRW 0666
#define xpnd(a) (join_wf(exp_word((a), NULL)))
#define _INCHLD (1 << 0)

typedef struct fdlist {
  int orig;
  int saved;
} fdlist;
struct redirtable {
  int flags;
  mode_t mode;
};

static const struct redirtable redir_tab[] = {
  [RDIN] = { O_RDONLY },
  [RDOUT] = { O_WRONLY | O_CREAT | O_TRUNC, OPENRW },
  [RDCLOB] = { O_WRONLY | O_CREAT | O_TRUNC, OPENRW },
  [RDAPP] = { O_WRONLY | O_CREAT | O_APPEND, OPENRW },
  [RDRW] = { O_RDWR | O_CREAT, OPENRW },
};
redir *predir = NULL;

static void poptmpvars(tmp_var *, size_t);
static int save_fd(redir *, fdlist *, size_t * restrict);
static char *bg_cmd(const cmd_tree *);
static pid_t forkrun(int);
static int fgwait_simple(pid_t, const char *);
static int run_if(const cmd_tree *);
static int run_case(const cmd_tree *);
static int run_while(const cmd_tree *);
static int run_for(const cmd_tree *);
static int run_func(const cmd_tree *restrict, char **restrict);
static int run_pipe(const cmd_tree *);
static int run_bg(const cmd_tree *);
static int run_redir(const cmd_tree *n, int nchld);
static int run_subsh(const cmd_tree *, int);
static int run_cmd(const cmd_tree *, int);
static int runsbltn(const builtin *restrict, char **restrict, wf **restrict);
static int runshcmd(shfunc *restrict, const builtin *restrict, char **restrict, wf **restrict);
static int runextcmd(char **restrict, wf **restrict, const cmd_tree *restrict, int);
static void shexec(char ** restrict, char ** restrict, redir *)
  __attribute__((noreturn));
int execcmd(char **);
static int shfexec(char ** restrict, char ** restrict, const char * restrict, redir *);

static char *
bg_cmd(const cmd_tree *n)
{
  switch (n->type) {
    case CMD:
      {
        char **argv;
        size_t len;
        argv = expand_argv(CARGS(n), &len);
        return join_strn(argv, &len);
      }
    case OP:
      return "(pipeline)";
    default:
      return "(command)";
  }
}

static inline int
restore_fd(fdlist *sfd, size_t sfdc)
{
  for (ssize_t i = sfdc; i-- > 0;)
    if (sfd[i].saved < 0)
      close(sfd[i].orig);
    else if (dup2(sfd[i].saved, sfd[i].orig) < 0)
      return sherr(1, "redirection", "dup2");
  for (ssize_t i = sfdc; i-- > 0;)
    if (sfd[i].saved >= 0 && close(sfd[i].saved) < 0)
      return sherr(1, "redirection", "close");
  return 0;
}

static int
apply_redir(redir *r)
{
  int qs;
  while (r) {
    char *name;
    int fd, flags, cached = 0;
    if (!(name = xpnd(r->name)))
      return 1;
    switch (r->type) {
      case RDIN:
      case RDAPP:
      case RDCLOB:
      case RDRW:
        flags = redir_tab[r->type].flags;
        OPENFD(name, flags, fd)
        goto dupredir;
        break;
      case RDOUT:
        if (Cflag) {
          if ((fd = open(name, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL, OPENRW)) < 0)
            return sherr(1, name, "open");
        } else {
          OPENFD(name, O_WRONLY | O_CREAT | O_TRUNC, fd)
        }
        goto dupredir;
        /* fall through */
dupredir:
        DUPFD(fd, r->fd)
        if (!cached && fd != r->fd)
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
            return sherr(1, "redirection", "close");
          }
          fd = atoi_(name);
          DUPFD(fd, r->fd);
        }
        break;

      case RDHERE_D:
      case RDHERE:
        if (!r->heredoc)
          return shwarn("heredoc", "EOF NOT FOUND");

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
          if (blen < HEREDOC_LIMIT) {
            if (pipe(p) < 0)
              return sherr(1, "heredoc", "pipe");
            if (write(p[1], body, blen) < 0)
              return sherr(1, "heredoc", "write");
            CLOSEFD(p[1]);
            DUPFD(p[0], r->fd);
            CLOSEFD(p[0]);
          } else {
            char tmpf[] = "/tmp/simpsh-heredoc-XXXXXX";
            int fd;
            if ((fd = mkstemp(tmpf)) < 0)
              return 1;
            unlink(tmpf);
            if (write(fd, body, blen) < 0) {
              close(fd);
              return 1;
            }
            lseek(fd, 0, SEEK_SET);
            DUPFD(fd, r->fd);
            CLOSEFD(fd);
          }
          break;
        }
    }
    r = r->next;
  }
  return 0;
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
  int saved;
  redir *t;
  t = r;
  while (t) {
    sfd[*sfdc].orig = t->fd;
    if ((saved = dup(t->fd)) < 0) {
      if (errno == EBADF)
        sfd[(*sfdc)++].saved = -1;
      else
        return sherr(1, "redirection", "save fd failed");
    } else {
      sfd[(*sfdc)++].saved = saved;
    }
    t = t->next;
  }
  return 0;
}

static int
fgwait_simple(pid_t pid, const char *cmd)
{
  int wstatus;

  for (;;) {
    runeventloop(&el, -1);
    intsigchk(-pid);
    sigquitchk(-pid);

    switch (waitpid(pid, &wstatus, WNOHANG | WUNTRACED)) {
      case -1:
        ttyreclaim();
        return 1;
      case 0:
        continue;
      default:
        if (WIFSTOPPED(wstatus)) {
          job *j = newjob(pid, cmd);
          j->wstatus = wstatus;
          j->state = JSTP;
          j->flags = JCHANGED;
          tcgetattr(tty_fd, &j->ttystate);
          j->flags |= JSAVEDTTY;
          j->saved_ttypgrp = tcgetpgrp(tty_fd);
          if (j->saved_ttypgrp >= 0)
            j->flags |= JSAVEDTTYPGRP;
          ttyrestore();
          jobmsg(j);
          j->flags &= ~JCHANGED;
          return 128 + WSTOPSIG(wstatus);
        }
        ttyreclaim();
        if (WIFSIGNALED(wstatus)) {
          if (WTERMSIG(wstatus) == SIGINT)
            putchar('\n');
          return 128 + WTERMSIG(wstatus);
        }
        return WEXITSTATUS(wstatus);
    }
  }
}

static inline int
_wait_(pid_t pid)
{
  int wstatus;
  if (!mflag) {
    waitpid(pid, &wstatus, 0);
  } else {
    for (;;) {
      if (waitpid(pid, &wstatus, WNOHANG) > 0)
        break;
      runeventloop(&el, -1);
      intsigchk(pid);
      sigquitchk(pid);
    }
  }
  return WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) :
           (WIFSIGNALED(wstatus) ? 128 + WTERMSIG(wstatus) : 1);
}

int
forkexec(char *path, char **argv, char **env, const char *cmd, redir *r)
{
  pid_t pid;
  posix_spawnattr_t attr;
  sighandler_t oldquit, oldtstp, oldttin, oldttou;
  fdlist sfd[FD_MAX];
  size_t sfdc = 0;
  int err;

  if (r) {
    if (save_fd(r, sfd, &sfdc) || apply_redir(r)) {
      if (sfdc)
        restore_fd(sfd, sfdc);
      return 1;
    }
  }

  oldquit = signal(SIGQUIT, SIG_DFL);
  oldtstp = signal(SIGTSTP, SIG_DFL);
  oldttin = signal(SIGTTIN, SIG_DFL);
  oldttou = signal(SIGTTOU, SIG_DFL);
  posix_spawnattr_init(&attr);
  if (mflag) {
    posix_spawnattr_setpgroup(&attr, 0);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
  }
  err = posix_spawn(&pid, path, NULL, &attr, argv, env);
  posix_spawnattr_destroy(&attr);
  signal(SIGQUIT, oldquit);
  signal(SIGTTOU, oldttou);
  signal(SIGTTIN, oldttin);
  signal(SIGTSTP, oldtstp);

  if (r)
    restore_fd(sfd, sfdc);
  if (err) {
    errno = err;
    perror(path);
    return 127;
  }
  if (mflag && getpid() == sh_pgid) {
    startjob(pid);
    return fgwait_simple(pid, cmd);
  }
  return _wait_(pid);
}

static pid_t
forkrun(int bg)
{
  pid_t pid;

  switch (pid = fork()) {
    case -1:
      return sherr(-1, "fork", "create child process");
    case 0:
      if (bg)
        child_setup_bg();
      else
        child_setup_fg(0);
      return pid;
    default:
      return pid;
  }
}

void
shexec(char **restrict args, char **restrict env, redir *r)
{
  char *fpath;
  if (!(fpath = getpath(args[0]))) {
    fprintf(stderr, "%s: %s: command not found\n", SHARGV0, args[0]);
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
shfexec(char ** restrict argv, char ** restrict env, const char * restrict cmd, redir *r)
{
  char *fullpath;

  /* get full command path */
  fullpath = getpath(argv[0]);
  if (!fullpath) {
    fprintf(stderr, "%s: %s: command not found\n", SHARGV0, argv[0]);
    return 1;
  }
  return forkexec(fullpath, argv, env, cmd, r);
}

static inline int
run_funcdef(const cmd_tree *n)
{
  char *name;
  name = st_strndup(CFUNC(n)->word, CFUNC(n)->len);
  setfunc(name, n->left);
  return 0;
}

/**  run command tree  */
__attribute__((hot)) int
run_commands(const cmd_tree *n, int nchld)
{
  if (!n)
    return 0;
  if (RETNOW)
    return LSTATUS = RETVAL;

  while (n->type == OP && (COPP(n) == TSEMI || COPP(n) == TNL)) {
    LSTATUS = run_commands(n->left, 0);
    if (RETNOW || LOOPBREAK || LOOPCONT || !n->right)
      return LSTATUS;
    n = n->right;
  }

  switch (n->type) {
    case CMD:
      return LSTATUS = run_cmd(n, nchld);
    case SUBSHELL:
      return LSTATUS = run_subsh(n, nchld);
    case BRACE:
      return LSTATUS = run_commands(n->left, nchld);
    case FUNC:
      return LSTATUS = run_funcdef(n);
    case REDIR:
      return LSTATUS = run_redir(n, nchld);
    case WHILE:
      return LSTATUS = run_while(n);
    case FOR:
      return LSTATUS = run_for(n);
    case IF:
      return LSTATUS = run_if(n);
    case CASE:
      return LSTATUS = run_case(n);
    case OP:
      if (COPP(n) != TPIPE && COPP(n) != TBKGRND)
        LSTATUS = run_commands(n->left, 0);
      switch (COPP(n)) {
        case TAND:
          if (LSTATUS  != 0)
            return LSTATUS;
          return LSTATUS = run_commands(n->right, nchld);
        case TOR:
          if (LSTATUS  == 0)
            return LSTATUS;
          return LSTATUS = run_commands(n->right, nchld);
        case TPIPE:
          return LSTATUS = run_pipe(n);
        case TBKGRND:
          return LSTATUS = run_bg(n);
        case TEOF:
          return LSTATUS;
        default:
          fprintf(stderr, "Unknown Operator\n"); /*NOLINT*/
          return 1;
      }
    default:
      return 1;
  }
  return 0;
}

static int
runsbltn(const builtin *restrict b, char **restrict final, wf **restrict vars)
{
  fdlist sfd[FD_MAX];
  size_t sfdc = 0;
  volatile int st = 0;
  jmploc * volatile svhandler;
  jmploc jmploc;

  if (predir) {
    fflush(shout);
    if (save_fd(predir, sfd, &sfdc) || apply_redir(predir))
      return 1;
  }
  if (vars && vars[0]) {
    for (size_t i = 0; vars[i]; i++) {
      char *name, *val;
      st_read_assn(xpnd(vars[i]), &name, &val);
      setvar(name, val, 0);
    }
  }
  svhandler = handler;
  if (sigsetjmp(jmploc.loc, 0)) {
    handler = svhandler;
    if (chksig[SIGQUIT]) {
      st = 128 + SIGQUIT;
      chksig[SIGQUIT] = 0;
    } else {
      st = 128 + SIGINT;
    }
    intsig = 0;
    unblocksigs();
    putchar('\n');
  } else {
    handler = &jmploc;
    st = builtin_launch(b, final);
    handler = svhandler;
  }
  if ((int)st > 0) {
    if (!iflag && b->fn != returncmd) {
      if (fakectx)
        return 1;
      else
        exit(1);
    }
  }
  if (predir) {
    if (!(b && b->fn == &execcmd && !final[1])) {
      fflush(NULL);
      restore_fd(sfd, sfdc);
    }
  }
  return (int)st;
}

static int
runshcmd(shfunc *restrict f, const builtin *restrict b, char **restrict final, wf **restrict vars)
{
  fdlist sfd[FD_MAX];
  size_t i, sfdc = 0;
  volatile size_t vc;
  jmploc * volatile svhandler;
  jmploc jmploc;
  static tmp_var tmp[MAX_TMP_VARS];
  int status;

  if (predir) {
    fflush(shout);
    if (save_fd(predir, sfd, &sfdc) || apply_redir(predir))
      return 1;
  }
  if (vars && vars[0]) {
    for (vc = 0, i = 0; vars[i]; i++) {
      char *name, *val;
      st_read_assn(xpnd(vars[i]), &name, &val);
      tmp[i] = grabvar(name);
      vc++;
      setvar(name, val, 0);
    }
  }
  svhandler = handler;
  if (sigsetjmp(jmploc.loc, 0)) {
    handler = svhandler;
    if (chksig[SIGQUIT]) {
      status = 128 + SIGQUIT;
      chksig[SIGQUIT] = 0;
    } else {
      status = 128 + SIGINT;
      intsig = 0;
    }
    unblocksigs();
    putchar('\n');
  } else {
    handler = &jmploc;
    status = f ? run_func(f->body, final) : builtin_launch(b, final);
    handler = svhandler;
  }
  if (vars && vars[0])
    poptmpvars(tmp, vc);
  if (predir) {
    if (!(b && b->fn == &execcmd && !final[1])) {
      fflush(NULL);
      restore_fd(sfd, sfdc);
    }
  }
  return status;
}

static int
runextcmd(char **restrict final, wf **restrict vars, const cmd_tree *restrict n, int inchld)
{
  size_t i;
  volatile size_t vc = 0;
  char **evars, **env = NULL;
  jmploc * volatile svhandler;
  int status;

  svhandler = handler;
  handler = NULL;
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
  if (!env)
    return shwarn("exec", "no environment found");
  if (inchld & _INCHLD)
    shexec(final, env, predir);
  else
    status = shfexec(final, env, bg_cmd(n), predir);
  if (evars)
    sfree(env);
  handler = svhandler;
  return status;
}

__attribute__((hot)) static int
run_cmd(const cmd_tree *n, int inchld)
{
  int status;
  size_t i, len;
  char ifl, efl;
  char **final = NULL;
  const builtin *volatile b = NULL;
  shfunc *f = NULL;
  shvar *v;
  stmark cm;

  ifl = iflag;
  efl = eflag;
  gstate.lineno = n->line;
  cm = stack_mark();
  {
    jmploc jmp;
    jmploc * const volatile sv = handler;
    handler = &jmp;
    if (sigsetjmp(jmp.loc, 0)) {
      handler = sv;
      intsig = 0;
      putchar('\n');
      unblocksigs();
      return 128 + SIGINT;
    }
    final = expand_argv(CARGS(n), &len);
    handler = sv;
  }
  if (gstate.nounseterr) {
    gstate.nounseterr = 0;
    if (!ifl)
      exit(1);
    return 1;
  }

  if (xflag) {
    char *xline, *ps4;
    ps4 = getvar("PS4");
    xline = join_strn(final, &len);
    printf("%s %s\n", ps4, xline);
  }

  if (!final || !final[0]) { /*  if no command only name=value  */
    if (CVARS(n) && CVARS(n)[0]) {
      wf **vars = CVARS(n);
      for (i = 0; vars[i]; i++) {
        char *name, *val /*, *evar*/;
        shvflags flags;
        char *evar;
        wf *w = vars[i];
        if (w && w->qs == QNONE && w->next && !w->next->next && w->next->qs == QARITH && w->len && w->word[w->len-1] == '=') {
          char valbuf[32], nbuf[64];
          size_t nlen, vlen;
          name = w->word;
          nlen = w->len - 1;
          i64 ival = arith_eval(w->next->word, w->next->len);
          vlen = lltoa(ival, valbuf);
          valbuf[vlen] = '\0';
          memcpy(nbuf, w->word, nlen);
          nbuf[nlen] = '\0';
          shvar *v = findvar(nbuf);
          setvar_i(nbuf, valbuf, ival, (v ? v->flags : 0));
        } else {
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
    }
    if (predir)
      status = apply_redir(predir);
    goto done;
  }
#ifdef DEBUG
  stack_state(final[0]);
#endif /* DEBUG */

  if ((b = findbuiltin(*final)) && (b->flags & SBLTN)) {
    status = runsbltn(b, final, CVARS(n));
  } else if ((f = findfunc(final[0]))) {
    status = runshcmd(f, NULL, final, CVARS(n));
  } else if (b) {
    status = runshcmd(NULL, b, final, CVARS(n));
  } else {
    status = runextcmd(final, CVARS(n), n, inchld);
  }
  if (CNEG(n))
    status = !status;
  if (efl && status != 0 && !ifl && !(n->flags & EFLAG_SAFE) && !CNEG(n))
    exit(status);
done:
  stack_restore(cm);
  return status;
}

static int
run_if(const cmd_tree *n)
{
  int status;
  status = run_commands(n->left, 0);
  if (status == 0)
    return run_commands(n->right, 0);
  if (CELSE(n))
    return run_commands(CELSE(n), 0);
  return status;
}

static int
run_case(const cmd_tree *n)
{
  wf *wrd;
  char *word;
  size_t wlen = 0, plen = 0;
  int gl = 0;

  if (!(wrd = exp_word(CCASE(n).word, &wlen)))
    return 1;
  word = join_wf(wrd);

  for (clause *cl = CCASE(n).clauses; cl; cl = cl->next) {
    for (size_t i = 0; cl->ptrn[i]; i++, gl = 0) {
      wf *pttrn = exp_word(cl->ptrn[i], &plen);
      if (!pttrn)
        continue;
      char *patstr = join_wf(pttrn);

      for (wf *f = pttrn; f; f = f->next)
        if (f->qs == QNONE && ismetachar(f->word, f->len))
          gl = 1;
      if (gl) {
        if (globmatch(patstr, word, 0)) {
          return run_commands(cl->body, 0);
        }
      } else if (strcmp(patstr, word) == 0) {
        return run_commands(cl->body, 0);
      }
    }
  }
  return 0;
}

static int
run_for(const cmd_tree *n)
{
  int status;
  size_t wrdc;
  stmark f;
  char **wrdv;

  LOOPDEPTH++;
  status = 0;
  wrdc = 0;
  if (CFOR(n).words) {
    wrdv = expand_argv(CFOR(n).words, &wrdc);
  } else {
    wrdc = SHARGC;
    wrdv = st_alloc((wrdc + 1) * sizeof(char *));
    for (size_t i = 0; i < wrdc; i++)
      wrdv[i] = st_strdup(SHARGV[i]);
    wrdv[wrdc] = NULL;
  }

  for (size_t i = 0; wrdv[i]; i++) {
    if (RETNOW)
      break;
    if (intsig) {
      intsig = 0;
      putchar('\n');
      return LOOPERR;
    }
    f = stack_mark();
    setvar(CFOR(n).name->word, wrdv[i], 0);
    status = run_commands(n->right, 0);
    stack_restore(f);
    if (LOOPBREAK)
      if (--LOOPBREAK >= 0)
        break;
    if (LOOPCONT) {
      if (--LOOPCONT > 0)
        break;
      continue;
    }
  }
  LOOPDEPTH--;
  return status;
}

static int
run_while(const cmd_tree *n)
{
  int status, cond;
  stmark w;

  LOOPDEPTH++;
  status = 0;
  for (;;) {
    if (RETNOW)
      break;
    if (intsig) {
      intsig = 0;
      putchar('\n');
      return LOOPERR;
    }
    w = stack_mark();
    cond = run_commands(n->left, 0);
    if ((n->flags & UNTIL) ? cond == 0 : cond != 0)
      break;
    status = run_commands(n->right, 0);
    stack_restore(w);
    if (LOOPBREAK)
      if (--LOOPBREAK >= 0)
        break;
    if (LOOPCONT) {
      if (--LOOPCONT > 0)
        break;
      continue;
    }
  }
  LOOPDEPTH--;
  return status;
}

static int
run_func(const cmd_tree *n, char **args)
{
  int status;
  tmp_var *loc;
  stmark fmark;
  size_t savedsp;

  loc = LOCALVARS;
  fmark = stack_mark();
  savedsp = LOCALCNT;

  pushframe();
  SHARGC = 0;
  array_len(args, SHARGC);
  SHARGC--;
  SHARGV = args + 1;
  ALLOCED = RETNOW = LOOPBREAK = LOOPDEPTH = LOOPCONT = 0;
  OPTIND = 1;
  OPTOFF = -1;

  if (gstate.funcdepth >= MAX_FUNC_DEPTH) {
    fprintf(stderr, "function: too many levels of recursion\n");
    status = 1;
    goto done;
  }
  gstate.funcdepth++;
  status = run_commands(n, 0);
  gstate.funcdepth--;
  if (RETNOW) {
    RETNOW = 0;
    status = RETVAL;
  }
  goto done;

done:

  while (LOCALCNT > savedsp) {
    LOCALCNT--;
    if (loc[LOCALCNT].set)
      setvar(loc[LOCALCNT].name, loc[LOCALCNT].val, loc[LOCALCNT].oldflags);
    else
      rmvar(loc[LOCALCNT].name);
  }
  stack_restore(fmark);
  freeshargv();
  popframe();
  return status;
}

static int
run_bg(const cmd_tree *n)
{
  pid_t pid;
  job *j;
  int status;

  switch (pid = forkrun(1)) {
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
      if (iflag)
        printf("[%d] %d\n", j->num, pid);
      return run_commands(n->right, 0);
  }
}

static int
run_redir(const cmd_tree *n, int nchld)
{
  fdlist sfd[FD_MAX];
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

  if (eflag && LSTATUS != 0 && !iflag && !(n->flags & EFLAG_SAFE))
    exit(LSTATUS);
  return status;
}

static int
run_subsh(const cmd_tree *n, int chld)
{
  int status;

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

  if (!canfakesubsh(n))
    goto realsubsh;
  fakestate *sv, ps;
  int svctx, svefl, svifl;
  fdlist sfd[FD_MAX];
  size_t sfdc = 0;

  ps = (fakestate) { .cwd = -1 };
  sv = fkstate, svctx = fakectx;
  svefl = eflag, svifl = iflag;
  eflag = 0, iflag = 0;
  if (predir && (save_fd(predir, sfd, &sfdc) || apply_redir(predir)))
    return 1;
  status = run_commands(n->left, 0);
  if (sfdc)
    restore_fd(sfd, sfdc);
  eflag = svefl, iflag = svifl;
  if (!(svefl && status && !svifl))
    fflush(NULL);
  fkrestore(&ps);
  LOOPBREAK = LOOPCONT = RETNOW = 0;
  fakectx = svctx, fkstate = sv;
  return status;

realsubsh:
  pid_t pid;
  int efl, ifl, mfl;

  mfl = (int)mflag;
  efl = (int)eflag;
  ifl = (int)iflag;
  switch (pid = forkrun(0)) {
    case -1:
      return 1;
    case 0:
      if (predir && apply_redir(predir))
        _exit(1);
      status = run_commands(n->left, _INCHLD);
      if (efl && status != 0 && !ifl)
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
        status = fgwait(j);
        if (CNEG(n))
          status = !status;
        if (efl && status != 0 && !ifl)
          exit(status);
        return status;
      }
      status = _wait_(pid);
      if (CNEG(n))
        status = !status;
      if (efl && status != 0 && !ifl && !(n->flags & EFLAG_SAFE))
        exit(status);
      return status;
  }
}

static int
run_pipe(const cmd_tree *n)
{
  int status = 0;

  if (!CPIPEC(n))
    return 0;
  for (size_t i = 0; i < CPIPEC(n); i++)
    if (!canfakepipe(CPIPE(n)[i]))
      goto realpipe;

  FILE *stdoutbk = NULL, *stdinbk = NULL;
  char *outbuf, *cleanbuf = NULL, *lastbuf = NULL;
  size_t outlen, llen = 0;
  fakestate *sv, ps;
  int svctx;

  ps = (fakestate) { .cwd = -1 };
  sv = fkstate, svctx = fakectx;
  fkstate = &ps;
  for (size_t i = 0; i < CPIPEC(n); i++) {
    fkinit(fkstate);
    if (i > 0) {
      stdinbk = shin;
      shin = fmemopen(lastbuf, llen, "r");
      cleanbuf = lastbuf;
    }
    if (i < CPIPEC(n) - 1) {
      fflush(shout);
      stdoutbk = shout;
      shout = open_memstream(&outbuf, &outlen);
    }
    status = run_commands(CPIPE(n)[i], 0);
    if (i < CPIPEC(n) - 1) {
      fclose(shout);
      shout = stdoutbk;
      lastbuf = outbuf;
      llen = outlen;
    }
    if (i > 0) {
      fclose(shin);
      if (cleanbuf)
        free(cleanbuf);
      shin = stdinbk;
    }
    fkrestore(fkstate);
  }
  fkstate = sv, fakectx = svctx;
  return (CNEG(n)) ? !status : status;

realpipe:
  int pipefd[2], prevr, mfl;
  pid_t pids[256];

  mfl = (int)mflag;
  prevr = -1;
  for (size_t i = 0; i < CPIPEC(n); i++) {
    if (i < CPIPEC(n) - 1 && pipe(pipefd) < 0) {
      err(1, "pipe");
    }
    pids[i] = fork();
    switch (pids[i]) {
      case -1:
        err(1, "fork");
      case 0:
        child_setup_fg(i > 0 ? pids[0] : 0);
        if (prevr >= 0)
          DUPFD(prevr, STDIN_FILENO);
        if (i < CPIPEC(n) - 1) {
          CLOSEFD(pipefd[0]);
          DUPFD(pipefd[1], STDOUT_FILENO);
          CLOSEFD(pipefd[1]);
        }
        if (prevr >= 0)
          CLOSEFD(prevr);
        int _st = run_commands(CPIPE(n)[i], _INCHLD);
        fflush(NULL);
        _exit(_st);
      default:
        if (mfl && i == 0)
          setpgid(pids[i], pids[i]);
        else if (mfl)
          setpgid(pids[i], pids[0]);
        if (prevr >= 0)
          CLOSEFD(prevr);
        if (i < CPIPEC(n) - 1) {
          CLOSEFD(pipefd[1]);
          prevr = pipefd[0];
        } else {
          prevr = -1;
        }
        break;
    }
  }

  if (prevr >= 0)
    CLOSEFD(prevr);
  if (mfl && getpid() == sh_pgid) {
    job *j;
    j = newjob(pids[0], bg_cmd(n));
    j->nlive = CPIPEC(n);
    j->status_pid = pids[CPIPEC(n) - 1];
    j->flags |= JFG;
    startjob(pids[0]);
    status = fgwait(j);
  } else {
    int wstatus;
    int pstatus = 0;
    int rstatus = 0;

    if (!mfl) {
      for (size_t i = 0; i < CPIPEC(n); i++) {
        waitpid(pids[i], &wstatus, 0);
        if (i == CPIPEC(n) - 1)
          rstatus = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : 1;
        else if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) != 0)
          pstatus = WEXITSTATUS(wstatus);
      }
    } else {
      for (;;) {
        if (waitpid(pids[CPIPEC(n) - 1], &wstatus, WNOHANG) > 0)
          break;
        runeventloop(&el, -1);
        if (intsig || chksig[SIGQUIT]) {
          int sig;
          if (intsig) {
            sig = SIGINT;
            intsig = 0;
          } else {
            sig = SIGQUIT;
            chksig[SIGQUIT] = 0;
          }
          if (mflag)
            kill(-pids[0], sig);
          else
            for (size_t i = 0; i < CPIPEC(n); i++)
              kill(pids[i], sig);
        }
      }
      for (size_t i = 0; i < CPIPEC(n) - 1; i++) {
        int ws, s;
        waitpid(pids[i], &ws, 0);
        s = WIFEXITED(ws) ? WEXITSTATUS(ws) : 1;
        if (s != 0)
          pstatus = s;
      }
      rstatus = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : 1;
    }
    status = pipeflag ? (rstatus ? rstatus : pstatus) : rstatus;
  }

  if (CNEG(n))
    status = !status;
  if (eflag && status != 0 && !iflag && !(n->flags & EFLAG_SAFE))
    exit(status);
  return status;
}

int
execcmd(char **argv)
{
  if (!argv[1])
    return 0;
  char *fullpath;
  char **env = build_env(NULL);

  if (!env)
    shwarn(argv[0], "failed to get environ"); /*NOLINT*/

  fullpath = getpath(argv[1]);
  if (!fullpath)
    goto fail;
  if (execve(fullpath, &argv[1], env) < 0)
    goto fail;
  return 0;

fail:
  if (env) {
    sfree(env);
  }
  sfree(fullpath);
  return sherrx(1, argv[0]);
}

