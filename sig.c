#define _POSIX_C_SOURCE 200809L
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include "sig.h"
 
sigjmp_buf shenv;
static void sigintlj(int);
static void dosighup(int);
static void dosigterm(int);

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

void
init_sig(void)
{
  signal(SIGINT, sigintlj);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGHUP, dosighup);
  signal(SIGTERM, dosigterm);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);
  signal(SIGCHLD, SIG_DFL);
}

static void
dosighup(int _)
{
    (void)_;
    _exit(0);
}

static void
dosigterm(int _)
{
    (void)_;
    _exit(0);
}

void
sigintlj(int _)
{
  (void)_;
  siglongjmp(shenv, 1);
}

