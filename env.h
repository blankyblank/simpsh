/* env.h - declarations surrounding various parts of the shell environment */
#ifndef ENV_H
#define ENV_H

#include <stdio.h>
#include <stdlib.h>

#include "alloc.h"
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
  slfree(f->word);
  free_wf(f->next);
  slfree(f);
}

static inline void
free_redir(redir *r)
{
  while (r) {
    redir *tmp;
    tmp = r;
    r = r->next;
    free_wf(tmp->name);
    slfree(tmp);
  }
  return;
}

static inline redir *
redirdup(redir *s)
{
  redir *n;
  if (!s)
    return NULL;
  n = slalloc(sizeof(redir));
  n->fd = s->fd;
  n->name = wfdup(s->name);
  n->type = s->type;
  n->next = redirdup(s->next);
  n->heredoc = NULL;
  n->heredoc_next = NULL;
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
      slfree(n);
      break;
    case SUBSHELL:
      free_tree(n->left);
      slfree(n);
      break;
    case FUNC:
      free_wf(CARGS(n)[0]);
      slfree(CARGS(n));
      free_tree(n->left);
      slfree(n);
      break;
    case REDIR:
      free_redir(CREDR(n));
      free_tree(n->left);
      slfree(n);
      break;
    case WHILE:
      free_tree(n->left);
      free_tree(n->right);
      slfree(n);
      break;
    case FOR:
      free_tree(n->right);
      if (CFOR(n).words) {
        for (size_t i = 0; CFOR(n).words[i]; i++)
          free_wf(CFOR(n).words[i]);
        slfree(CFOR(n).words);
      }
      free_wf(CFOR(n).name);
      slfree(n);
      break;
    case CASE:
      free_wf(CCASE(n).word);
      for (clause *c = CCASE(n).clauses; c;) {
        clause *next = c->next;
        for (size_t i = 0; c->ptrn[i]; i++)
          free_wf(c->ptrn[i]);
        slfree(c->ptrn);
        free_tree(c->body);
        slfree(c);
        c = next;
      }
      slfree(n);
      break;
    case IF:
      free_tree(n->left);
      free_tree(n->right);
      free_tree(CELSE(n));
      slfree(n);
      break;
    case BRACE:
      free_tree(n->left);
      slfree(n);
      break;
    case CMD:
      for (size_t i = 0; CARGS(n)[i]; i++)
        free_wf(CARGS(n)[i]);
      slfree(CARGS(n));
      if (CVARS(n)) {
        for (size_t i = 0; CVARS(n)[i]; i++)
          free_wf(CVARS(n)[i]);
        slfree(CVARS(n));
      }
      slfree(n);
      break;
    default:
      return;
  }
}

#endif /* ENV_H */
