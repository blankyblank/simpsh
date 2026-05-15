/* exec.h - functions surrounding running external programs or builtins */
#ifndef EXEC_H
#define EXEC_H

#include "lex.h"

#define BUILTIN_BUCKETS 16
extern int builtin_tab[BUILTIN_BUCKETS];
extern void init_builtins(void);
extern int run_commands(const cmd_tree *);
extern char *getpath(char *);
extern char *chkpath(const char *, const char *, unsigned int);

/* vim: set filetype=c: */
#endif /* EXEC_H */
