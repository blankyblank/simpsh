#ifndef SIG_H
#define SIG_H
#define _POSIX_C_SOURCE 200809L
#include <setjmp.h>
#include <signal.h>

extern volatile sig_atomic_t intsig;
extern volatile sig_atomic_t neednotify;
extern sigset_t emptyset;
extern sigset_t sigchldmask;
extern sigjmp_buf linejmp;

typedef void (*sighandler_t)(int);
sighandler_t __signal(int sig, sighandler_t handler);
void init_sig(void);
void init_job(void);

#define signal(sig, handler) __signal(sig, handler)

#endif /* SIG_H */
