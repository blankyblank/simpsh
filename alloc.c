/*  alloc.c - stack arena allocator and otther malloc functions */
#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */
#include <malloc.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>

#include "alloc.h"
#include "main.h"
#include "lex.h"

/*
 * NOTE:
 *      this stack allocator is directly inspired by dash's stack allocator
 *      I don't know if anyone will ever use this shell but I felt like I should
 *      give credit to them for it. I'm mostly doing this to learn. Because I
 *      want understand how this kind of memory management works, and how to
 *      write optimized code like dash.
 */

#define ul unsigned long
stackseg stackbase;
stackseg *current = &stackbase;
char *stnext = stackbase.buf;
size_t stleft = MINSTACK_S;
unsigned char stacksl = 0;

#ifdef DEBUG
  ststat stt;
#endif /* DEBUG */

slclass slotsz[SLCLASSN] = {
  { .stsz = 24,    .sbsz = sizeof(slab) + (ul)(512 * 16)  },
  { .stsz = 40,    .sbsz = sizeof(slab) + (ul)(256 * 32)  },
  { .stsz = 72,    .sbsz = sizeof(slab) + (ul)(128 * 64)  },
  { .stsz = 136,   .sbsz = sizeof(slab) + (ul)(128 * 128) },
  { .stsz = 264,   .sbsz = sizeof(slab) + (ul)(64 * 128)  },
  { .stsz = 520,   .sbsz = sizeof(slab) + (ul)(32 * 512)  },
  { .stsz = 1032,  .sbsz = sizeof(slab) + (ul)(18 * 1024) },
  { .stsz = 2056,  .sbsz = sizeof(slab) + (ul)(8 * 2056)  },
  { .stsz = 4104,  .sbsz = sizeof(slab) + (ul)(4 * 4104)  },
  { .stsz = 16392, .sbsz = sizeof(slab) + (ul)(2 * 16392) },
};
/*     STACK ALLOCATOR      */

void
stunalloc(void *p)
{
  if (p >= (void *)stnext)
    return;
#ifdef ENABLE_VALGRIND
  VALGRIND_MAKE_MEM_NOACCESS((char *)p, stnext - (char *)p);
#endif
  stleft += stnext - (char *)p;
  stnext = p;
}

/**  clear everything back to stmark  */
void
stack_restore(stmark m)
{
  while (current != &stackbase && current != m.current) {
    stackseg *tmp = current->prev;
#ifdef DEBUG
    stt.live -= current->cap;
    stt.cursegs--;
    stt.segfree++;
#endif
    sfree(current);
    current = tmp;
  }
  current = m.current;
#ifdef ENABLE_VALGRIND
  if (stnext > m.next)
    VALGRIND_MAKE_MEM_NOACCESS(m.next, stnext - m.next);
#endif
  stnext = m.next, stleft = m.stleft;
  wf_chunk = NULL;
  wf_chunk_left = 0;
}

void *
st_addseg(size_t asize)
{
  size_t need, len;
  stackseg *nseg;
  char *rp;

  need = asize < MINSTACK_S ? MINSTACK_S : asize;
  len = sizeof(stackseg) - MINSTACK_S + need;
  stacksl = 1;
  nseg = salloc(len);
  if (!nseg)
    return NULL;
  nseg->prev = current;
  stnext = nseg->buf;
  stleft = allocsz(nseg) - (sizeof(stackseg) - MINSTACK_S);
  current = nseg;
  rp = stnext;
  stnext += asize;
#ifdef DEBUG
  nseg->cap = stleft;
  stt.live += stleft;
  stt.cursegs++;
  stt.segalloc++;
  if (stt.live > stt.peak)
    stt.peak = stt.live;
  if (stt.cursegs > stt.peaksegs)
    stt.peaksegs = stt.cursegs;
#endif /* ifdef DEBUG */
  stleft -= asize;
#ifdef ENABLE_VALGRIND
  VALGRIND_MAKE_MEM_UNDEFINED(rp, asize);
#endif
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
  stacksl = 1;
  if (!(nb = salloc(sizeof(stackseg) - MINSTACK_S + nsize)))
    return NULL;
  nb->prev = current;
  current = nb;
  if (used > 0)
    memcpy(nb->buf, oldbuf, used);
  stnext = nb->buf + used;
  stleft = allocsz(nb) - (sizeof(stackseg) - MINSTACK_S) - used;
#ifdef DEBUG
  nb->cap = stleft + used;
  stt.live += nb->cap;
  stt.cursegs++;
  stt.segalloc++;
  if (stt.live > stt.peak)
    stt.peak = stt.live;
  if (stt.cursegs > stt.peaksegs)
    stt.peaksegs = stt.cursegs;
#endif /* ifdef DEBUG */
  return stnext;
}

