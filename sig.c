#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job.h"
#include "sig.h"
#include "simpsh.h"

volatile sig_atomic_t intsig;
volatile sig_atomic_t ndnotify = 0;
eventloop el;
int tty_fd = -1;
int selfpipe[2] = { -1, -1 };
int intpipe[2] = { -1, -1 };

static void dosighup(int);
static void dosigterm(int);
static void dosigchld(int);
static void dosigint(int);
static void restoreterm(void) { tcsetattr(tty_fd, TCSADRAIN, &sh_termios); }

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

static void
dosigchld(int _)
{
  (void)_;
  write(selfpipe[1], "\1", 1);
  ndnotify = 1;
}

void
dosigint(int _)
{
  (void)_;
  write(intpipe[1], "\1", 1);
  intsig = 1;
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
  return -1; // XXX: ???
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

