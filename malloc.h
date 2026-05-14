#ifndef MALLOC_H
#define MALLOC_H

#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <string.h>

/* clang-format off */
#define SHELL_SIZE (sizeof(union {int i; char *cp; double d; }) - 1)
#define align_mem(n) (((n) + SHELL_SIZE) & ~(sizeof(void *) - 1)) /* clang-format on */
#define MINSTACK_S align_mem(2048)

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
extern char *stend;
extern size_t stleft;

extern stmark stack_mark(void);
extern void stack_restore(stmark);
extern void *st_alloc(size_t);
extern void stack_clear(void);
extern void *grow_stack(size_t);
extern char *grab_str(size_t);
extern void init_stack(void);
extern void stunalloc(void *p);

static inline char *
st_strndup(const char *s, size_t len)
{
  char *d;
  d = st_alloc(len + 1);
  memcpy(d, s, len);
  d[len] = '\0';
  return d;
}

#define st_putc(c) (void)(stleft == 0 ? grow_stack(1) : (void *)0), *stnext++ = (c), stleft--
/** stack allocator strdup */
#define st_strdup(s) (st_strndup(s, strlen(s)))

/* #define st_putc(c) do { \
  if (stleft == 0)      \
    (void)grow_stack(1); \
  *stnext++ = (c);       \
  stleft--;              \
} while (0) */


#endif /* !MALLOC_H */
