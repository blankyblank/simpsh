/* builtins.h - builtin shell commands */
#ifndef BUILTIN_H
#define BUILTIN_H

#include <stddef.h>

typedef struct {
  char *name;
  int (*fn)(char **);
  unsigned int flags;
} builtin;

#define SBLTN (1 << 0)
extern int bltin_atoi(char *, char *, char *);
extern void init_builtins(void);
extern const builtin *find_builtin(const char *, size_t);

/**  get builtin command  */
#define findbuiltin(str) (find_builtin(str, strlen(str)))

/* vim: set filetype=c: */
#endif /* BUILTIN_H */
