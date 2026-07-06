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
  shvar *var_tab; /* variable table */
  size_t var_tab_size; /* variable table size */
  unsigned int var_cnt; /* variable count */
  shvar *var_cache[VAR_CACHE_S]; /* cache for recently accessed vars */
  tmp_var local_vars[LOCAL_MAX] /* local variables */;
  unsigned int local_cnt;
  int sh_line_no;
  pid_t sh_bgpid;
  char *sh_pid_str; /* the shell's pid */
  char *sh_ppid_str; /* the shell's ppid */
  char *sh_bgpid_str;  /* the last background processes pid */
  char *homevar;
  size_t homevarlen;
  char **sh_argv;
  int sh_argc;
  char *sh_argv0;
  u8 alloced_sh_argv;
  u8 ifs_null;
  int sh_optind;
  int sh_optoff;
  shvar line_no_var;
  char line_buf[256];
} GVAR;

enum {
  VEXPRT = 1 << 0,
  VREADONLY = 1 << 1,
  VUNSET = 1 << 2,
  VNOCB = 1 << 3,
};

#define TOMBSTONE ((char *)1)
#define vartab        (gvar.var_tab)
#define vartab_size   (gvar.var_tab_size)
#define varcnt        (gvar.var_cnt)
#define varcache      (gvar.var_cache)
#define localvars     (gvar.local_vars)
#define localcnt       (gvar.local_cnt)
#define sh_lineno     (gvar.sh_line_no)
#define shbgpid       (gvar.sh_bgpid)
#define sh_pid_s      (gvar.sh_pid_str)
#define sh_ppid_s     (gvar.sh_ppid_str)
#define sh_bgpid_s    (gvar.sh_bgpid_str)
#define home          (gvar.homevar)
#define homelen       (gvar.homevarlen)
#define shargv        (gvar.sh_argv)
#define shargc        (gvar.sh_argc)
#define shargv0       (gvar.sh_argv0)
#define alloc_shargv (gvar.alloced_sh_argv)
#define ifsnull       (gvar.ifs_null)
#define optind        (gvar.sh_optind)
#define optoff        (gvar.sh_optoff)
#define linevar       (gvar.line_no_var)
#define linebuf       (gvar.line_buf)
#define shvar_val(v) ((v)->var + (v)->nlen + 1)
#define vallen(v) ((v)->flen - (v)->nlen - 1)
#define findvar(v) findvar_n(v, strlen(v))

extern GVAR gvar;
extern char **build_env(char **);
extern void init_env(void);
extern void setvar(const char * restrict, const char * restrict, shvar_flags);
extern tmp_var grabvar(char *);
extern shvar *findvar_n(const char *restrict, size_t);
extern void rmvar(const char *);

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

