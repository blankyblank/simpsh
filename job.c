#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "exec.h"
#include "job.h"
#include "main.h"
#include "opts.h"
#include "sig.h"
#include "utils.h"

pid_t sh_pgid;
job *job_list;
struct termios sh_termios;
static int njn = 1; /* next job num */

/* string for printing job status */
static const char * const jstates[] = { "Running", "Stopped", "Done" };

static job *findjob(const char *);

job *
newjob(pid_t pgid, const char *cmd)
{
  job *nj;
  nj = malloc(sizeof(job));
  if (!nj)
    return NULL;
  nj->pgid = pgid;
  nj->num = njn++;
  nj->state = JRUN;
  nj->flags = 0;
  nj->cmd = strdup_(cmd);
  nj->nlive = 1;
  nj->status_pid = pgid;
  nj->wstatus = 0;
  nj->lwstatus = 0;
  nj->ttystate = (struct termios){0};
  nj->saved_ttypgrp = -1;
  nj->next = job_list;
  job_list = nj;
  sh_bgpid = pgid;
  snprintf(sh_bgpid_s, 16, "%d", sh_bgpid);
  return nj;
}

job *
rmjob(job *j)
{
  job *n;
  job **p;

  n = j->next;
  p = &job_list;
  while (*p && *p != j)
    p = &(*p)->next;
  if (*p)
    *p = n;
  free(j->cmd);
  free(j);
  return n;
}

