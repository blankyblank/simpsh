/* env.h - declarations surrounding various parts of the shell environment */
#ifndef VAR_H
#define VAR_H

#include "simpsh.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct alias alias;
struct alias {
  char *name;
  char *value;
  alias *next;
};
typedef struct {
  int exported;
  int readonly;
  int null;
} shvar_flag;

typedef struct shvar shvar;
struct shvar {
  shvar *next;
  char *name;
  char *value;
  shvar_flag flags;
};

#define ENV_BUCKETS 39
#define MAX_ALIAS_DEPTH 10
extern shvar *var_tab[ENV_BUCKETS];
extern alias *alias_tab[ENV_BUCKETS];

extern void init_env(void);
extern char **build_env(char **sh_env);
extern char *exp_var(char *, size_t, size_t *);
extern alias *find_alias(const char *);
extern void set_alias(const char *, const char *);
extern void rm_alias(const char *);
extern void setvar(const char *, const char *, shvar_flag);
extern void unset_var(const char *name);
extern shvar *find_var(const char *name);

/* vim: set filetype=c: */
#endif /* VAR_H */
