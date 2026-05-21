#ifndef SIG_H
#define SIG_H
#define _POSIX_C_SOURCE 200809L
#include <setjmp.h>

extern sigjmp_buf shenv;

typedef void (*sighandler_t)(int);
sighandler_t __signal(int sig, sighandler_t handler);
void init_sig(void);

#define signal(sig, handler) __signal(sig, handler)

#endif /* SIG_H */
