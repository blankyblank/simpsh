#ifndef ALLOC_H
#define ALLOC_H

#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* so far 8000 for minstack_s seems pretty good for performance, but it seems
 * large which can have it's own drawbacks test more sized */
#define align_mem(n) (((n) + 15) & ~15)
#define MINSTACK_S align_mem(8192)
#define PAGE_SIZE 4096
#define MEMMAGIC 0x534C4142
#define LARGEMAGIC ((void *)(uintptr_t)0x4C52474C)   /* "LARG" */
#define SLCLASSN 10
#define STACK_CLS 9
#define MINSLAB align_mem(4096)
#define stack_mark() ((stmark) { current, stnext, stleft })
#define st_strdup(s) (st_strndup(s, strlen(s))) /** stack allocated strdup */
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

#define getclass(n, i) \
  do { \
    size_t _n = (size_t)(n); \
    int _flr = 63 - __builtin_clzll(_n | 1); \
    int _ceil = _flr + ((_n & (_n - 1)) != 0); \
    i = _ceil > 4 ? _ceil - 4 : 0; \
    if (i > 8) \
      i = 8; \
  } while (0)

typedef struct stackseg stackseg;
struct stackseg {
  stackseg *prev;
  char _pad[8];
  char buf[MINSTACK_S + 16];
};

typedef struct {
  stackseg *current;
  char *next;
  size_t stleft;
} stmark;

typedef struct slab slab;
struct slab {
  int magic;
  int nalloc;  /* outstanding allocations */
  unsigned char ci;      /* class index */
  slab *next;  /* list for free lookup (linked list) */
  size_t stsz; /* slot size */
  void *flist; /* free list */
  void *p;     /* current position pointer in slot */
  void *end;   /* beginning of guard page */
};

typedef struct {
  size_t stsz;
  size_t sbsz;
  slab *slabs;
} slclass;

extern slclass slotsz[SLCLASSN];

extern char *stnext;
extern size_t stleft;
extern stackseg stackbase;
extern stackseg *current;
extern unsigned char stacksl;

/* stack allocator functions */
extern void stack_restore(stmark);
extern void stack_clear(void);
void *st_addseg(size_t);
extern void *grow_stack(size_t);
extern void init_stack(void);
extern void stunalloc(void *);
/* slab allocator functions */
void *newslab(int);
extern void slclear(void);

static inline void *
slalloc(size_t n)
{
  int i;
  slclass *c;
  slab *s;
  void *p;

if (stacksl) {
    stacksl = 0;
    if (n + sizeof(slab *) > slotsz[STACK_CLS].stsz)
      goto large;
    i = STACK_CLS;
    goto stackskip;
  }
  if (n + sizeof(slab *) >= PAGE_SIZE) {
  large:
    if (posix_memalign(&p, PAGE_SIZE, align_mem(n) + sizeof(slab *)))
      return NULL;
    *(slab **)p = LARGEMAGIC;
    memset((char *)p + sizeof(slab *), 0, align_mem(n));
    return (char *)p + sizeof(slab *);
  }
  getclass(n + sizeof(slab *), i);

stackskip:
  c = &slotsz[i];
  if (!c->slabs) {
    c->slabs = newslab(i);
    if (!c->slabs)
      return NULL;
  }

  for (;;) {
    s = c->slabs;
    if (s->flist) {
      p = s->flist;
      s->flist = *(void **)s->flist;
      s->nalloc++;
      *(slab **)p = s;
      return (char *)p + sizeof(slab *);
    }
    if ((char *)s->p + s->stsz <= (char *)s->end) {
      p = s->p;
      s->p = (char *)s->p + s->stsz;
      s->nalloc++;
      *(slab **)p = s;
      return (char *)p + sizeof(slab *);
    }
    s = newslab(i);
    if (!s)
      return NULL;
  }
}

static inline void
slfree(void *p)
{
  if (!p)
    return;
  void *magic, *save;
  slab *s;

  magic = *(void **)((char *)p - sizeof(slab *));
  if (magic == LARGEMAGIC) {
    free((char *)p - sizeof(slab *));
    return;
  }

  s = magic;
  save = p;
  p = (char *)p - sizeof(slab *);
  if (s->magic != MEMMAGIC || (char *)p < (char *)s + sizeof(slab) ||
      (char *)p >= (char *)s->end) {
    free(save);
    return;
  }
  *(void **)p = s->flist;
  s->flist = p;
  s->nalloc--;
}

static inline void
*slcalloc(size_t n, size_t size) {
    void *p;
    size_t total = n * size;
    if ((p = slalloc(total))) {
            memset(p, 0, total);
    }
    return p;
}

/**  allocate new stack block  */
static inline __attribute__((always_inline)) void *
st_alloc(size_t dsize)
{
  size_t pad;
  size_t asize = align_mem(dsize);
  if (__builtin_expect(asize >= stleft, 0))
    return st_addseg(asize);

  pad = (-(size_t)stnext) & (sizeof(void *) - 1);
  if (pad > stleft) {
    stnext = grow_stack(asize);
    if (!stnext)
      return NULL;
  }
  stnext += pad;
  stleft -= pad;
  char *rp = stnext;
  stnext += asize;
  stleft -= asize;
  return rp;
}

/**  grab string from arena  */
static inline __attribute__((always_inline)) char *
grab_str(size_t len)
{
  if (len >= stleft)
    grow_stack(1);
  char *start = stnext - len;
  *stnext++ = '\0';
  stleft--;

  return start;
}

/** stack allocated strndup */
static inline __attribute__((always_inline)) char *
st_strndup(const char *s, size_t len)
{
  char *d;
  d = st_alloc(len + 1);
  memcpy(d, s, len);
  d[len] = '\0';
  return d;
}
#endif /* !ALLOC_H */
