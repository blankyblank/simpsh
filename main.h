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

/* the current shell state within a given context */
struct stackframe {
  char **argv;
  char *argv0;
  int argc;
  int alloced;
  int loop_depth;
  int loop_break;
  int loop_cont;
  int ret_now;
  int ret_val;
  int l_status;
  int opt_ind;
  int opt_off;
  int lineno;
};

typedef struct {
  int l_status; /* last exit status */
  int ret_val; /* value from 'return n' */
  int ret_now; /* if set return from func or . file */
  int loop_depth; /* current loop nesting depth */
  int loopbreak; /* remaining break depth */
  int loopcontinue; /* remaining continue depth */
  int nounseterr;
  struct {
    char **argv;
    char *argv0;
    int argc;
    int alloced;
    int opt_ind;
    int opt_off;
  } shparm;
  int lineno;
  int bgpgid;
  int funcdepth;
  char shopts[OPTC];
  struct stackframe stackframes[64];
  int stackdepth;
} GSTATE;

extern const char defpathn[80];
extern GSTATE gstate;
extern const char shname[];
extern const char shusg[43];
extern char **environ;
extern char histfile[PATH_MAX];

#define LSTATUS     (gstate.l_status)
#define RETVAL      (gstate.ret_val)
#define RETNOW      (gstate.ret_now)
#define LOOPDEPTH   (gstate.loop_depth)
#define SHOPTS      (gstate.shopts)
#define SHARGV        (gstate.shparm.argv)
#define SHARGC        (gstate.shparm.argc)
#define SHARGV0       (gstate.shparm.argv0)
#define ALLOCED     (gstate.shparm.alloced)
#define OPTIND      (gstate.shparm.opt_ind)
#define OPTOFF      (gstate.shparm.opt_off)

static inline void
pushframe(void)
{
  struct stackframe *f = &gstate.stackframes[++gstate.stackdepth];
  f->argv = SHARGV;
  f->argv0 = SHARGV0;
  f->argc = SHARGC;
  f->alloced = ALLOCED;
  f->loop_depth = LOOPDEPTH;
  f->loop_break = gstate.loopbreak;
  f->loop_cont = gstate.loopcontinue;
  f->ret_now = RETNOW;
  f->ret_val = RETVAL;
  f->l_status = LSTATUS;
  f->opt_ind = OPTIND;
  f->opt_off = OPTOFF;
  f->lineno = gstate.lineno;
}

static inline void
popframe(void)
{
  struct stackframe *f = &gstate.stackframes[gstate.stackdepth--];
  SHARGV = f->argv;
  SHARGV0 = f->argv0;
  SHARGC = f->argc;
  ALLOCED = f->alloced;
  LOOPDEPTH = f->loop_depth;
  gstate.loopbreak = f->loop_break;
  gstate.loopcontinue = f->loop_cont;
  RETNOW = f->ret_now;
  RETVAL = f->ret_val;
  LSTATUS = f->l_status;
  OPTIND = f->opt_ind;
  OPTOFF = f->opt_off;
  gstate.lineno = f->lineno;
}

#endif /* !MAIN_H */
