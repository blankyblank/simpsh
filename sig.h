#ifndef SIG_H
#define SIG_H
/* clang-format off */
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <unistd.h>
#include <sys/poll.h>

#include "opts.h"
#include "job.h"

#define EVMAX 16
#ifndef NSIG
#define NSIG 64
#endif /* NSIG */
#define signal(sig, handler) __signal(sig, handler)
#define init_eventloop(e) ((e)->nsrc = 0, (e)->running = 1)
#define stopeventloop(e) ((e)->running = 0)
#define runeventloop_cont(el) do { while((el)->running) runeventloop(el, -1); } while (0)
/* clang-format on */

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

extern eventloop el;
extern volatile sig_atomic_t intsig;
extern volatile sig_atomic_t ndnotify;
extern int tty_fd;
extern int selfpipe[2];
extern int intpipe[2];

extern volatile sig_atomic_t chksig[NSIG];
extern volatile sig_atomic_t fchksig;
extern const char *signame[NSIG + 1];

typedef void (*sighandler_t)(int);
sighandler_t __signal(int sig, sighandler_t handler);
void init_sig(void);
void init_job(void);
extern int addeventloop(eventloop *, int, short,void (*)(void *), void *);
extern int rmeventloop(eventloop *, int);
extern int runeventloop(eventloop *, int);

int init_traps(void);
void exittrap(int) __attribute__((__noreturn__));
void dotrap(void);
void trapsig(int);
void cleartraps(void);
void setsig(int);
int getsig(const char *);
int trapcmd(char **);


static inline void
drain_chldp(void)
{
  char buf[64];
  while (read(selfpipe[0], buf, sizeof(buf)) > 0);
}

static inline void
drain_intp(void)
{
  char buf[64];
  while (read(intpipe[0], buf, sizeof(buf)) > 0);
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
  drain_intp();
}

#endif /* SIG_H */
