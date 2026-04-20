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
#define MINSTACK_S align_mem(512)

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

char *
stack_ptr(void)
{
  return stnext;
}

/** allocate new stack block */
void *
st_alloc(size_t dsize)
{
  size_t asize = align_mem(dsize);
  size_t len;

  if (asize >= stleft) {
    stack_seg *nseg;
    size_t need = asize;
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

  /*
   * Alignment disabled for testing
   */
  size_t align_offset = ((size_t)stnext & (sizeof(void *) - 1));
  if (align_offset) {
    size_t pad = sizeof(void *) - align_offset;
    if (pad > stleft)
      return grow_stack(asize);
    stnext += pad;
    stleft -= pad;
  }

  char *rp = stnext;
  stnext += asize;
  stleft -= asize;
  stend = stnext + stleft;
  return rp;
}

void *
grow_stack(size_t msize)
{
  size_t nsize;
  size_t commit;
  stack_seg *nb;
  char *obuf;

  commit = stnext - current->buf;
  obuf = current->buf;

  nsize = align_mem(msize + commit + 128);
  if (nsize <MINSTACK_S)
    nsize = MINSTACK_S;

  nb = malloc(sizeof(stack_seg) - MINSTACK_S + nsize);
  if (!nb)
    return NULL;
  nb->prev = current;
  current = nb;

  if (commit > 0)
    memcpy(nb->buf, obuf, commit);
  stnext = nb->buf + commit;
  stleft = nsize - commit;
  stend = nb->buf + nsize;
  return stnext;

  /* nsize = stleft * 2;
  if (nsize < msize)
    nsize = msize;
  nsize = align_mem(nsize + 128);  // NOTE: see why + 128

  used = stnext - current->buf;
  fprintf(stderr, "1: grow_stack: called with msize=%zu, stnext=%p\n", msize, stnext);
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
    fprintf(stderr, "2: grow_stack: called with msize=%zu, stnext=%p\n", msize, stnext);
    return stnext;
  } else {
    nb = st_alloc(nsize);
    memcpy(nb->buf, current->buf, used);
    stnext = nb->buf + used;
    stleft = nsize - used;
    stend = stnext + stleft;
    fprintf(stderr, "3: grow_stack: called with msize=%zu, stnext=%p\n", msize, stnext);
    return nb->buf;
  } */
    /* nb->prev = current; */
}

char *
grab_str(size_t len)
{
  
  char *start = stnext - len;
  if (stleft == 0)
    grow_stack(1);
  *stnext++ = '\0';
  stleft--;

  return start;

  // size_t len = end - start;
  // char *start = end - len;
  /* char *op = stack_ptr();
  char *res;
  fprintf(stderr, "1: grab_str: end=%p, start=%p, len=%zu\n", end, start, len); */

  /* Use st_alloc for the arena pattern - strings are cleaned up by stack_clear
   */
  /* res = st_alloc(len + 1);
  char *new = stack_ptr();
  if (res) {
    memcpy(res, start, len);
    res[len] = '\0';
  }
  st_unalloc(new);
  fprintf(stderr, "2: grab_str: end=%p, start=%p, len=%zu\n", end, start, len);
  return res; */
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
