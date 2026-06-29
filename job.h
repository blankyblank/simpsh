#ifndef JOB_H
#define JOB_H
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

typedef struct job job;
struct job {
  pid_t pgid;          /* process group */
  pid_t status_pid;    /* which pid's exit determines job exit status */
  pid_t saved_ttypgrp; /* saved on stop (valid when JSAVEDTTYPGRP) */
  int wstatus;         /* wait status of status_pid */
  int lwstatus; /* wait status of for chld on left (pipefail needs this) */
  short nlive;  /* # of processes still running (not yet exited) */
  unsigned char state;     /* JRUN, JSTP, JDONE */
  unsigned char flags;     /* JCHANGED, JFG, JSAVEDTTY, JSAVEDTTYPGRP */
  int num;                 /* job number */
  int age;                 /* monotonic counter for current-job selection */
  job *next;               /* next job */
  char *cmd;               /* command string */
  struct termios ttystate; /* saved on stop (valid when JSAVEDTTY) */
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

#define init_pgrp() sh_pgid = getpgrp()

/* return term back to process group */
#define ttyreclaim() tcsetpgrp(tty_fd, sh_pgid)

/* return term back to shell, reseting attributes */
#define ttyrestore() \
  (tcsetpgrp(tty_fd, sh_pgid), tcsetattr(tty_fd, TCSADRAIN, &sh_termios))

#endif /* JOB_H */
