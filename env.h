/* env.h - declarations surrounding various parts of the shell environment */
#ifndef ENV_H
#define ENV_H


#include "alloc.h"
#include "lex.h"
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
extern wf * wfdup(wf *);
extern cmd_tree *tree_dup(cmd_tree *);
extern void free_tree(cmd_tree *);

static inline redir *
redirdup(redir *s)
{
  redir *n;
  if (!s)
    return NULL;
  n = salloc(sizeof(redir));
  n->fd = s->fd;
  n->name = wfdup(s->name);
  n->type = s->type;
  n->next = redirdup(s->next);
  n->heredoc = NULL;
  n->heredoc_next = NULL;
  return n;
}

#endif /* ENV_H */
