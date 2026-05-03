/* env.h - declarations surrounding various parts of the shell environment */
#ifndef VAR_H
#define VAR_H

#define _POSIX_C_SOURCE 200809L
#include "simpsh.h"
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
// #include <string.h>

typedef struct alias alias;
struct alias {
  char *name;
  char *value;
  alias *next;
};
typedef struct {
  int exported;
  int readonly;
} shvar_flags;

#define EXPRT { .exported = 1, .readonly = 0 }
/* typedef struct shvar shvar;
struct shvar {
  shvar *next;
  char *name;
  char *value;
  shvar_flag flags;
}; */

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

// TODO: write helper functions to return name, and  one for value value

static inline char *
shvar_val(const shvar *v)
{
  return s_strchrnul(v->var, '=') + 1;
}

static inline size_t
shvar_namelen(char *var)
{
  return s_strchrnul(var, '=') - var;
}

#endif /* VAR_H */
