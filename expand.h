/* expand.h - string expandsion logic */
#ifndef EXPAND_H
#define EXPAND_H

#include <stddef.h>
#include "lex.h"

typedef struct {
  size_t start;
  size_t len;
} ifssect;

extern int ifsnull;

extern void ifsupdt(const char *);
extern char *exp_tilde(char *restrict, size_t, size_t *restrict, size_t *restrict);
extern char *homedir(char *);
extern char **expand_argv(wf **, size_t *restrict);
extern char *expand_ps1(char *);
extern wf *exp_word(wf *, size_t *restrict);
// extern char *lookupvar(const char *, size_t);

static inline wf *
st_wfdup(wf *s)
{
  wf *n;
  if (!s)
    return NULL;
  n = wfalloc();
  n->word = st_strndup(s->word, s->len);
  n->len = s->len;
  n->qs = s->qs;
  n->next = st_wfdup(s->next);
  n->flags = s->flags;
  return n;
}

/* vim: set filetype=c: */
#endif /* EXPAND_H */
