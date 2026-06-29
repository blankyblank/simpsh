/* builtins.h - builtin shell commands */
#ifndef BUILTIN_H
#define BUILTIN_H

#include "utils.h"

#define BUILTIN_BUCKETS 64

typedef struct {
  char *name;
  int (*fn)(char **);
} builtin;

extern const builtin builtins[];
extern int builtin_tab[BUILTIN_BUCKETS];

extern int nbuiltins(void);
extern void init_builtins(void);

/**  get builtin command  */
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

#endif /* BUILTIN_H */
