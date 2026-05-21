/* env.h - declarations surrounding various parts of the shell environment */
#ifndef VAR_H
#define VAR_H

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>

#include "simpsh.h"
#include "utils.h"
#include "lex.h"

typedef struct alias alias;
struct alias {
  char *name;
  char *value;
  alias *next;
};

typedef int shvar_flags;
typedef struct shvar shvar;
struct shvar {
  shvar *next;
  shvar_flags flags;
  char *var;
  void (*func)(const char *);
};

typedef struct shfunc shfunc;
struct shfunc {
  shfunc *next;
  char *name;
  cmd_tree *body;
};

#define VEXPRT    (1 << 0)
#define VREADONLY (1 << 1)
#define VUNSET    (1 << 2)

#define ENV_BUCKETS 39
#define MAX_ALIAS_DEPTH 10
#define MAX_FUNC_DEPTH 40
#define shvar_val(v) (s_strchrnul(v->var, '=') + 1)
#define shvar_namelen(v) (s_strchrnul(v, '=') - v)

extern shvar *var_tab[ENV_BUCKETS];
extern alias *alias_tab[ENV_BUCKETS];
extern shfunc *func_tab[ENV_BUCKETS];

extern char *exp_tilde(char *, size_t, size_t *);
extern char **build_env(char **);
extern char *exp_var(char *, size_t, size_t *);
extern char *homedir(char *);
extern void init_env(void);

extern void setalias(const char *, const char *);
extern alias *findalias(const char *);
extern void rmalias(const char *);
extern void setvar(char *, char *, shvar_flags);
extern shvar *findvar(const char *);
extern void rmvar(const char *);
extern void setfunc(const char *, cmd_tree *);
extern shfunc *findfunc(const char *);
extern void rmfunc(const char *);

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
      free_redir(CREDR(n));
      free(n);
      break;
  }
}

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
