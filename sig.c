#define _POSIX_C_SOURCE 200809L
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
volatile sig_atomic_t neednotify = 0;
sigset_t emptyset;
sigset_t sigchldmask;

static void dosighup(int);
static void dosigterm(int);
static void dosigchld(int);
static void dosigint(int);

void
init_sig(void)
{
  signal(SIGINT, dosigint);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGHUP, dosighup);
  signal(SIGTERM, dosigterm);
  signal(SIGCHLD, dosigchld);
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
  tcgetattr(STDIN_FILENO, &sh_termios);
  tcsetattr(STDIN_FILENO, TCSADRAIN, &sh_termios);
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
  if (mflag)
    killjob();
  if (bflag)
    neednotify = 1;
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
