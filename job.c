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
  nj->cmd = s_strdup(cmd);
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

job *
update_jobs(job *c)
{
  int wstatus;
  pid_t pid;

  if (!c)
    return NULL;

  pid = waitpid(-c->pgid, &wstatus, WNOHANG);
  switch (pid) {
    case -1:
      if (errno == ECHILD) {
        printf("[%d] Done        %s\n", c->num, c->cmd);
        // c = rmjob(c);
        return update_jobs(rmjob(c));
      }
      break;
    case 0:
      break;
    default:
      while ((pid = waitpid(-c->pgid, &wstatus, WNOHANG)) > 0)
        ;
      if (pid == -1 && errno == ECHILD) {
        printf("[%d] Done        %s\n", c->num, c->cmd);
        return update_jobs(rmjob(c));
      }
      break;
  }
  c->next = update_jobs(c->next);
  return c;
}

int
startjob(pid_t pgid)
{
  job_pgid = pgid;
  if (tcsetpgrp(STDIN_FILENO, pgid) < 0)
    err(-1, "simpsh: tcset");
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
