#ifndef MALLOC_H
#define MALLOC_H

#include <stddef.h>
#include <string.h>

extern char *stnext;
extern char *stend;
extern size_t stleft;

extern void *st_alloc(size_t);
extern void stack_clear(void);
extern void *grow_stack(size_t);
extern char *grab_str(char *);

static inline char *
stack_ptr(void)
{
  return stnext;
}

static inline size_t
stack_left(void)
{
  return stleft;
}

static inline void
st_unalloc(void *p)
{
  stleft += stnext - (char *)p;
  stnext = p;
}

static inline void
chk_space(size_t n, char **p)
{
  if (n > (size_t)(stend - *p))
    *p = grow_stack(n);
}

static inline char *
st_putc(int c, char *p)
{
  if (p >= stend)
    p = grow_stack(p - stnext + 1);
  *p++ = c;
  return p;
}

static inline size_t
align_mem(size_t n)
{
  return (n + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
}

static inline char *
st_strndup(const char *s, size_t len)
{
  char *d;
  d = st_alloc(len + 1);
  memcpy(d, s, len);
  d[len] = '\0';
  return d;
}

static inline char *
st_strdup(const char *s)
{
  return st_strndup(s, strlen(s));
}

#endif /* !MALLOC_H */
