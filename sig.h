#ifndef SIG_H
#define SIG_H
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <unistd.h>
#include <poll.h>
#include <setjmp.h>

#include "opts.h"
#include "job.h"

#define EVMAX 16
#ifndef NSIG
  #ifdef _NSIG
    #define NSIG _NSIG
  #else
    #define NSIG 64
  #endif
#endif
#define signal(sig, handler) __signal(sig, handler)

typedef struct {
  int fd;
  short events;
  void (*cb)(void *);
  void *data;
} ev_src;

typedef struct {
  struct pollfd pfds[EVMAX];
  ev_src srcs[EVMAX];
  unsigned char nsrc;
  unsigned char running;
} eventloop;


typedef enum {
  S_DFL = 1,
  S_CATCH,
  S_IGN,
  S_HIGN,
} sig;

typedef struct {
  sigjmp_buf loc;
} jmploc;

extern jmploc *volatile handler;
extern eventloop el;
extern volatile sig_atomic_t intsig;
extern volatile sig_atomic_t ndnotify;
extern int tty_fd;
extern int selfpipe[2];
extern int sigpipe[2];
extern char *trap[NSIG];
extern unsigned long trapm;

extern volatile sig_atomic_t chksig[NSIG];
extern volatile sig_atomic_t fchksig;
extern const char *signame[NSIG + 1];

typedef void (*sighandler_t)(int);
sighandler_t __signal(int sig, sighandler_t handler);
extern void init_sig(void);
extern void init_job(void);
extern void setsignal(int);
extern int addeventloop(eventloop *, int, short,void (*)(void *), void *);
extern int rmeventloop(eventloop *, int);
extern int runeventloop(eventloop *, int);

int init_traps(void);
void exittrap(int) __attribute__((__noreturn__));
void dotrap(void);
void trapsig(int);
void cleartraps(void);
void setsignal(int);
int getsig(const char *);

static inline void
drain_chldp(void)
{
  char buf[64];
  while (read(selfpipe[0], buf, sizeof(buf)) > 0);
}

static inline void
drain_sigp(void)
{
  char buf[64];
  while (read(sigpipe[0], buf, sizeof(buf)) > 0);
}

static inline void
chld_cb(void *data)
{
  (void)data;
  drain_chldp();
  if (bflag) {
    ndnotify = 0;
    jobnotify();
  }
}

static inline void
int_cb(void *data)
{
  (void)data;
  drain_sigp();
}

static inline void
unblocksigs(void)
{
  sigset_t set;
  sigfillset(&set);
  sigprocmask(SIG_UNBLOCK, &set, NULL);
}

#endif /* SIG_H */
