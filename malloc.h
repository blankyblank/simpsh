#ifndef MALLOC_H
#define MALLOC_H

#include <stddef.h>

extern char *stnext;
extern size_t stleft;

static inline size_t
align_mem(size_t n)
{
  return (n + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
}

void *st_alloc(size_t dsize);
extern void stack_clear(void);

#endif /* !MALLOC_H */
