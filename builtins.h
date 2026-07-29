/* builtins.h - builtin shell commands */
#ifndef BUILTINS_H
#define BUILTINS_H

#include <stddef.h>
 
#include "utils.h"

#define BUILTIN_BUCKETS 64

typedef struct {
  const char *name;
  int (*fn)(char **);
  unsigned int flags;
} builtin;

typedef struct {
  const char *name;
  int resource; /* cmd to get/set */
  int factor;   /* multiply by to get rlim_{cur,max} values */
  char option;  /* option character (-d, -f, ...) */
} limit;

extern const builtin builtins[];
extern int builtin_tab[BUILTIN_BUCKETS];
extern const limit limits[];

#define GETBLKSIZE(f, st) (fstat(fileno(f), &(st)), (st).st_blksize ? (st).st_blksize : BUFSIZ)
#define SBLTN (1 << 0)
extern int bltin_atoi(char *, char *, char *);
extern void init_builtins(void);
extern int returncmd(char **);

static inline const builtin *
findbuiltin(const char *args)
{
  unsigned int idx;
  idx = hash(args, BUILTIN_BUCKETS);
  for (; builtin_tab[idx] >= 0; idx = (idx + 1) & (BUILTIN_BUCKETS - 1))
    if (builtins[builtin_tab[idx]].name[0] == args[0] &&
        strcmp(builtins[builtin_tab[idx]].name, args) == 0)
      return &builtins[builtin_tab[idx]];
  return NULL;
}
/* vim: set filetype=c: */
#endif /* BUILTINS_H */
