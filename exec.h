/* exec.h - functions surrounding running external programs or builtins */
#ifndef EXEC_H
#define EXEC_H

#include "lex.h"

#define BUILTIN_BUCKETS 32
extern int builtin_tab[BUILTIN_BUCKETS];
extern void init_builtins(void);
extern int run_commands(const cmd_tree *);
extern char *getpath(char *);
extern char *chkpath(const char *, const char *, unsigned int);

#define DUPFD(s, d) \
  if (dup2(s, d) < 0) { \
    perror("dup2"); \
    return 1; \
  }
#define OPENFD(f, m, n) \
  if ((n = open(f, m, 0666)) < 0) { \
    perror("open"); \
    return 1; \
  }
#define CLOSEFD(f) \
  if (close(f) < 0) { \
    perror("close"); \
    return 1; \
  }
/* vim: set filetype=c: */
#endif /* EXEC_H */
