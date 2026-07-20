/* exec.h - functions surrounding running external programs or builtins */
#ifndef EXEC_H
#define EXEC_H

#define _POSIX_C_SOURCE 200809L
#include <sys/wait.h>
#include <signal.h>

#include "job.h"
#include "main.h"
#include "lex.h"
#include "parse.h"
#include "opts.h"
#include "sig.h"


#define builtin_launch(b, a) (b->fn(a))

extern int run_commands(const cmd_tree *, int);
extern int forkexec(char *, char **, char **, const char *, redir *r);

static inline int
fgwait(job *j)
{
  int wstatus, wsig;

  while (j->state == JRUN) {
    runeventloop(&el, -1);
    if (intsig) {
      intsig = 0;
      kill(-j->pgid, SIGINT);
    }
    killjob();
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

  ttyreclaim();
  j->flags &= ~(JFG | JCHANGED);
  wsig = WIFSIGNALED(j->wstatus) ? WTERMSIG(j->wstatus) : 0;
  wstatus = WIFEXITED(j->wstatus) ? WEXITSTATUS(j->wstatus) : 1;
  rmjob(j);
  if (wsig == SIGINT)
    putchar('\n');
  return wstatus;
}

/* vim: set filetype=c: */
#endif /* EXEC_H */
