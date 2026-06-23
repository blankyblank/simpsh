/* builtins.h - builtin shell commands */
#ifndef BUILTIN_H
#define BUILTIN_H

#define BUILTIN_BUCKETS 64

typedef struct {
  char *name;
  int (*fn)(char **);
} builtin;

extern const builtin builtins[];
extern int builtin_tab[BUILTIN_BUCKETS];

extern int nbuiltins(void);
extern void init_builtins(void);

/* vim: set filetype=c: */

#endif /* BUILTIN_H */
