#define _POSIX_C_SOURCE 200809L
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "alloc.h"
#include "input.h"
#include "lex.h"
#include "simpsh.h"

static char basebuf[BASEBUFSIZE];
shinput base_shinput;
shinput *cur_shinpt = &base_shinput;

void
init_input(void)
{
  base_shinput.prev = NULL;
  base_shinput.buf = basebuf;
  base_shinput.nchar = basebuf;
  base_shinput.nleft = 0;
  base_shinput.fd = STDIN_FILENO;
  base_shinput.linenum = 1;
  base_shinput.unget = 0;
  base_shinput.strpush = NULL;
  base_shinput.mapsize = 0;
  cur_shinpt = &base_shinput;
}

void
pushstring(char *s, size_t len, int alias)
{
  strpush *sp = st_alloc(sizeof(strpush));
  sp->prev = cur_shinpt->strpush;
  sp->saved_nchar = cur_shinpt->nchar;
  sp->saved_nleft = cur_shinpt->nleft;
  sp->saved_unget = cur_shinpt->unget;
  memcpy(sp->saved_ungetbuf, cur_shinpt->ungetbuf, 4);
  sp->alias = alias;
  cur_shinpt->nchar = s;
  cur_shinpt->nleft = len;
  cur_shinpt->unget = 0;
  cur_shinpt->strpush = sp;
  if (alias)
    alias_depth++;
}

void
popstring(void)
{
  strpush *sp;
  sp = cur_shinpt->strpush;
  cur_shinpt->nchar = sp->saved_nchar;
  cur_shinpt->nleft = sp->saved_nleft;
  cur_shinpt->unget = sp->saved_unget;
  memcpy(cur_shinpt->ungetbuf, sp->saved_ungetbuf, 4);
  cur_shinpt->strpush = sp->prev;
  if (sp->alias)
    alias_depth--;
}

size_t
shgetline(char *buf, size_t sz)
{
  size_t i;
  int c;

  i = 0;
  while (i < sz - 1 && (c = shgetchar()) != SHEOF) {
    buf[i++] = c;
    if (c == '\n')
      break;
  }
  buf[i] = '\0';
  return i;
}

int
shungetc(int c)
{
  if (cur_shinpt->unget < 4) {
    return cur_shinpt->ungetbuf[cur_shinpt->unget++] = c;
  }
  return SHEOF;
}

void
setinputstrn(char *s, int len)
{
  shinput *new;
  new = st_alloc(sizeof(shinput));
  new->prev = cur_shinpt;
  new->buf = NULL;
  new->fd = -1;
  new->nleft = len;
  new->nchar = s;
  new->linenum = 1;
  new->unget = 0;
  new->mapsize = 0;
  new->strpush = NULL;
  cur_shinpt = new;
}

void
setinputf(int fd)
{
  void *map;
  struct stat st;
  if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0) {
    map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map != MAP_FAILED) {
      posix_madvise(map, st.st_size, POSIX_MADV_SEQUENTIAL);
      close(fd);
      shinput *new;
      new = st_alloc(sizeof(shinput));
      new->prev = cur_shinpt;
      new->buf = map;
      new->nchar = map;
      new->nleft = st.st_size;
      new->mapsize = st.st_size;
      new->fd = -1;
      new->linenum = 1;
      new->unget = 0;
      new->strpush = NULL;
      cur_shinpt = new;
      if (vflag)
        if ( write(STDERR_FILENO, map, st.st_size)<0)
          perror("write");
      return;
    }
  }
  shinput *new;
  new = st_alloc(sizeof(shinput));
  new->buf = st_alloc(BUFSIZ);
  new->prev = cur_shinpt;
  new->fd = fd;
  new->nleft = 0;
  new->nchar = new->buf;
  new->mapsize = 0;
  new->linenum = 1;
  new->unget = 0;
  new->strpush = NULL;
  cur_shinpt = new;
}
