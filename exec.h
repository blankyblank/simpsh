/* exec.h - functions surrounding running external programs or builtins */
#ifndef EXEC_H
#define EXEC_H

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <sys/wait.h>

#include "job.h"
#include "parse.h"
#include "opts.h"
#include "sig.h"

#define _INCHLD (1 << 0)
extern int func_depth;

extern int run_commands(const cmd_tree *, int);
extern int run_cmdsub(const cmd_tree *);

#define DUPFD(s, d) \
  if (dup2(s, d) < 0) { \
    perror("dup2"); \
    return 1; \
  }
#define OPENFD(f, m, n) \
  if ((n = open(f, m, 0666)) < 0) { \
    perror("open"); \
    return 1; \
  }
#define CLOSEFD(f) \
  if (close(f) < 0) { \
    perror("close"); \
    return 1; \
  }

static inline  int
_wait_(pid_t pid)
{
  int wstatus;
  if (!mflag) {
    waitpid(pid, &wstatus, 0);
  } else {
    for (;;) {
      if (intsig) {
        intsig = 0;
        kill(pid, SIGINT);
      }
      if (waitpid(pid, &wstatus, WNOHANG) > 0)
        break;
      sigsuspend(&emptyset);
    }
  }
  return WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : 1;
}

static inline int
fgwait(job *j)
{
  int wstatus, wsig;
  sigset_t old;

  while (j->state == JRUN) {
    if (intsig) {
      intsig = 0;
      kill(-j->pgid, SIGINT);
    }
    sigsuspend(&emptyset);
  }

  if (j->state == JSTP) {
    tcgetattr(tty_fd, &j->ttystate);
    j->flags |= JSAVEDTTY;
    j->saved_ttypgrp = tcgetpgrp(tty_fd);
    if (j->saved_ttypgrp >= 0)
      j->flags |= JSAVEDTTYPGRP;
    ttyrestore();
    j->flags &= ~(JFG | JCHANGED);
    jobmsg(j);
    return 128 + WSTOPSIG(j->wstatus);
  }

  ttyrestore();
  j->flags &= ~(JFG | JCHANGED);
  wsig = WIFSIGNALED(j->wstatus) ? WTERMSIG(j->wstatus) : 0;
  wstatus = WIFEXITED(j->wstatus) ? WEXITSTATUS(j->wstatus) : 1;
  job_lock(&old);
  rmjob(j);
  job_unlock(&old);
  if (wsig == SIGINT)
    putchar('\n');
  return wstatus;
}

/* vim: set filetype=c: */
#endif /* EXEC_H */
