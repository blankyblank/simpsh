#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include "alloc.h"
#include "error.h"
#include "job.h"
#include "sig.h"
#include "simpsh.h"
#include "utils.h"

volatile sig_atomic_t intsig;
volatile sig_atomic_t ndnotify = 0;
volatile sig_atomic_t chksig[NSIG];
volatile sig_atomic_t fchksig = 0;

const char *signame[NSIG + 1];
eventloop el;
static unsigned long trapm = 0;
int tty_fd = -1;
int selfpipe[2] = { -1, -1 };
int intpipe[2] = { -1, -1 };

static void restoreterm(void) { tcsetattr(tty_fd, TCSADRAIN, &sh_termios); }

void
init_sig(void)
{
  if (pipe(selfpipe) == 0) {
    fcntl(selfpipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(selfpipe[0], F_SETFL, fcntl(selfpipe[0], F_GETFL) | O_NONBLOCK);
    fcntl(selfpipe[1], F_SETFD, FD_CLOEXEC);
    fcntl(selfpipe[1], F_SETFL, fcntl(selfpipe[1], F_GETFL) | O_NONBLOCK);
  }
  if (pipe(intpipe) == 0) {
    fcntl(intpipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(intpipe[0], F_SETFL, fcntl(intpipe[0], F_GETFL) | O_NONBLOCK);
    fcntl(intpipe[1], F_SETFD, FD_CLOEXEC);
    fcntl(intpipe[1], F_SETFL, fcntl(intpipe[1], F_GETFL) | O_NONBLOCK);
  }

  init_eventloop(&el);
  addeventloop(&el, selfpipe[0], POLLIN, chld_cb, NULL);
  addeventloop(&el, intpipe[0], POLLIN, int_cb, NULL);

  init_traps();
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
  atexit(restoreterm);
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

int
addeventloop(eventloop *el, int fd, short events,void (*cb)(void *), void *data)
{
  if (el->nsrc >= EVMAX)
    return -1;
  el->pfds[el->nsrc].fd = fd;
  el->pfds[el->nsrc].events = events;
  el->srcs[el->nsrc].fd = fd;
  el->srcs[el->nsrc].events = events;
  el->srcs[el->nsrc].cb = cb;
  el->srcs[el->nsrc].data = data;
  el->nsrc++;
  return 0;
}

int
rmeventloop(eventloop *el, int fd)
{
  for (int i = 0; i < el->nsrc; i++) {
    if (el->srcs[i].fd == fd) {
      el->nsrc--;
      el->srcs[i] = el->srcs[el->nsrc];
      el->pfds[i] = el->pfds[el->nsrc];
      return 0;
    }
  }
  return -1;
}

int
runeventloop(eventloop *el, int cont)
{
  int n = poll(el->pfds, el->nsrc, cont);
  if (n < 0) {
    if (errno == EINTR)
      return 0;
    return -1;
  }
    for (int i = 0; i < el->nsrc; i++) {
      if (el->pfds[i].revents & el->srcs[i].events)
        el->srcs[i].cb(el->srcs[i].data);
    }
  return 0;
}

static char signum[NSIG][8];
static char *trap[NSIG];                        /* trap command strings */
static int sigmode[NSIG];                       /* S_DFL, S_CATCH, S_IGN, S_HARD_IGN */

static void setsignal(int);

int
init_traps(void)
{
  signame[0] = "EXIT";

  for (size_t i = 0; i < NSIG; i++) {
    itoa(i, signum[i]);
    signame[i] = signum[i];
  }

  signame[SIGHUP] = "HUP";
  signame[SIGINT] = "INT";
  signame[SIGQUIT] = "QUIT";
  signame[SIGILL] = "ILL";
  signame[SIGTRAP] = "TRAP";
  signame[SIGABRT] = "ABRT";
  signame[SIGBUS] = "BUS";
  signame[SIGFPE] = "FPE";
  signame[SIGKILL] = "KILL";
  signame[SIGUSR1] = "USR1";
  signame[SIGSEGV] = "SEGV";
  signame[SIGUSR2] = "USR2";
  signame[SIGPIPE] = "PIPE";
  signame[SIGALRM] = "ALRM";
  signame[SIGTERM] = "TERM";
  signame[SIGCHLD] = "CHLD";
  signame[SIGCONT] = "CONT";
  signame[SIGSTOP] = "STOP";
  signame[SIGTSTP] = "TSTP";
  signame[SIGTTIN] = "TTIN";
  signame[SIGTTOU] = "TTOU";
  signame[SIGURG] = "URG";
  signame[SIGXCPU] = "XCPU";
  signame[SIGXFSZ] = "XFSZ";
  signame[SIGVTALRM] = "VTALRM";
  signame[SIGPROF] = "PROF";
  signame[SIGWINCH] = "WINCH";
  signame[SIGIO] = "IO";
  signame[SIGPWR] = "PWR";
  signame[SIGSYS] = "SYS";
#ifdef SIGSTKFLT
  signame[SIGSTKFLT] = "STKFLT";
#endif
  
  for (size_t s = 1; s < NSIG; s++) {
  struct sigaction old;
    if (sigaction(s, NULL, &old) < 0)
      continue;
    if (old.sa_handler == SIG_IGN)
      sigmode[s] = S_HIGN;
    else 
      sigmode[s] = S_DFL;
  }
  setsignal(SIGCHLD);
  if (iflag) {
    setsignal(SIGINT);
    setsignal(SIGHUP);
    setsignal(SIGTERM);
    setsignal(SIGQUIT);
  }
  return 1;
}

static void
setsignal(int n)
{
  int set;
  struct sigaction sa;

  if (sigmode[n] == S_HIGN)
    return;
  if (!trap[n]) {
    switch (n) {
      case SIGCHLD:
        set = S_CATCH;
        break;
      case SIGINT:
      case SIGTERM:
      case SIGHUP:
        set = iflag ? S_CATCH : S_DFL;
        break;
      case SIGQUIT:
        set = iflag ? S_IGN : S_DFL;
        break;
      default:
        set = S_DFL;
        break;
    }
  } else if (trap[n][0] == '\0') {
    set = S_IGN;
  } else {
    set = S_CATCH;
  }
  if (set == sigmode[n])
    return;

  sa.sa_handler = set == S_CATCH ? trapsig : set == S_IGN ? SIG_IGN : SIG_DFL;
  sigfillset(&sa.sa_mask);
  sa.sa_flags = (n == SIGCHLD) ? SA_RESTART : 0;
  if (sigaction(n, &sa, NULL) < 0)
    return;
  sigmode[n] = set;
}

void
trapsig(int n)
{
  chksig[n] = 1;
  fchksig = n;
  if (trap[n]) {
    if (n == SIGCHLD)
      write(selfpipe[1], "\1", 1);
    if (n == SIGINT)
      write(intpipe[1], "\1", 1);
    return;
  } else {
    switch (n) {
      case SIGCHLD:
        write(selfpipe[1], "\1", 1);
        ndnotify = 1;
        break;
      case SIGINT:
        write(intpipe[1], "\1", 1);
        intsig = 1;
        break;
      case SIGTERM:
        _exit(0);
        break;
      case SIGHUP:
        for (job *j = job_list; j; j = j->next)
          kill(-j->pgid, SIGHUP);
        _exit(0);
        break;
    }
  }
}

int
getsig(const char *sig)
{
  int nnum = 0, n = -1;


  if ((sig[0] == '-' && sig[1] != '\0') || isdigit_(sig[0])) {
    for (size_t i = 1; sig[i]; i++)
      if (!isdigit_(sig[i])) {
        nnum = 1;
        break;
      }
    if (!nnum) {
      n = atoi(sig);
      if (n >= 0 && n < NSIG) {
        return n;
      } else {
        return -1;
      }
    }
  } else if (strncasecmp(sig, "SIG", 3) == 0) {
    sig += 3;
  }
  for (size_t i = 0; i < NSIG; i++) {
    if (strcasecmp(sig, signame[i]) == 0)
      return i;
  }
  return -1;
}

void
dotrap(void)
{
  int sstatus;

  sstatus = lstatus;
  fchksig = 0;
  for (size_t i = 0; i < NSIG; i++) {
    if (!chksig[i])
      continue;
    chksig[i] = 0;
    if (trap[i] && trap[i][0]) {
      sh_ccmd(trap[i]);
    }
  }
  lstatus = sstatus;
}

void
cleartraps(void)
{
  unsigned long tm = trapm;
  if (!tm)
    return;
  trapm = 0;

  struct sigaction dfl;
  dfl.sa_handler = SIG_DFL;
  sigemptyset(&dfl.sa_mask);
  dfl.sa_flags = 0;

  while (tm) {
    int i = __builtin_ctzll(tm);
    tm &= tm - i;
    slfree(trap[i]);
    trap[i] = NULL;
    if (i && sigmode[i] != S_HIGN)
      sigaction(i, &dfl, NULL);
  }
}

void
exittrap(int status)
{
  if (trap[0] && trap[0][0]) {
    char *cmd = trap[0];
    trap[0] = NULL;
    trapm &= ~1ULL;
    sh_ccmd(cmd);
  }
  exit(status);
}

int
trapcmd(char **argv)
{
  size_t s = 0, argc = 0;
  int sig;
  char *act, *argv0;

  array_len(argv, argc);
  argc--;
  argv0 = argv[0];
  argv++;

  if (!argc) {
    for (size_t i = 0; i < NSIG; i++) {
      if (trap[i]) {
        printf("%s -- '%s' %s\n", argv0, trap[i], signame[i]);
      }
    }
    return 0;
  } else {
    if ((sig = getsig(*argv)) < 0) {
      act = *argv;
      s = 1;
    } else {
      act = NULL;
      s = 0;
    }

    for (size_t i = s; i < argc; i++) {
      int n = 0;
      if ((n = getsig(argv[i])) < 0) {
        shwarn_arg(argv0, argv[i], "bad trap");
        return 1;
      } else {
        if (strcmp(signame[n], "KILL") == 0 ||
            strcmp(signame[n], "STOP") == 0)
          continue;

        if (trap[n])
          slfree(trap[n]), trapm &= ~(1ULL << n);
        if (!act || (act[0] == '-' && act[1] == '\0'))
          trap[n] = NULL, trapm &= ~(1ULL << n);
        else if (act[0] == '\0')
          trap[n] = strdup_(""), trapm |= 1ULL << n;
        else
          trap[n] = strdup_(act), trapm |= 1ULL << n;
        if (n)
          setsignal(n);
      }
    }
  }
  return 0;
}
