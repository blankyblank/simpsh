#ifndef MALLOC_H
#define MALLOC_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* clang-format off */
#define SHELL_SIZE (sizeof(union {int i; char *cp; double d; }) - 1)
/* clang-format on */
#define align_mem(n) (((n) + SHELL_SIZE) & ~(sizeof(void *) - 1))
#define MINSTACK_S align_mem(8000)
/* so far 8000 seems pretty good for performance, but it seems large which can have it's own drawbacks
 * test more sized */
/**  set mark to reset allocator back to  */
#define stack_mark() ((stmark){ current, stnext, stleft })
/** stack allocated strdup */
#define st_strdup(s) (st_strndup(s, strlen(s)))
#define st_putc(c) (void)(stleft == 0 ? grow_stack(1) : (void *)0), *stnext++ = (c), stleft--
#define st_write(src, n, len) \
  do { \
    size_t _n = (n); \
    if (_n >= stleft) \
      grow_stack(_n); \
    memcpy(stnext, (src), _n); \
    stnext += _n; \
    stleft -= _n; \
    len += _n; \
  } while (0)

typedef struct stack_seg stack_seg;
struct stack_seg {
  stack_seg *prev;
  char buf[MINSTACK_S];
};

typedef struct {
  stack_seg *current;
  char *next;
  size_t stleft;
} stmark;

extern char *stnext;
extern size_t stleft;
extern stack_seg stackbase;
extern stack_seg *current;

extern void stack_restore(stmark);
extern void stack_clear(void);
extern void *grow_stack(size_t);
extern void *st_grow(size_t);
extern void init_stack(void);
extern void stunalloc(void *p);

/**  grab string from arena  */
static inline char *
grab_str(size_t len)
{
  if (len >= stleft)
    grow_stack(1);
  char *start = stnext - len;
  *stnext++ = '\0';
  stleft--;

  return start;
}

/**  allocate new stack block  */
static inline __attribute__((always_inline)) void *
st_alloc(size_t dsize)
{
  size_t asize = align_mem(dsize);
  if (__builtin_expect(asize >= stleft, 0))
    st_grow(asize);
  char *rp = stnext;
  stnext += asize;
  stleft -= asize;
  return rp;
}

/** stack allocated strndup */
__attribute__((always_inline)) static inline char *
st_strndup(const char *s, size_t len)
{
  char *d;
  d = st_alloc(len + 1);
  memcpy(d, s, len);
  d[len] = '\0';
  return d;
}

#endif /* !MALLOC_H */
