/*  alloc.c - stack arena allocator and otther malloc functions */
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>

#include "alloc.h"
#include "lex.h"

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

#define ul unsigned long
stackseg stackbase;
stackseg *current = &stackbase;
char *stnext = stackbase.buf;
size_t stleft = MINSTACK_S;
int stacksl = 0;

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
  stleft += stnext - (char *)p;
  stnext = p;
}

/**  clear everything back to stmark  */
void
stack_restore(stmark m)
{
  while (current != &stackbase && current != m.current) {
    stackseg *tmp = current->prev;
    slfree(current);
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
  stacksl = 1;
  stackseg *nseg = slalloc(len);
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
  stacksl = 1;
  if (!(nb = slalloc(sizeof(stackseg) - MINSTACK_S + nsize)))
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
    slfree(current);
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
  memset(stackbase.buf, 0, sizeof(stackbase.buf));
  current = &stackbase;
  stnext = stackbase.buf;
  stleft = MINSTACK_S;
}

/*  slab allocator  */


/* if needed create a new mmaped slab for the size needed */
void *
newslab(int ci)
{
  void *base;

  if ((base = mmap(NULL, slotsz[ci].sbsz, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
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

