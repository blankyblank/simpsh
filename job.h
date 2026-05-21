#ifndef JOB_H
#define JOB_H

#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <unistd.h>

extern pid_t sh_pgid;
extern pid_t job_pgid;

typedef struct job job;
struct job {
  job *next;
  pid_t pgid;
  int state;
  int num;
  char *cmd;
};

#define JRUN 0
#define JSTP 1


extern job *job_list;

#define init_job() sh_pgid = getpgrp()
extern int startjob(pid_t);
extern void killjob(void);
extern void chld_setpgid(pid_t);
extern job *newjob(pid_t, const char *);
extern job *update_jobs(job *);
extern job * rmjob(job *j); // is extern needed?

/*
 * unblock job signal
 */
#define jobsig_unblk(old) sigprocmask(SIG_SETMASK, old, NULL)

/*
 * block job signal
 */
static inline int
jobsig_blk(sigset_t *old)
{
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  return sigprocmask(SIG_BLOCK, &set, old);
}

/*
 * return term back to shell
 */
static inline void
endjob(void)
{
  tcsetpgrp(STDIN_FILENO, sh_pgid);
  job_pgid = 0;
}

#endif /* JOB_H */
