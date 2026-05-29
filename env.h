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

static inline void
free_wf(wf *f)
{
  if (!f)
    return;
  free(f->word);
  free_wf(f->next);
  free(f);
}

static inline void
free_redir(redir *r)
{
  while (r) {
    redir *tmp;
    tmp = r;
    r = r->next;
    free_wf(tmp->name);
    free(tmp);
  }
  return;
}

static inline void
free_tree(cmd_tree *n)
{
  if (!n)
    return;

  switch (n->type) {
    case OP:
      free_tree(n->left);
      free_tree(n->right);
      free(n);
      break;
    case SUBSHELL:
      free_tree(n->left);
      free(n);
      break;
    case FUNC:
      free_wf(CARGS(n)[0]);
      free(CARGS(n));
      free_tree(n->left);
      free(n);
      break;
    case REDIR:
      free_redir(CREDR(n));
      free_tree(n->left);
      free(n);
      break;
    case CMD:
      for (size_t i = 0; CARGS(n)[i]; i++)
        free_wf(CARGS(n)[i]);
      free(CARGS(n));
      if (CVARS(n)) {
        for (size_t i = 0; CVARS(n)[i]; i++)
          free(CVARS(n)[i]);
        free(CVARS(n));
      }
      free(n);
      break;
  }
}

#endif /* ENV_H */
