#ifndef VAR_H
#define VAR_H
#include <stddef.h>
#include <string.h>

typedef int shvar_flags;
typedef struct shvar shvar;
struct shvar {
  char *var;                  /* Name=value */
  size_t nlen;                /* name lenght */
  size_t flen;                /* full length */
  shvar_flags flags;          /* VEXPRT | VREADONLY | VUNSET */
  void (*func)(const char *); /* callback func */
};

typedef struct tmp_var {
  char *name;
  char *val;
  shvar_flags oldflags;
  int set; /* was it already set? */
} tmp_var;

enum {
  VEXPRT = 1 << 0,
  VREADONLY = 1 << 1,
  VUNSET = 1 << 2,
};

#define VAR_BUCKETS_INIT 128
#define LOCAL_MAX 256
#define TOMBSTONE ((char *)1)
#define VAR_CACHE_S 64

#define shvar_val(v) ((v)->var + (v)->nlen + 1)
#define vallen(v) ((v)->flen - (v)->nlen - 1)
#define findvar(v) findvar_n(v, strlen(v))

extern shvar *var_tab;
extern size_t var_tab_size;
extern size_t var_count;
extern shvar *var_cache[VAR_CACHE_S];
extern tmp_var localvars[LOCAL_MAX];
extern size_t localsp;

extern char **build_env(char **);
extern void init_env(void);
extern void setvar(const char * restrict, char * restrict, shvar_flags);
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

