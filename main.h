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

enum {
  FLAG_c = 1 << 0,
  FLAG_i = 1 << 1,
  FLAG_N = 1 << 2,
  FLAG_L = 1 << 3,
  FLAG_P = 1 << 4,
  FLAG_V = 1 << 5,
  FLAG_r = 1 << 6,
};

#define usage() fprintf(stderr, "Usage: simpsh [-abCefhiImnosvVx] [-o longopt] [-c 'cmd']\n")

/* shell variables */
extern char **environ;
extern char *sh_argv0; /* the shells first arguement */
extern char **sh_argv; /* shell arguement array */
extern char *sh_ps1; /* prompt string */
extern char *sh_ps2; /* continuation prompt string */
extern char *sh_ps4;
extern int sh_argc; /* shell arg count */
extern int lstatus; /* last exit status */
extern pid_t sh_pid; /* the shell's pid */
extern pid_t sh_bgpid; /* the last background processes pid */
extern char *sh_pid_s;
extern char *sh_bgpid_s;
extern int alloc_sh_argv;
extern char *home;

extern char histfile[256];
#endif /* !MAIN_H */
