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
shinput *shinpt = &base_shinput;

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
  base_shinput.b.mapsize = 0;
  shinpt = &base_shinput;
}

void
pushstring(char *s, size_t len, int alias)
{
  strpush *sp = st_alloc(sizeof(strpush));
  sp->prev = shinpt->strpush;
  sp->saved_nchar = shinpt->nchar;
  sp->saved_nleft = shinpt->nleft;
  sp->saved_unget = shinpt->unget;
  memcpy(sp->saved_ungetbuf, shinpt->ungetbuf, 4);
  sp->alias = alias;
  shinpt->nchar = s;
  shinpt->nleft = len;
  shinpt->unget = 0;
  shinpt->strpush = sp;
  if (alias)
    alias_depth++;
}

void
popstring(void)
{
  strpush *sp;
  sp = shinpt->strpush;
  shinpt->nchar = sp->saved_nchar;
  shinpt->nleft = sp->saved_nleft;
  shinpt->unget = sp->saved_unget;
  memcpy(shinpt->ungetbuf, sp->saved_ungetbuf, 4);
  shinpt->strpush = sp->prev;
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
  if (shinpt->unget < 4) {
    return shinpt->ungetbuf[shinpt->unget++] = c;
  }
  return SHEOF;
}

void
setinputstrn(char *s, int len)
{
  shinput *new;
  new = st_alloc(sizeof(shinput));
  new->prev = shinpt;
  new->buf = NULL;
  new->fd = -1;
  new->nleft = len;
  new->nchar = s;
  new->linenum = 1;
  new->unget = 0;
  new->b.mapsize = 0;
  new->b.lleft = 0;
  new->strpush = NULL;
  shinpt = new;
}

void
setinputf(int fd, const char *name, int nmp)
{
  if (!nmp) {
    void *map;
    struct stat st;
    if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0) {
      map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
      if (map != MAP_FAILED) {
        posix_madvise(map, st.st_size, POSIX_MADV_SEQUENTIAL);
        close(fd);
        shinput *new;
        new = st_alloc(sizeof(shinput));
        new->prev = shinpt;
        new->buf = map;
        new->name = name ? st_strdup(name) : NULL;
        new->nchar = map;
        new->nleft = st.st_size;
        new->b.mapsize = st.st_size;
        new->fd = -1;
        new->linenum = 1;
        new->unget = 0;
        new->strpush = NULL;
        shinpt = new;
        if (vflag)
          if (write(STDERR_FILENO, map, st.st_size) < 0)
            perror("write");
        return;
      }
    }
  }
  shinput *new;
  new = st_alloc(sizeof(shinput));
  new->buf = st_alloc(BUFSIZ);
  new->name = name ? st_strdup(name) : NULL;
  new->b.lleft = 0;
  new->prev = shinpt;
  new->fd = fd;
  new->nleft = 0;
  new->nchar = new->buf;
  new->b.mapsize = 0;
  new->linenum = 1;
  new->unget = 0;
  new->strpush = NULL;
  shinpt = new;
}

