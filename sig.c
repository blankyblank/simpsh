#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job.h"
#include "opts.h"
#include "sig.h"
#include "simpsh.h"

sigjmp_buf linejmp;
volatile sig_atomic_t intsig;
volatile sig_atomic_t ndnotify = 0;
volatile sig_atomic_t ndreap = 0;
sigset_t emptyset;
sigset_t sigchldmask;
int tty_fd = -1;

static void dosighup(int);
static void dosigterm(int);
static void dosigchld(int);
static void dosigint(int);

void
init_sig(void)
{
  {
    struct sigaction sa;
    sa.sa_handler = dosigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
  }
  signal(SIGINT, dosigint);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGHUP, dosighup);
  signal(SIGTERM, dosigterm);
  sigemptyset(&emptyset);
  sigemptyset(&sigchldmask);
  sigaddset(&sigchldmask, SIGCHLD);
}

void
init_job(void)
{
  signal(SIGTSTP, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);

  if ((tty_fd = open("/dev/tty", O_RDWR)) < 0)
    tty_fd = STDIN_FILENO;
  else {
    int nfd = fcntl(tty_fd, F_DUPFD, 10);
    if (nfd >= 0) {
      close(tty_fd);
      tty_fd = nfd;
    }
    fcntl(tty_fd, F_SETFD, FD_CLOEXEC);
  }

  tcgetattr(tty_fd, &sh_termios);
  tcsetattr(tty_fd, TCSADRAIN, &sh_termios);
  sh_pgid = getpgrp();  //
}

sighandler_t
__signal(int sig, sighandler_t handler)
{
  struct sigaction sa, old;
  sa.sa_handler = handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(sig, &sa, &old) < 0)
    return SIG_ERR;
  return old.sa_handler;
}

static void
dosigchld(int _)
{
  (void)_;
  ndreap = 1;
  if (bflag)
    ndnotify = 1;
}

void
dosigint(int _)
{
  (void)_;
  intsig = 1;
  siglongjmp(linejmp, 1);
}

static void
dosighup(int _)
{
  (void)_;
  job *j;
  for (j = job_list; j; j = j->next)
    kill(-j->pgid, SIGHUP);
  _exit(0);
}

static void
dosigterm(int _)
{
  (void)_;
  _exit(0);
}
