#ifndef VAR_H
#define VAR_H


#include <stddef.h>
#include "utils.h"
typedef int shvar_flags;
typedef struct shvar shvar;
struct shvar {
  shvar *next;
  shvar_flags flags;
  char *var;
  void (*func)(const char *);
};

typedef struct tmp_var {
  char *name;
  char *val;
  shvar_flags oldflags;
  int set;
} tmp_var;

#define VEXPRT    (1 << 0)
#define VREADONLY (1 << 1)
#define VUNSET    (1 << 2)
#define VAR_BUCKETS 64
#define LOCAL_MAX 256

#define shvar_val(v) (strchrnul_(v->var, '=') + 1)
#define shvar_namelen(v) (strchrnul_(v, '=') - v)

extern shvar *var_tab[VAR_BUCKETS];
extern tmp_var localvars[LOCAL_MAX];
extern size_t localsp;

extern char **build_env(char **);
extern void init_env(void);
extern void setvar(char *, char *, shvar_flags);
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