/**  clear the stack arena  */
void
stack_clear(void)
{
  stackseg *tmp;
  while (current->prev != NULL) {
    tmp = current->prev;
#ifdef DEBUG
    stt.live -= current->cap;
    stt.cursegs--;
    stt.segfree++;
#endif
    sfree(current);
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
  mallopt(M_ARENA_MAX, 1); // these two seem fine for now
  mallopt(M_TRIM_THRESHOLD, 128);
  memset(stackbase.buf, 0, sizeof(stackbase.buf));
  current = &stackbase;
  stnext = stackbase.buf;
  stleft = MINSTACK_S;
#ifdef DEBUG
  memset(&stt, 0, sizeof(stt));
#endif /* ifdef DEBUG */
}

/*  slab allocator  */


/* if needed create a new mmaped slab for the size needed */
void *
newslab(int ci)
{
  void *base;

  if ((base = mmap(NULL, slotsz[ci].sbsz, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON, -1, 0)) == MAP_FAILED)
    return NULL;
  slab *s;
  s = (slab *)base;
  s->ci = ci;
  s->magic = MEMMAGIC;
  s->stsz = slotsz[ci].stsz;
  s->flist = NULL;
  s->p = (char *)base + sizeof(slab);
  s->end = (char *)base + slotsz[ci].sbsz;
  s->next = slotsz[ci].slabs;
  slotsz[ci].slabs = s;
  return s;
}

void
slclear(void)
{
  for (size_t i = 0; i < SLCLASSN; i++) {
    slab *s = slotsz[i].slabs;
    while (s) {
      slab *n = s->next;
      munmap(s, slotsz[s->ci].sbsz);
      s = n;
    }
    slotsz[i].slabs = NULL;
  }
}

#ifdef DEBUG
void
stack_state(const char *label)
{
  char cb[32];
  size_t committed = MINSTACK_S + stt.live;

  if (committed >> 20)
    snprintf(cb, sizeof cb, "%.2fM", (double)committed / (1 << 20));
  else if (committed >> 10)
    snprintf(cb, sizeof cb, "%.2fK", (double)committed / (1 << 10));
  else
    snprintf(cb, sizeof cb, "%zuB", committed);

  if (stt.cursegs)
    fprintf(stderr, "%s: committed %s in %zu seg, %zuB free in top\n",
            label, cb, stt.cursegs + 1, stleft);
  else
    fprintf(stderr, "%s: used %zuB of %s base\n",
            label, stnext - stackbase.buf, cb);
}

void
stack_report(void)
{
  size_t v[2] = { stt.live, stt.peak };
  char buf[2][32];

  for (size_t i = 0; i < 2; i++) {
    if (v[i] >> 20)
      snprintf(buf[i], sizeof buf[i], "%.2fM", (double)v[i] / (1 << 20));
    else if (v[i] >> 10)
      snprintf(buf[i], sizeof buf[i], "%.2fK", (double)v[i] / (1 << 10));
    else
      snprintf(buf[i], sizeof buf[i], "%zuB", v[i]);
  }
  fprintf(stderr, "stack: live %s in %zu seg, peak %s in %zu seg, %zu alloc / %zu free\n",
    buf[0], stt.cursegs, buf[1], stt.peaksegs, stt.segalloc, stt.segfree);
}
#endif /* DEBUG */
