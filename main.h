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

#define MAX_CMDS 256
#define MAX_LENGTH 256

/* shell variables */
extern char **environ;
extern char *progname;
extern char *sh_argv0;
extern char **sh_argv;
extern int sh_argc;
extern int lstatus;
extern pid_t sh_pid;
extern char *sh_pid_s;
extern char *home;

#endif /* !MAIN_H */