void
killjob(void)
{
  job *j;
  int wstatus;
  pid_t pid;
  sigset_t old;

  sigprocmask(SIG_BLOCK, &sigchldmask, &old);

  for (j = job_list; j; j = j->next) {
    int oldstate;

    if (j->state == JDONE)
      continue;
    oldstate = j->state;
    while ((pid = waitpid(-j->pgid, &wstatus, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
      if (pid == j->status_pid)
        j->wstatus = wstatus;
      else if (pipeflag)
        j->lwstatus = wstatus;

      if (WIFSTOPPED(wstatus)) {
        j->state = JSTP;
      } else if (WIFCONTINUED(wstatus)) {
        if (j->state == JSTP)
          j->state = JRUN;
      } else {
        j->nlive--;
      }
    }

    if (pipeflag && j->nlive <= 0 && j->state == JRUN) {
      int rstatus, cmb;
      rstatus = WIFEXITED(j->wstatus) ? WEXITSTATUS(j->wstatus) : 1;
      cmb = rstatus ? rstatus : (WIFEXITED(j->lwstatus) ? WEXITSTATUS(j->lwstatus) : 1);
      j->wstatus = cmb << 8;
    }

    if (j->nlive <= 0 && j->state == JRUN)
      j->state = JDONE;
    if (j->state != oldstate)
      j->flags |= JCHANGED;
  }
  sigprocmask(SIG_SETMASK, &old, NULL);
}

int
startjob(pid_t pgid)
{
  if (tcsetpgrp(STDIN_FILENO, pgid) < 0)
    err(-1, "tcset");
  return 0;
}

void
jobmsg(job *j)
{
  job *cur, *sec;
  char c;
  cur = findjob(NULL);
  sec = findjob("%-");
    if (cur == j)
      c = '+';
    else if (sec == j)
      c = '-';
    else
      c = ' ';

  printf("[%d]%c %-8s\t\t\t\t\t\t\t\t\t\t\t\t%s\n", j->num, c, jstates[j->state], j->cmd); //XXX: clang says potential buffer overflow (overflow index)
}

void
jobnotify(void)
{
job *j;
  sigset_t old;
  job_lock(&old);
  killjob();
  for (j = job_list; j; ) {
    if ((j->flags & JCHANGED) && !(j->flags & JFG)) {
      jobmsg(j);
      j->flags &= ~JCHANGED;
      if (j->state == JDONE)
        j = rmjob(j);
      else
        j = j->next;
    } else {
      j = j->next;
    }
  }
  job_unlock(&old);
}

job *
findjob(const char *s)
{
  job *j;

  j = job_list;
  if (!s || (s[0] == '%' && s[1] == '+') || (s[0] == '%' && s[1] == '%')) {
    for (job *t = j; t; t = t->next)
      if (t->state == JSTP)
        return t;
    for (job *t = j; t; t = t->next)
      if (t->state == JRUN)
        return t;
    return j;
  } else if (s[0] == '%' && s[1] == '-') {
    job *second;
    second = findjob(NULL);
    return second ? second->next : NULL;
  } else if (s[0] == '%' && isdigit_(s[1])) {
    int jnum = atoi_(s + 1);
    for (job *t = j; t; t = t->next)
      if (t->num == jnum)
        return t;
  }
  return NULL;
}

/* set up signal handlers for foreground command */
void
child_setup_fg(pid_t pgid)
{
  signal(SIGINT, SIG_DFL);
  signal(SIGQUIT, SIG_DFL);
  signal(SIGTSTP, SIG_DFL);
  signal(SIGTTIN, SIG_DFL);
  signal(SIGTTOU, SIG_DFL);
  if (mflag)
    if (setpgid(0, pgid) < 0)
      warn("simpsh: setpgid");
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  sigprocmask(SIG_UNBLOCK, &set, NULL);
}

void
child_setup_bg(void)
{
  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGTSTP, SIG_DFL);
  signal(SIGTTIN, SIG_DFL);
  signal(SIGTTOU, SIG_DFL);
  if (mflag)
    if (setpgid(0, 0) < 0)
      warn("simpsh: setpgid");
  close(0);
  open("/dev/null", O_RDONLY);
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  sigprocmask(SIG_UNBLOCK, &set, NULL);
}


// TODO: check interative mode or not
int
bgcmd(char **argv)
{
  size_t argc;
  job *j;
  sigset_t old;

  argc = array_len(argv);
  if (argc < 2) {
    job_lock(&old);
    j = findjob(NULL);
    if (!j) {
      shwarnx(argv[0], "no current job"); /*NOLINT*/
      return 1;
    }
    if (kill(-j->pgid, SIGCONT) < 0) {
      job_unlock(&old);
      err(1, "kill");
    }
    j->state = JRUN;
    j->flags |= JCHANGED;
    job_unlock(&old);
    jobmsg(j);
    return 0;
  } else {
    for (size_t i = 1; i < argc; i++) {
      job_lock(&old);
      j = findjob(argv[i]);
      if (!j) {
        shwarn_arg(argv[0], argv[i], "no such job"); /*NOLINT*/
        return 1;
      }
      if (j->state != JSTP) {
        shwarn_arg(argv[0], argv[i], "job not stopped"); /*NOLINT*/
        return 1;
      }
      if (kill(-j->pgid, SIGCONT) < 0) {
        job_unlock(&old);
        err(1, "kill");
      }
      j->state = JRUN;
      j->flags |= JCHANGED;
      job_unlock(&old);
      jobmsg(j);
    }
  }
  return 0;
}

int
fgcmd(char **argv)
{
  size_t argc;
  job *j;
  sigset_t old;

  argc = array_len(argv);
  if (argc < 2) {
    job_lock(&old);
    j = findjob(NULL);
    if (!j) {
      shwarnx(argv[0], "no current job"); /*NOLINT*/
      job_unlock(&old);
      return 1;
    }
  } else {
    job_lock(&old);
    j = findjob(argv[argc - 1]);
    if (!j) {
      shwarn_arg(argv[0], argv[argc - 1], "no such job"); /*NOLINT*/
      job_unlock(&old);
      return 1;
    }
  }

  if (j->state == JDONE) {
    shwarn_arg(argv[0], argv[argc - 1], "job already completed"); /*NOLINT*/
    job_unlock(&old);
    return 1;
  }

  if (j->state == JSTP) {
    if (kill(-j->pgid, SIGCONT) != 0) {
      job_unlock(&old);
      err(1, "kill");
    }
    j->state = JRUN;
  }
  j->flags |= JFG;
  if (j->flags & JSAVEDTTY)
    tcsetattr(STDIN_FILENO, TCSADRAIN, &j->ttystate);
  startjob(j->pgid);
  job_unlock(&old);
  return fgwait(j);
}

int
jobscmd(char **argv)
{
  job *j;
  size_t argc;
  sigset_t old;

  j = job_list;
  argc = array_len(argv);

  job_lock(&old);
  if (argc < 2) {
    for (job *t = j; t;) {
      jobmsg(t);
      if (t->state == JDONE)
        t = rmjob(t);
      else
        t = t->next;
    }
  } else {
    for (size_t i = 1; i < argc; i++) {
      job *t;
      if ((t = findjob(argv[i]))) {
        jobmsg(t);
        if (t->state == JDONE)
          rmjob(t);
      } else {
        shwarn_arg(argv[0], argv[i], "no such job"); /*NOLINT*/
      }
    }
  }

  job_unlock(&old);
  return 0;
}

