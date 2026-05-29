#ifndef MAIN_H
#define MAIN_H

/* #define _XOPEN_SOURCE 700 */
#define _POSIX_C_SOURCE 200809L
#ifdef HAVE_PATHS_H
  #include <paths.h>
#endif
#ifdef ENABLE_VALGRIND
  #include <valgrind/cachegrind.h>
  #include <valgrind/memcheck.h>
#endif /* ifdef ENABLE_VALGRIND */

#include <unistd.h>

#define MAX_ENV 256
#define defpath "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

#define FLAG_c (1 << 0)
#define FLAG_i (1 << 1)
#define FLAG_N (1 << 2)
#define FLAG_L (1 << 3)
#define FLAG_P (1 << 4)
#define FLAG_V (1 << 5)

#define usage() fprintf(stderr, "Usage: simpsh [-abCefhiImnosvVx] [-o longopt] [-c 'cmd']\n")

/* shell variables */
extern char **environ;
extern char *sh_argv0;
extern char **sh_argv;
extern int alloc_sh_argv;
extern char *shps1;
extern char *shps2;
// extern char *shps3;  // TODO: bring back when i implement select
extern char *shps4;
extern int sh_argc;
extern int lstatus;
extern pid_t sh_pid;
extern char *sh_pid_s;
extern char *home;

extern char histfile[256];
#endif /* !MAIN_H */
