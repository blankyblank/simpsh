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

typedef int8_t i8;
typedef u_int8_t u8;
typedef int16_t i16;
typedef u_int16_t u16;
typedef int32_t i32;
typedef u_int32_t u32;
typedef int64_t i64;
typedef u_int64_t u64;

#define MAX_ENV      500
#define HISTORY_SIZE 1000
#define OPTC 19
#define SHOPTC 16 /* short option count */
#define defpath      "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
#define STR(s)       ((const char[16]) { s })
#define lstatus      (gstate.l_status)
#define retval       (gstate.ret_val)
#define retnow       (gstate.ret_now)
#define loopdepth    (gstate.loop_depth)
#define loopbreak    (gstate.loop_break)
#define loopcontinue (gstate.loop_continue)
#define func_depth   (gstate.shfunc_depth)
#define nounseterr   (gstate.no_unset_err)
#define shopts       (gstate.sh_opts)

/* the current shell state within a given context */
typedef struct {
  int l_status; /* last exit status */
  int ret_val; /* value from 'return n' */
  u8 ret_now; /* if set return from func or . file */
  int loop_depth; /* current loop nesting depth */
  int loop_break; /* remaining break depth */
  int loop_continue; /* remaining continue depth */
  u8 shfunc_depth;
  u8 no_unset_err;
  char sh_opts[OPTC];
} GSTATE;

extern GSTATE gstate;
extern const char shname[];
extern const char shusg[43];
extern char **environ;
extern char histfile[PATH_MAX];

#endif /* !MAIN_H */
