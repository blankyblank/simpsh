/* expand.h - string expandsion logic */
#ifndef EXPAND_H
#define EXPAND_H

#include <stddef.h>
#include "lex.h"

typedef struct {
  size_t start;
  size_t len;
} ifssect;


extern char *exp_tilde(char *, size_t, size_t *restrict, size_t *restrict);
extern char *exp_var(char *, size_t, size_t *restrict, size_t *restrict);
extern char *homedir(char *);
extern char **expand_argv(wf **, size_t *restrict);
extern char *expand_ps1(char *);
extern char *exp_word(wf *, size_t *restrict, ifssect **restrict);
extern char *lookupvar(const char *, size_t);

/* vim: set filetype=c: */
#endif /* EXPAND_H */
