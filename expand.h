/* expand.h - string expandsion logic */
#ifndef EXPAND_H
#define EXPAND_H
#include "lex.h"

extern char *exp_word(wf *);
extern char *exp_tilde(char *, size_t, size_t *);
extern char *exp_var(char *, size_t, size_t *);
extern char *homedir(char *);
extern char **expand_argv(wf **, size_t *);
extern char *expand_ps1(char *);

/* vim: set filetype=c: */
#endif /* EXPAND_H */
