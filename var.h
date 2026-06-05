#ifndef VAR_H
#define VAR_H
#include <stddef.h>

typedef int shvar_flags;
typedef struct shvar shvar;
struct shvar {
  char *var;
  size_t nlen;
  shvar_flags flags;
  void (*func)(const char *);
};

typedef struct tmp_var {
  char *name;
  char *val;
  shvar_flags oldflags;
  int set;
} tmp_var;

enum {
  VEXPRT = 1 << 0,
  VREADONLY = 1 << 1,
  VUNSET = 1 << 2,
};

#define VAR_BUCKETS_INIT 128
#define LOCAL_MAX 256
#define TOMBSTONE ((char *)1)
#define VAR_CACHE_S 16

#define shvar_val(v) ((v)->var + (v)->nlen + 1)
// XXX: probably just remove the namelen macro
#define shvar_namelen(v) ((v)->nlen)

extern shvar *var_tab;
extern size_t var_tab_size;
extern size_t var_count;
extern shvar *var_cache[VAR_CACHE_S];
extern tmp_var localvars[LOCAL_MAX];
extern size_t localsp;

extern char **build_env(char **);
extern void init_env(void);
extern void setvar(char * restrict, char * restrict, shvar_flags);
extern tmp_var grabvar(char *);
extern shvar *findvar(const char *);
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

