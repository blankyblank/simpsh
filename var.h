#ifndef VAR_H
#define VAR_H
#include <stddef.h>
#include <string.h>
#include <sys/types.h>

#include "main.h"

typedef int shvar_flags;
typedef struct shvar shvar;
struct shvar {
  char *var;                  /* Name=value */
  shvar_flags flags;          /* VEXPRT | VREADONLY | VUNSET */
  u16 nlen;                   /* name lenght */
  unsigned int flen;          /* full length */
  void (*func)(const char *); /* callback func */
};

typedef struct tmp_var {
  char *name;
  char *val;
  shvar_flags oldflags;
  int set; /* was it already set? */
} tmp_var;

/* shell variables */
#define VAR_BUCKETS_INIT 128
#define LOCAL_MAX 256
#define VAR_CACHE_S 64

typedef struct {
  const char *text; /* "NAME=default" */
  shvar_flags flags;
  void (*func)(const char *);
} varinit;

typedef struct  {
  shvar *vartab; /* variable table */
  size_t vartab_size; /* variable table size */
  unsigned int varcnt; /* variable count */
  shvar *varcache[VAR_CACHE_S]; /* cache for recently accessed vars */
  tmp_var localvars[LOCAL_MAX] /* local variables */;
  unsigned int localcnt;
  u8 ifsnull;
  char ifsv[64];
  size_t ifsvlen;
  char *home;
  size_t homelen;
  char *pid_s; /* the shell's pid */
  char *ppid_s; /* the shell's ppid */
  char *bgpid_s;  /* the last background processes pid */
  shvar linenov;
  char linebuf[16];
} GVAR;

enum {
  VEXPRT = 1 << 0,
  VREADONLY = 1 << 1,
  VUNSET = 1 << 2,
  VNOCB = 1 << 3,
};

#define TOMBSTONE    ((char *)1)
#define VARTAB       (gvar.vartab)
#define VARTAB_SIZE  (gvar.vartab_size)
#define VARCNT       (gvar.varcnt)
#define VARCACHE     (gvar.varcache)
#define LOCALVARS    (gvar.localvars)
#define LOCALCNT     (gvar.localcnt)
#define LINENO       (gvar.linenov)

#define shvar_val(v) ((v)->var + (v)->nlen + 1)
#define vallen(v)    ((v)->flen - (v)->nlen - 2)
#define findvar(v)   findvar_n(v, strlen(v))

extern const char oinn[16];
extern const char oargn[16];
extern const char oerrn[16];

extern GVAR gvar;
extern char **build_env(char **);
extern void init_env(void);
extern void setvar(const char * restrict, const char * restrict, shvar_flags);
extern tmp_var grabvar(char *);
extern shvar *findvar_n(const char *restrict, size_t);
extern void rmvar(const char *);
extern void printvars(const char *,shvar_flags);

/* builtins */
extern int exportcmd(char **);
extern int localcmd(char **);
extern int readonlycmd(char **);
extern int unsetcmd(char **);

static inline char *
getvar(const char *vt)
{
  char *var = NULL;
  shvar *v;
  if ((v = findvar(vt)))
    var = shvar_val(v);
  return var;
}

#endif /* VAR_H */

