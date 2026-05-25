#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <err.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job.h"
#include "malloc.h"
#include "utils.h"

pid_t sh_pgid;
pid_t job_pgid;
job *job_list;
/* next job num */
static int njn = 1;


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
  nj->cmd = strdup_(cmd);
  nj->next = job_list;
  job_list = nj;
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

int
startjob(pid_t pgid)
{
  job_pgid = pgid;
  if (tcsetpgrp(STDIN_FILENO, pgid) < 0)
    err(-1, "tcset");
  return 0;
}

void
killjob(void)
{
  if (job_pgid > 0) {
    kill(-job_pgid, SIGCONT);
    kill(-job_pgid, SIGTERM);
    job_pgid = 0;
  }
}

job *
updatejobs(job *c)
{
  int wstatus;
  pid_t pid;

  if (!c)
    return NULL;

  if (c->state == JRUN) {
    pid = waitpid(-c->pgid, &wstatus, WNOHANG);
    switch (pid) {
      case -1:
        if (errno == ECHILD)
          c->state = JDONE;
        break;
      case 0:
        break;
      default:
        while ((pid = waitpid(-c->pgid, &wstatus, WNOHANG)) > 0)
          ;
        if (pid == -1 && errno == ECHILD)
          c->state = JDONE;
        break;
    }
  }
  c->next = updatejobs(c->next);
  return c;
}

void
notify_done(void)
{
  job *j, *cur, *sec;
  j = job_list;
  char c;
  cur = findjob(NULL);
  sec = findjob("%-");

  while (j) {
    if (cur == j)
      c = '+';
    else if (sec == j)
      c = '-';
    else
      c = ' ';
    if (j->state == JDONE) {
      printf("[%d]%c Done\t\t\t\t\t\t\t\t\t\t\t\t%s\n", j->num, c, j->cmd);
      j = rmjob(j);
    } else
    j = j->next;
  }
}

// void
// cleanjobs(void)
// {
//   job *j;
//   j = job_list;
//   
//   while (j) {
//     if (j->state == JDONE)
//     else
//       j = j->next;
//   }
// }

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

void
chld_setpgid(pid_t pgid)
{
  sigset_t set;

  if (setpgid(0, pgid) < 0 ) {
    warn("simpsh: setpgid");
  }
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  sigprocmask(SIG_UNBLOCK, &set, NULL);
}
