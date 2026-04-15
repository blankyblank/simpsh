/* exec.h - functions surrounding running external programs or builtins */
#ifndef EXEC_H
#define EXEC_H

#include "lex.h"

extern int getbuiltin(char **);
extern int builtin_launch(char **);
extern int shexec(char **, char **);
extern int run_commands(const cmd_tree *);

/* vim: set filetype=c: */
#endif /* EXEC_H */
