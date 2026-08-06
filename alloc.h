#ifndef ALLOC_H
#define ALLOC_H

#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef ENABLE_VALGRIND
  #include <valgrind/cachegrind.h>
  #include <valgrind/memcheck.h>
#endif /* ifdef ENABLE_VALGRIND */
#if defined(__clang__)
#  if __has_attribute(no_sanitize)
#    define NO_UBSAN __attribute__((no_sanitize("unsigned-integer-overflow")))
#  endif
#endif
#ifndef NO_UBSAN
#  define NO_UBSAN
#endif

/* so far 8000 for minstack_s seems pretty good for performance, but it seems
 * large which can have it's own drawbacks test more sized */
#define align_mem(n) (((n) + (_Alignof(max_align_t) - 1)) & ~(size_t)(_Alignof(max_align_t) - 1))
#define MINSTACK_S   align_mem(8192)
#define MEMSIZE    4096
#define MEMMAGIC     0x534C4142
#define LARGEMAGIC   ((void *)(uintptr_t)0x4C52474C) /* "LARG" */
#define SLCLASSN     10
#define STACK_CLS    9
#define MINSLAB      align_mem(4096)
#define stack_mark() ((stmark) { current, stnext, stleft })
#define st_strdup(s) (st_strndup(s, strlen(s))) /** stack allocated strdup */
#define stcheck(n) ((void)(stleft == 0 ? grow_stack(n) : (void *)0))
#define st_putc(c)  (*(unsigned char *)stnext++ = (c), stleft--)
#define strdup_(s) (strndup_((s), strlen(s)))
#ifdef __TINYC__
static inline __attribute__((always_inline)) int
flrlog2(size_t x)
{
  int n = 0;
  if (x >> 32)
    n += 32, x >>= 32;
  if (x >> 16)
    n += 16, x >>= 16;
  if (x >> 8)
    n += 8, x >>= 8;
  if (x >> 4)
    n += 4, x >>= 4;
  if (x >> 2)
    n += 2, x >>= 2;
  if (x >> 1)
    n += 1;
  return n;
}
#else
#define flrlog2(x) (63 - __builtin_clzll((unsigned long long)(x)))
#endif /* __TINYC__ */

#define streallocar(ar, sz, used, t) \
  do { \
    t *n = st_alloc((sz) * sizeof(t)); \
    memcpy(n, (ar), (used) * sizeof(t)); \
    (ar) = n; \
  } while (0)

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
    int _flr = flrlog2(_n | 1); \
    int _ceil = _flr + ((_n & (_n - 1)) != 0); \
    i = _ceil > 4 ? _ceil - 4 : 0; \
    if (i > 8) \
      i = 8; \
  } while (0)

typedef struct stackseg stackseg;
struct stackseg {
  stackseg *prev;
  char _pad[8];
  char buf[MINSTACK_S];
};

typedef struct {
  stackseg *current;
  char *next;
  size_t stleft;
} stmark;

typedef struct slab slab;
struct slab {
  int magic;
  int nalloc;       /* outstanding allocations */
  unsigned char ci; /* class index */
  slab *next;       /* list for free lookup (linked list) */
  size_t stsz;      /* slot size */
  void *flist;      /* free list */
  void *p;          /* current position pointer in slot */
  void *end;        /* beginning of guard page */
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
salloc(size_t n)
{
  int i;
  size_t sz;
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
  if (n + sizeof(slab *) >= MEMSIZE) {
  large:
    sz = align_mem(n) + sizeof(slab *) + sizeof(size_t);
    sz = (sz + MEMSIZE - 1) & ~(size_t)(MEMSIZE - 1);
    p = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED)
      return NULL;
    *(size_t *)p = sz;
    *(void **)((char *)p + sizeof(size_t)) = LARGEMAGIC;
    return (char *)p + sizeof(slab *) + sizeof(size_t);
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
#ifdef ENABLE_VALGRIND
      VALGRIND_MALLOCLIKE_BLOCK((char *)p + sizeof(slab *), c->stsz - sizeof(slab *), 0, 0);
#endif
      return (char *)p + sizeof(slab *);
    }
    if ((char *)s->p + s->stsz <= (char *)s->end) {
      p = s->p;
      s->p = (char *)s->p + s->stsz;
      s->nalloc++;
      *(slab **)p = s;
#ifdef ENABLE_VALGRIND
      VALGRIND_MALLOCLIKE_BLOCK((char *)p + sizeof(slab *), c->stsz - sizeof(slab *), 0, 0);
#endif
      return (char *)p + sizeof(slab *);
    }
    s = newslab(i);
    if (!s)
      return NULL;
  }
}

static inline void
sfree(void *p)
{
  if (!p)
    return;
  void *magic, *save;
  slab *s;

  magic = *(void **)((char *)p - sizeof(slab *));
  if (magic == LARGEMAGIC) {
    void *base = (char *)p - sizeof(slab *) - sizeof(size_t);
    munmap(base, *(size_t *)base);
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
#ifdef ENABLE_VALGRIND
  VALGRIND_FREELIKE_BLOCK(save, 0);
#endif
  *(void **)p = s->flist;
  s->flist = p;
  s->nalloc--;
}

/**  strndup using memcpy, and slmalloc  */
static inline char *
strndup_(const char *restrict s, size_t n)
{
  char *dup;
  if ((dup = salloc(n + 1))) {
    memcpy(dup, s, n);
    dup[n] = '\0';
  }
  return dup;
}

/* reallocate from slab allocator */
static inline void *
srealloc(void *p, size_t s)
{
  if (!p && !s)
    return NULL;
  if (s && !p)
    return salloc(s);
  if (p && !s) {
    sfree(p);
    return NULL;
  }
  void *magic;
  size_t o;
  char *n;

  magic = *(void **)((char *)p - sizeof(slab *));
  if (magic == LARGEMAGIC) {
    size_t *base = (size_t *)((char *)p - sizeof(slab *) - sizeof(size_t));
    o = *base - sizeof(slab *) - sizeof(size_t);
  } else {
    slab *sp = magic;
    o = sp->stsz - sizeof(slab *);
  }
  n = salloc(s);
  if (n)
    memcpy(n, p, o < s ? o : s);
  sfree(p);
  return n;
}

static inline void *
slcalloc(size_t n, size_t size)
{
  void *p;
  size_t total = n * size;
  if ((p = salloc(total))) {
    memset(p, 0, total);
  }
  return p;
}

/**  allocate new stack block  */
NO_UBSAN
__attribute__((always_inline))
static inline  void *
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
#ifdef ENABLE_VALGRIND
  VALGRIND_MAKE_MEM_UNDEFINED(rp, asize);
#endif
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
