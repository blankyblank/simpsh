#ifndef MAIN_H
#define MAIN_H

/* #define _XOPEN_SOURCE 700 */
#include <stdint.h>
#define _POSIX_C_SOURCE 200809L
#ifdef HAVE_PATHS_H
  #include <paths.h>
#endif
#ifdef ENABLE_VALGRIND
  #include <valgrind/cachegrind.h>
  #include <valgrind/memcheck.h>
#endif /* ifdef ENABLE_VALGRIND */

#include <unistd.h>
#include <limits.h>

#define MAX_ENV 500
#define HISTORY_SIZE 1000
#define defpath "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

enum {
  FLAG_c = 1 << 0,
  FLAG_i = 1 << 1,
  FLAG_N = 1 << 2,
  FLAG_L = 1 << 3,
  FLAG_P = 1 << 4,
  FLAG_l = 1 << 3,
  FLAG_p = 1 << 4,
  FLAG_V = 1 << 5,
  FLAG_r = 1 << 6,
  LOGIN = 1 << 7,
};

#define charf int_fast8_t
#define ucharf uint_fast8_t
#define shortf int_fast16_t
#define intf int_fast32_t
#define uintf uint_fast32_t
#define longf int_fast64_t
#define ulongf uint_fast64_t
#define llongf int_fast64_t
#define ullongf uint_fast64_t

extern const char pwdn[16];
extern const char ifsn[16];
extern const char envn[16];
extern const char oldpwdn[16];
extern const char homen[16];
extern const char pathn[16];
extern const char ppidn[16];
extern const char shlvln[16];
extern const char shname[];
extern const char shelln[16];
extern const char cdpthn[16];
extern const char linen[16];
extern const char ps1n[16];
extern const char ps2n[16];

/* shell variables */
extern char **environ;
extern char *sh_argv0; /* the shells first arguement */
extern char **sh_argv; /* shell arguement array */
extern char *sh_prompt; /* prompt string */
extern char *sh_ps1; /* prompt string */
extern char *sh_ps2; /* continuation prompt string */
extern char *sh_ps4;
extern intf sh_argc; /* shell arg count */
extern intf lstatus; /* last exit status */
extern int retval; /* return builtins value */
extern int retnow; /* if set return from func or . file */
extern int loopdepth; /* current loop nesting depth */
extern int loopbreak; /* remaining break depth */
extern int loopcontinue; /* remaining continue depth */
extern intf sh_lineno;
extern pid_t sh_pid;
extern pid_t sh_ppid;
extern pid_t sh_bgpid;
extern char *sh_lineno_s; /* current line number */
extern char *sh_pid_s; /* the shell's pid */
extern char *sh_ppid_s; /* the shell's ppid */
extern char *sh_bgpid_s; /* the last background processes pid */
extern int alloc_sh_argv; /* if sh_argv was alloced */
extern char *home;
extern size_t homelen;

extern char histfile[PATH_MAX];
#endif /* !MAIN_H */
