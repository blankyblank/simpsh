/* env.h - declarations surrounding various parts of the shell environment */
#ifndef VAR_H
#define VAR_H

#define _POSIX_C_SOURCE 200809L
#include "simpsh.h"
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

typedef struct alias alias;
struct alias {
  char *name;
  char *value;
  alias *next;
};

typedef int shvar_flags;
#define VEXPRT    (1 << 0)
#define VREADONLY (1 << 1)
#define VUNSET    (1 << V)

typedef struct shvar shvar;
struct shvar {
  shvar *next;
  shvar_flags flags;
  char *var;
  void (*func)(const char *);
};

#define ENV_BUCKETS 39
#define MAX_ALIAS_DEPTH 10
extern shvar *var_tab[ENV_BUCKETS];
extern alias *alias_tab[ENV_BUCKETS];

extern void init_env(void);
extern char **build_env(char **);
extern char *exp_var(char *, size_t, size_t *);
extern alias *find_alias(const char *);
extern void set_alias(const char *, const char *);
extern void rm_alias(const char *);
extern void setvar(char *, char *, shvar_flags);
extern void unset_var(const char *);
extern shvar *find_var(const char *);

static inline char *
shvar_val(const shvar *v)
{
  return s_strchrnul(v->var, '=') + 1;
}

static inline char *
getvar(const char *vt)
{
  char *var = NULL;
  shvar *v;
  if ((v = find_var(vt)))
    var = shvar_val(v);
  return var;
}

static inline size_t
shvar_namelen(char *var)
{
  return s_strchrnul(var, '=') - var;
}

#endif /* VAR_H */
