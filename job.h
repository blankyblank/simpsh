#ifndef JOB_H
#define JOB_H
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include "sig.h"

typedef struct job job;
struct job {
  job *next;
  pid_t pgid;              /* process group */
  int state;               /* JRUN, JSTP, JDONE */
  int num;                 /* job number */
  char *cmd;               /* command string */
  int flags;               /* JCHANGED, JFG, JSAVEDTTY, JSAVEDTTYPGRP */
  int age;                 /* monotonic counter for current-job selection */
  struct termios ttystate; /* saved on stop (valid when JSAVEDTTY) */
  pid_t saved_ttypgrp;     /* saved on stop (valid when JSAVEDTTYPGRP) */
  pid_t status_pid;        /* which pid's exit determines job exit status */
  int wstatus;             /* wait status of status_pid */
  int lwstatus;            /* wait status of for chld on left (pipefail needs this) */
  int nlive;               /* # of processes still running (not yet exited) */
};

#define JRUN 0
#define JSTP 1
#define JDONE 2
#define JCHANGED 0x01      /* state changed since last notification */
#define JFG 0x02           /* currently in foreground (has terminal) */
#define JSAVEDTTY 0x04     /* j->ttystate is valid */
#define JSAVEDTTYPGRP 0x08 /* j->saved_ttypgrp is valid */

extern pid_t sh_pgid;
extern job *job_list;
extern struct termios sh_termios;

extern job *newjob(pid_t, const char *);
extern job *rmjob(job *j);
extern void killjob(void);
extern void jobnotify(void);
extern void jobmsg(job *j);
extern void child_setup_fg(pid_t);
extern void child_setup_bg(void);
extern int startjob(pid_t);

extern int bgcmd(char **);
extern int fgcmd(char **);
extern int jobscmd(char **);

/* lock job signal */
#define job_lock(old) sigprocmask(SIG_BLOCK, &sigchldmask, old)
/* unblock job signal */
#define job_unlock(old) sigprocmask(SIG_SETMASK, old, NULL)
#define init_pgrp() sh_pgid = getpgrp()

/*
 * return term back to shell
 */
static inline void
ttyrestore(void)
{
  tcsetpgrp(STDIN_FILENO, sh_pgid);
  tcsetattr(STDIN_FILENO, TCSADRAIN, &sh_termios);
}

#endif /* JOB_H */
