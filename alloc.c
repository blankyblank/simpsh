/*  malloc.c - stack arena allocator and otther malloc functions */
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

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
 * TODO:
 *      test different sizes, and see what impact
 *      they have on performance if any
 */

stack_seg stackbase;
stack_seg *current = &stackbase;
char *stnext = stackbase.buf;
size_t stleft = MINSTACK_S;

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
    stack_seg *tmp = current->prev;
    free(current);
    current = tmp;
  }
  current = m.current;
  stnext = m.next;
  stleft = m.stleft;
  wf_chunk = NULL;
  wf_chunk_left = 0;
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
  if (nsize < MINSTACK_S)
    nsize = MINSTACK_S;
  if (!(nb = malloc(sizeof(stack_seg) - MINSTACK_S + nsize)))
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
  stack_seg *tmp;
  while (current->prev != NULL) {
    tmp = current->prev;
    free(current);
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
  mallopt(M_TOP_PAD, 262144);
}
