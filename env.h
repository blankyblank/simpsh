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
extern void setfunc(const char *restrict, cmd_tree *restrict);
extern shfunc *findfunc(const char *);
extern void rmfunc(const char *);
extern wf * wfdup(wf *s);
extern cmd_tree *tree_dup(cmd_tree *);

/* builtins */
extern int aliascmd(char **);
extern int unaliascmd(char **);

static void
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

static inline redir *
redirdup(redir *s)
{
  redir *n;
  if (!s)
    return NULL;
  n = malloc(sizeof(redir));
  n->fd = s->fd;
  n->name = wfdup(s->name);
  n->type = s->type;
  n->next = redirdup(s->next);
  return n;
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
    case WHILE:
      free_tree(n->left);
      free_tree(n->right);
      free(n);
      break;
    case IF:
      free_tree(n->left);
      free_tree(n->right);
      free_tree(CELSE(n));
      free(n);
      break;
    case CMD:
      for (size_t i = 0; CARGS(n)[i]; i++)
        free_wf(CARGS(n)[i]);
      free(CARGS(n));
      if (CVARS(n)) {
        for (size_t i = 0; CVARS(n)[i]; i++)
          free_wf(CVARS(n)[i]);
        free(CVARS(n));
      }
      free(n);
      break;
    default:
      return;
  }
}

#endif /* ENV_H */
