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
extern int retval; /* return builtins value */
extern int retnow; /* if set return from func or . file */
extern pid_t sh_pid;
extern pid_t sh_ppid;
extern pid_t sh_bgpid;
extern char *sh_pid_s; /* the shell's pid */
extern char *sh_ppid_s; /* the shell's ppid */
extern char *sh_bgpid_s; /* the last background processes pid */
extern int alloc_sh_argv; /* if sh_argv was alloced */
extern char *home;
extern size_t homelen;

extern char histfile[256];
#endif /* !MAIN_H */
