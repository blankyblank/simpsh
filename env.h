/* env.h - declarations surrounding various parts of the shell environment */
#ifndef VAR_H
#define VAR_H

#include "ctype.h"
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

#define VAR_BUCKETS 39
#define ALIAS_BUCKETS 39
#define MAX_ALIAS_DEPTH 10
extern shvar *var_tab[VAR_BUCKETS];
extern alias *alias_tab[ALIAS_BUCKETS];

extern void init_env(void);
extern char **build_env(char **sh_env);
extern char *exp_var(char *, size_t, size_t *);
extern alias *find_alias(const char *);
extern void set_alias(const char *, const char *);
extern void rm_alias(const char *);
extern void setvar(const char *, const char *, shvar_flag);
extern void unset_var(const char *name);
extern shvar *find_var(const char *name);

/** get the variable for the return status of last command */
static inline char *
statusvar(void)
{
  char *buf = malloc(12);
  snprintf(buf, 12, "%d", lstatus);
  return buf;
}

/** get the pid shell variable */
static inline char *
var_pid(void)
{
  return strdup(sh_pid_s);
}

/** get positional parameters */
static inline char *
get_posparam(int n)
{
  if (n == 0)
    return sh_argv0 ? sh_argv0 : "";

  if (n < 0 || n > sh_argc)
    return "";
  return sh_argv[n - 1] ? sh_argv[n - 1] : "";
}

/** find if variable is positional parameter */
static inline int
is_posparam(const char *var)
{
  size_t i;
  size_t var_l = strlen(var);

  if (!var || !*var)
    return 0;
  for (i = 0; i < var_l; i++) {
    if (!isdigit(var[i]))
      return 0;
  }
  return 1;
}

/* vim: set filetype=c: */
#endif /* VAR_H */
