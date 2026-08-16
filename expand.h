/* expand.h - string expandsion logic */
#ifndef EXPAND_H
#define EXPAND_H

#include <stddef.h>
#include "lex.h"

typedef struct {
  size_t start;
  size_t len;
} ifssect;

extern char ifschar[256];
extern int incmdsub;

extern char *exp_tilde(char *restrict, size_t, size_t *restrict, size_t *restrict);
extern char *homedir(char *);
extern char **expand_argv(wf **, size_t *restrict);
extern char *expand_ps1(char *);
extern char * exp_cmdsub(const char *restrict, size_t, size_t *restrict);
extern wf *exp_word(wf *, size_t *restrict);

#define chk_cap(arc, c, arv, t) \
  if ((arc) >= (c)) { \
    (c) *= 2; \
    streallocar(arv, c, arc, t); \
  }

/* vim: set filetype=c: */
#endif /* EXPAND_H */
