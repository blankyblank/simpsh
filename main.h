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
#include <sys/types.h>
#include <unistd.h>

#define MAX_ENV      500
#define HISTORY_SIZE 1000
#define defpath      "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
#define STR(s)       ((const char[16]) { s })

typedef int8_t i8;
typedef u_int8_t u8;
typedef int16_t i16;
typedef u_int16_t u16;
typedef int32_t i32;
typedef u_int32_t u32;
typedef int64_t i64;
typedef u_int64_t u64;

enum {
  FLAG_c = 1 << 0,
  FLAG_i = 1 << 1,
  FLAG_N = 1 << 2,
  FLAG_L = 1 << 3,
  FLAG_P = 1 << 4,
  FLAG_l = 1 << 5,
  FLAG_p = 1 << 6,
  FLAG_V = 1 << 7,
  FLAG_v = 1 << 8,
  FLAG_r = 1 << 9,
  FLAG_s = 1 << 10,
  LOGIN = 1 << 11,
};

extern const char shname[];
extern const char shusg[43];

extern char **environ;
extern char *sh_argv0; /* the shells first arguement */
extern char **sh_argv; /* shell arguement array */
extern int sh_argc; /* shell arg count */
extern u8 alloc_sh_argv; /* if sh_argv was alloced */
extern int lstatus; /* last exit status */
extern int retval; /* value from 'return n' */
extern u8 retnow; /* if set return from func or . file */
extern int loopdepth; /* current loop nesting depth */
extern int loopbreak; /* remaining break depth */
extern int loopcontinue; /* remaining continue depth */

extern char histfile[PATH_MAX];
#endif /* !MAIN_H */
