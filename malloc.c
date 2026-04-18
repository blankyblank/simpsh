/*  malloc.c - stack arena allocator and otther malloc functions */
#include "simpsh.h"
/* #include "utils.h" */
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
char *stend;

/** allocate new stack block */
void *
st_alloc(size_t dsize)
{
  size_t asize = align_mem(dsize);

  if (asize >= stleft) {
    stack_seg *nseg;
    size_t need = asize;
    if (need < MINSTACK_S)
      need = MINSTACK_S;
    nseg = malloc(sizeof(stack_seg) - MINSTACK_S + need);
    if (!nseg)
      return NULL;
    nseg->prev = current;
    current = nseg;
    stnext = nseg->buf;
    stleft = need;
    stend = stnext + stleft;
  }

  /*
   * Alignment disabled for testing
   * size_t align_offset = ((size_t)stnext & (sizeof(void *) - 1));
   * if (align_offset) {
   *   size_t pad = sizeof(void *) - align_offset;
   *   if (pad > stleft)
   *     return grow_stack(asize);
   *   stnext += pad;
   *   stleft -= pad;
   * }
   */

  char *rp = stnext;
  stnext += asize;
  stleft -= asize;
  stend = stnext + stleft;
  return rp;
}

void *
grow_stack(size_t msize)
{
  size_t nsize, used;
  stack_seg *nb;

  nsize = stleft * 2;
  if (nsize < msize)
    nsize = msize;
  nsize = align_mem(nsize + 128);  // NOTE: see why + 128

  used = stnext - current->buf;
  if (!used && current != &stackbase) {
    nb = malloc(sizeof(stack_seg) - MINSTACK_S + nsize);
    if (!nb)
      return NULL;
    memcpy(nb->buf, stackbase.buf, used);
    nb->prev = NULL;
    current = nb;
    stnext = nb->buf + used;
    stleft = nsize - used;
    stend = stnext + stleft;
    return stnext;
  } else {
    nb = st_alloc(nsize);
    memcpy(nb->buf, current->buf, used);
    stnext = nb->buf + used;
    stleft = nsize - used;
    stend = stnext + stleft;
    return stnext;
  }
}

char *
grab_str(char *end)
{
  size_t len = end - stack_ptr();
  char *start = end - len;
  char *res;

  /* Use st_alloc for the arena pattern - strings are cleaned up by stack_clear
   */
  res = st_alloc(len + 1);
  if (res) {
    memcpy(res, start, len);
    res[len] = '\0';
  }
  st_unalloc(end);
  return res;
}

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

void
init_stack(void)
{
  current = &stackbase;
  stnext = stackbase.buf;
  stleft = MINSTACK_S;
  stend = stnext + stleft;
}
