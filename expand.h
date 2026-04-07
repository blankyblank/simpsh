#ifndef EXPAND_H
#define EXPAND_H
#include "lex.h"

extern char *expand_word(wf *);
extern char **expand_argv(wf **);
extern void bufcat(char **, size_t *, size_t *, const char *, size_t );


/* vim: set filetype=c: */
#endif /* EXPAND_H */
