/*  malloc.c - stack arena allocator and otther malloc functions */
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "malloc.h"

/*@NOTE:
 *      this stack allocator is directly inspired by dash's stack allocator
 *      I don't know if anyone will ever use this shell but I felt like I should
 *      give credit to them for it. I'm mostly doing this to learn. Because I
 *      want understand how this kind of memory management works, and how to
 * write optimized code like dash uses.
 */

/* TODO:
 *      test different sizes, and see what impact
 *      they have on performance if any
 */
stack_seg stackbase;
stack_seg *current = &stackbase;
char *stnext = stackbase.buf;
size_t stleft = MINSTACK_S;
char *stend;

/**  return stmark with the current state of the allocator  */
stmark
stack_mark(void)
{
  stmark m = { current, stnext, stleft };
  return m;
}

/**  clear everything back to stmark  */
void
stack_restore(stmark m)
{
  while (current != &stackbase && current != m.current) {
    stack_seg *tmp = current->prev;
    free(current);
    current = tmp;
  }
  current = m.current;
  stnext = m.next;
  stleft = current->buf + MINSTACK_S - stnext;
  stend = stnext + stleft;
}

/**  allocate new stack block  */
void *
st_alloc(size_t dsize)
{
  size_t asize = align_mem(dsize);
  size_t align_offset, pad;
  size_t len, need;
 
  char *rp;
  if (asize >= stleft) {
    stack_seg *nseg;
    need = asize;
    if (need < MINSTACK_S)
      need = MINSTACK_S;
    len = sizeof(stack_seg) - MINSTACK_S + need;
    nseg = malloc(len);
    if (!nseg)
      return NULL;
    nseg->prev = current;
    stnext = nseg->buf;
    stleft = need;
    current = nseg;
    stend = stnext + stleft;
  }

  align_offset = ((size_t)stnext & (sizeof(void *) - 1));
  if (align_offset) {
     pad = sizeof(void *) - align_offset;
    if (pad > stleft) {
      stnext = grow_stack(asize);
      if (!stnext)
        return NULL;
    }
    stnext += pad;
    stleft -= pad;
  }

  rp = stnext;
  stnext += asize;
  stleft -= asize;
  stend = stnext + stleft;
  return rp;
}

/**  grow the stack allocation  */
void *
grow_stack(size_t msize)
{
  size_t nsize;
  size_t used;
  stack_seg *nb;
  char *oldbuf;

  used = stnext - current->buf;
  oldbuf = current->buf;

  nsize = align_mem(msize + used + 128);
  if (nsize <MINSTACK_S)
    nsize = MINSTACK_S;
  nb = malloc(sizeof(stack_seg) - MINSTACK_S + nsize);
  if (!nb)
    return NULL;
  nb->prev = current;
  current = nb;
  if (used > 0)
    memcpy(nb->buf, oldbuf, used);
  stnext = nb->buf + used;
  stleft = nsize - used;
  stend = nb->buf + nsize;
  return stnext;
}

/**  grab string from arena  */
char *
grab_str(size_t len)
{
  if (len >= stleft)
    grow_stack(1);
  char *start = stnext - len;
  *stnext++ = '\0';
  stleft--;

  return start;
}

/**  clear the stack arena  */
void
stack_clear(void)
{
  stack_seg *tmp;
  while (current->prev != NULL) {
    tmp = current->prev;
    free(current);
    current = tmp;
  }
  current = &stackbase;
  stnext = stackbase.buf;
  stleft = MINSTACK_S;
  stend = stnext + stleft;
}

/**  initialize the stack  */
void
init_stack(void)
{
  current = &stackbase;
  stnext = stackbase.buf;
  stleft = MINSTACK_S;
  stend = stnext + stleft;
}

/**  original st_alloc stack allocator  */
// void *st_alloc(size_t dsize) {
//   size_t asize;
//   size_t align_offset, pad;
//   asize = align_mem(dsize);
//   char *rp;
//
//   if (asize >= stleft) {
//     stack_seg *nseg;
//     if (asize < MINSTACK_S)
//       asize = MINSTACK_S;
//     nseg = malloc(sizeof(stack_seg) - MINSTACK_S + asize);
//     nseg->prev = current;
//     current = nseg;
//     stnext = nseg->buf;
//     stleft = asize;
//   }
//
//   align_offset = ((size_t)stnext & (sizeof(void *) - 1));
//   if (align_offset) {
//     pad = sizeof(void *) - align_offset;
//     if (pad > stleft) {
//       stnext = grow_stack(asize);
//       if (!stnext)
//         return NULL;
//     }
//     stnext += pad;
//     stleft -= pad;
//   }
//
//   rp = stnext;
//   stnext += asize;
//   stleft -= asize;
//   return rp;
// }

