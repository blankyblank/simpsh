/* expand.h - string expandsion logic */
#ifndef EXPAND_H
#define EXPAND_H
#include "lex.h"

extern char *expand_word(wf *);
extern char **expand_argv(wf **, size_t *);

/* vim: set filetype=c: */
#endif /* EXPAND_H */
