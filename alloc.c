/*  alloc.c - stack arena allocator and otther malloc functions */
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "alloc.h"
#include "lex.h"

void *
getchunk(size_t n)
{
  size_t ttl = sizeof(chunk) + n + MINSLAB;
  chunk *c, **prev = &freelist;
  while (*prev) {
    if ((*prev)->size >= ttl) {
      chunk *c = *prev;
      *prev = c->next;
      return (char *)c + sizeof(chunk);
    }
    prev = &(*prev)->next;
  }
  if ((c = mmap(NULL, ttl, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE,
                -1, 0)) == MAP_FAILED)
    return NULL;
  c->size = ttl;
  c->next = NULL;
  mprotect((char *)c + ttl - MINSLAB, MINSLAB, PROT_READ);
  return (char *)c + sizeof(chunk);
}

void
clearchunk(void)
{
  chunk *c = freelist;
  while (c) {
    chunk *next = c->next;
    munmap(c, c->size);
    c = next;
  }
  freelist = NULL;
}

/* if needed create a new mmaped slab for the size needed */
void *
newslab(int ci)
{
  void *base;

  if (!(base = getchunk(slclasses[ci].sbsz)))
    return NULL;

  slab *s;
  s = (slab *)base;
  s->stsz = slclasses[ci].stsz;
  s->flist = NULL;
  s->p = (char *)base + sizeof(slab);
  s->end = (char *)base + slclasses[ci].sbsz;
  s->next = slclasses[ci].slabs;
  slclasses[ci].slabs = s;
  return s;
}

void
slclear(void)
{
  for (size_t i = 0; i < SLCLASSN; i++) {
    slab *s = slclasses[i].slabs;
    while (s) {
      slab *n = s->next;
      chunk *c = (chunk *)((char *)s - sizeof(chunk));
      munmap(c, c->size);
      s = n;
    }
  }
  clearchunk();
}

/*     STACK ALLOCATOR      */
/*
 * NOTE:
 *      this stack allocator is directly inspired by dash's stack allocator
 *      I don't know if anyone will ever use this shell but I felt like I should
 *      give credit to them for it. I'm mostly doing this to learn. Because I
 *      want understand how this kind of memory management works, and how to
 *      write optimized code like dash.
 */

/*
 * NOTE:
 *      we need to have the - 16, because we are using simd which loads 16 bytes
 * of data. if we don't keep that padding it goes past the buffer boundry and
 * causes a segfault.
 */

stackseg stackbase;
stackseg *current = &stackbase;
char *stnext = stackbase.buf;
size_t stleft = MINSTACK_S;

slclass slclasses[SLCLASSN] = {
  { .stsz = 16,   .sbsz = 65536  },
  { .stsz = 32,   .sbsz = 65536  },
  { .stsz = 64,   .sbsz = 65536  },
  { .stsz = 128,  .sbsz = 65536  },
  { .stsz = 256,  .sbsz = 65536  },
  { .stsz = 512,  .sbsz = 131072 },
  { .stsz = 1024, .sbsz = 131072 },
  { .stsz = 2048, .sbsz = 131072 },
  { .stsz = 4096, .sbsz = 131072 },
};

void
stunalloc(void *p)
{
  if (p >= (void *)stnext)
    return;
  stleft += (char *)stnext - (char *)p;
  stnext = p;
}

/**  clear everything back to stmark  */
void
stack_restore(stmark m)
{
  while (current != &stackbase && current != m.current) {
    stackseg *tmp = current->prev;
    putchunk(current);
    current = tmp;
  }
  current = m.current;
  stnext = m.next, stleft = m.stleft;
  wf_chunk = NULL;
  wf_chunk_left = 0;
}

void *
st_addseg(size_t asize)
{
  size_t need = asize < MINSTACK_S ? MINSTACK_S : asize;
  size_t len = sizeof(stackseg) - MINSTACK_S + need;
  stackseg *nseg = getchunk(len);
  if (!nseg)
    return NULL;
  nseg->prev = current;
  stnext = nseg->buf;
  stleft = need;
  current = nseg;
  char *rp = stnext;
  stnext += asize;
  stleft -= asize;
  return rp;
}

/**  grow the stack allocation  */
void *
grow_stack(size_t msize)
{
  size_t nsize;
  size_t used;
  stackseg *nb;
  char *oldbuf;

  used = stnext - current->buf;
  oldbuf = current->buf;

  nsize = align_mem(msize + used + 128);
  if (nsize < MINSTACK_S)
    nsize = MINSTACK_S;
  if (!(nb = getchunk(sizeof(stackseg) - MINSTACK_S + nsize)))
    return NULL;
  nb->prev = current;
  current = nb;
  if (used > 0)
    memcpy(nb->buf, oldbuf, used);
  stnext = nb->buf + used;
  stleft = nsize - used;
  return stnext;
}

/**  clear the stack arena  */
void
stack_clear(void)
{
  stackseg *tmp;
  while (current->prev != NULL) {
    tmp = current->prev;
    putchunk(current);
    current = tmp;
  }
  current = &stackbase;
  stnext = stackbase.buf;
  stleft = MINSTACK_S;
}

/**  initialize the stack  */
void
init_stack(void)
{
  current = &stackbase;
  stnext = stackbase.buf;
  stleft = MINSTACK_S;
}
