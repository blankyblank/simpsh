/* env.h - declarations surrounding various parts of the shell environment */
#ifndef ENV_H
#define ENV_H

#include <stdio.h>
#include <stdlib.h>

#include "parse.h"

typedef struct alias alias;
struct alias {
  char *name;
  char *value;
  alias *next;
};

typedef struct shfunc shfunc;
struct shfunc {
  shfunc *next;
  char *name;
  cmd_tree *body;
};

#define MAX_ALIAS_DEPTH 10
#define MAX_FUNC_DEPTH 40
#define ENV_BUCKETS 64

extern alias *alias_tab[ENV_BUCKETS];
extern shfunc *func_tab[ENV_BUCKETS];

extern void setalias(const char *, const char *);
extern alias *findalias(const char *);
extern void rmalias(const char *);
extern void setfunc(const char *, cmd_tree *);
extern shfunc *findfunc(const char *);
extern void rmfunc(const char *);

/* builtins */
extern int aliascmd(char **);
extern int unaliascmd(char **);

#endif /* ENV_H */
