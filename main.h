#ifndef MAIN_H
#define MAIN_H

#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */
#ifdef HAVE_PATHS_H
  #include <paths.h>
#endif
#ifdef ENABLE_VALGRIND
  #include <valgrind/cachegrind.h>
  #include <valgrind/memcheck.h>
#endif /* ifdef ENABLE_VALGRIND */

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "opts.h"

enum {
  FLAG_L = 1 << 0,
  FLAG_N = 1 << 1,
  FLAG_P = 1 << 2,
  FLAG_V = 1 << 3,
  FLAG_c = 1 << 4,
  FLAG_e = 1 << 5,
  FLAG_i = 1 << 6,
  FLAG_l = 1 << 7,
  FLAG_n = 1 << 8,
  FLAG_p = 1 << 9,
  FLAG_r = 1 << 10,
  FLAG_s = 1 << 11,
  FLAG_v = 1 << 12,
  LOGIN = 1 << 13,
};

typedef int8_t i8;
typedef uint8_t u8;
typedef int16_t i16;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;

#define MAX_ENV      500
#define defpath      "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
#define doexpect(x)   __builtin_expect(!!(x), 1)
#define dontexpect(x) __builtin_expect(!!(x), 0)

/* the current shell state within a given context */
struct stackframe {
  char **argv;    /* shell argv */
  char *argv0;    /* shell argv[0] */
  int argc;       /* shell argc */
  int alloced;    /* shargv was alloced */
  int loopdepth; /* loop nesting level */
  int loopbreak; /* pending loop break */
  int loopcont;  /* pending loop continue */
  int retnow;    /* pending shell return */
  int retval;    /* shell returns value */
  int lstatus;   /* the last commands exit status */
  int optind;    /* getopts optind */
  int optoff;    /* getopts optoff */
  int lineno;     /* line number */
};

typedef struct {
  int lstatus;     /* last exit status */
  int retnow;      /* if set return from func or . file */
  int retval;      /* value from 'return n' */
  int loopdepth;   /* current loop nesting depth */
  int loopbreak;    /* remaining break depth */
  int loopcont; /* remaining continue depth */
  int nounseterr;   /* */
  struct {
    char **argv; /* shell argv */
    char *argv0; /* shell argv[0] */
    int argc;    /* shell argc */
    int alloced; /* shargv was alloced */
    int optind; /* getopts optind */
    int optoff; /* getopts optoff */
  } shparm;
  int lineno;     /* line number */
  int bgpgid;     /* last bg command's pgid */
  int funcdepth;  /* shell function nesting level */
  shopt shopts;   /* shell options */
  int stackdepth; /* frame nesting level */
  struct stackframe stackframes[64]; /* current frames */
} GSTATE;

extern const char defpathn[80];
extern GSTATE gstate;
extern const char shusg[43];
extern char **environ;
extern FILE *shin;
extern FILE *shout;

#define LSTATUS   (gstate.lstatus)        /* last exit status */
#define RETVAL    (gstate.retval)         /* value from 'return n' */
#define RETNOW    (gstate.retnow)         /* if set return from func or . file */
#define LOOPDEPTH (gstate.loopdepth)      /* current loop nesting depth */
#define LOOPBREAK (gstate.loopbreak)      /* remaining break depth */
#define LOOPCONT  (gstate.loopcont)       /* pending loop continue */
#define SHOPTS    (gstate.shopts.bits)    /* shell options */
#define SHARGV    (gstate.shparm.argv)    /* shell argv */
#define SHARGV0   (gstate.shparm.argv0)   /* shell argv[0] */
#define SHARGC    (gstate.shparm.argc)    /* shell argc */
#define ALLOCED   (gstate.shparm.alloced) /* shargv was alloced */
#define OPTIND    (gstate.shparm.optind)  /* getopts optind */
#define OPTOFF    (gstate.shparm.optoff)  /* getopts optoff */

static inline void
pushframe(void)
{
  struct stackframe *f = &gstate.stackframes[++gstate.stackdepth];
  f->argv = SHARGV;
  f->argv0 = SHARGV0;
  f->argc = SHARGC;
  f->alloced = ALLOCED;
  f->loopdepth = LOOPDEPTH;
  f->loopbreak = LOOPBREAK;
  f->loopcont = LOOPCONT;
  f->retnow = RETNOW;
  f->retval = RETVAL;
  f->lstatus = LSTATUS;
  f->optind = OPTIND;
  f->optoff = OPTOFF;
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
  LOOPDEPTH = f->loopdepth;
  LOOPBREAK = f->loopbreak;
  LOOPCONT = f->loopcont;
  RETNOW = f->retnow;
  RETVAL = f->retval;
  LSTATUS = f->lstatus;
  OPTIND = f->optind;
  OPTOFF = f->optoff;
  gstate.lineno = f->lineno;
}

#endif /* !MAIN_H */
