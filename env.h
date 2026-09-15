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
  int inuse;
  alias *next;
};

typedef struct shfunc shfunc;
struct shfunc {
  shfunc *next;
  char *name;
  cmd_tree *body;
  int inuse;
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
extern cmd_tree *tree_dup(cmd_tree *);
extern void free_tree(cmd_tree *);
void cleandefered(void);
wf * wfdup(wf *s, int d);

static inline redir *
redirdup(redir *s)
{
  redir *sp, *head, *n;

  if (!s)
    return NULL;
  if (!(head = salloc(sizeof(redir))))
    return NULL;

  n = head;
  sp = s;
  do {
    n->fd = sp->fd;
    n->name = wfdup(sp->name, 0);
    n->type = sp->type;
    n->heredoc = sp->heredoc ? strdup_(sp->heredoc) : NULL;
    n->heredoc_next = NULL;
    if ((sp = sp->next)) {
      if (!(n->next = salloc(sizeof(redir))))
        return NULL;
      n = n->next;
    }
  } while (sp);
  n->next = NULL;
  return head;
}

#endif /* ENV_H */
















