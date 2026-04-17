/*  malloc.c - stack arena allocator and otther malloc functions */
#include "simpsh.h"
/* #include "utils.h" */
#include "malloc.h"


/*@NOTE:
 *      this stack allocator is directly inspired by dash's stack allocator
 *      I don't know if anyone will ever use this shell but I felt like I should
 *      give credit to them for it. I'm mostly doing this to learn. Because I want
 *      understand how this kind of memory management works, and how to write optimized
 *      code like dash uses.
 */

/* TODO:
 *      test different sizes, and see what impact
 *      they have on performance if any
 */
#define MINSTACK_S 512

typedef struct stack_seg stack_seg;
struct stack_seg {
  stack_seg *prev;
  char buf[MINSTACK_S];
};

typedef struct {
  stack_seg *current;
  char *next;
  size_t stleft;
} stack;

stack_seg stackbase;
stack_seg *current = &stackbase;
char *stnext = stackbase.buf;
size_t stleft = MINSTACK_S;

/** allocate new stack block */
void *
st_alloc(size_t dsize) {
  size_t asize;
  asize = align_mem(dsize);
  
  if (asize >= stleft) {
    stack_seg *nseg;
    if (asize < MINSTACK_S)
      asize = MINSTACK_S;
    nseg = malloc(sizeof(stack_seg) - MINSTACK_S + asize);
    nseg->prev = current;
    current = nseg;
    stnext = nseg->buf;
    stleft = asize;
  }
  char *rp = stnext;
  stnext += asize;
  stleft -= asize;
  return rp;
}

void
stack_clear(void) {
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

void
init_stack(void)
{
  current = &stackbase;
  stnext = stackbase.buf;
  stleft = MINSTACK_S;
}


