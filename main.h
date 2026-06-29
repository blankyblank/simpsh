#ifndef MAIN_H
#define MAIN_H

#define _POSIX_C_SOURCE 200809L
#ifdef HAVE_PATHS_H
  #include <paths.h>
#endif
#ifdef ENABLE_VALGRIND
  #include <valgrind/cachegrind.h>
  #include <valgrind/memcheck.h>
#endif /* ifdef ENABLE_VALGRIND */

#include <limits.h>
#include <stdint.h>
#include <unistd.h>

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
  FLAG_v = 1 << 6,
  FLAG_r = 1 << 7,
  LOGIN = 1 << 8,
};

typedef int_fast8_t charf;
typedef uint_fast8_t ucharf;
typedef int_fast16_t shortf;
typedef uint_fast16_t ushortf;
typedef int_fast32_t intf;
typedef uint_fast32_t uintf;
typedef int_fast64_t longf;
typedef uint_fast64_t ulongf;
typedef int_fast64_t llongf;
typedef uint_fast64_t ullongf;

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
extern const char ps4n[16];

extern char **environ;
extern char *sh_argv0; /* the shells first arguement */
extern char **sh_argv; /* shell arguement array */
extern int sh_argc; /* shell arg count */
extern ucharf alloc_sh_argv; /* if sh_argv was alloced */
extern int lstatus; /* last exit status */
extern int retval; /* value from 'return n' */
extern ucharf retnow; /* if set return from func or . file */
extern int loopdepth; /* current loop nesting depth */
extern ucharf loopbreak; /* remaining break depth */
extern ucharf loopcontinue; /* remaining continue depth */

extern char histfile[PATH_MAX];
#endif /* !MAIN_H */
