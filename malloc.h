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
extern char *grab_str(char *, char *);
extern char *stack_ptr(void);

static inline size_t
stack_left(void)
{
  return stleft;
}

/** reclaim temp space in the stack block */
static inline void
st_unalloc(void *p)
{
  stleft += stnext - (char *)p;
  stnext = p;
}

static inline char *
stack_mark(void)
{
  return stack_ptr();
}

static inline void
stack_restore(char *mark)
{
  st_unalloc(mark);
}

/** alligned st_unalloc */
static inline void
st_aunalloc(void *p)
{
  char *cp = p;
  size_t offset = ((size_t)p & (sizeof(void *) - 1));
  if (offset) {
    cp += sizeof(void *) - offset;
  }
  st_unalloc(cp);
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

#define align_mem(n) (((n) + sizeof(void *) - 1) & ~(sizeof(void *) - 1))
/* static inline size_t
align_mem(size_t n)
{
  return (n + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
} */

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
