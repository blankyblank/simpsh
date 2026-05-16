#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "input.h"
#include "malloc.h"
#include "lex.h"
#include "simpsh.h"

int shreadbuf(void);
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
  base_shinput.strn = NULL;
  base_shinput.strnpos = 0;
  base_shinput.strnleft = 0;
  base_shinput.fd = STDIN_FILENO;
  base_shinput.eof = 0;
  base_shinput.linenum = 1;
  base_shinput.unget = 0;
  base_shinput.strpush = NULL;
  cur_shinpt = &base_shinput;
}

void
pushstring(char *s, size_t len, int alias)
{
  strpush *sp = malloc(sizeof(strpush));
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
  free(sp);
}

int
shgetchar(void)
{
  if (cur_shinpt->unget > 0) {
    cur_shinpt->unget--;
    return cur_shinpt->ungetbuf[cur_shinpt->unget];
  }
  if (cur_shinpt->nleft > 0) {
    cur_shinpt->nleft--;
    return *cur_shinpt->nchar++;
  }
  if (cur_shinpt->strpush) {
    popstring();
    return shgetchar();
  }

  if (shreadbuf() == 0)
    return SHEOF;
  cur_shinpt->nleft--;
  if (*cur_shinpt->nchar == '\n')
    cur_shinpt->linenum++;
  
  return *cur_shinpt->nchar++;
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
shreadbuf(void)
{
  // WARN: This is worrying to me. check on this, does this even handle readline correctly??????
  int readbc;

  if (cur_shinpt->fd < 0)
    return 0;
  readbc = read(cur_shinpt->fd, cur_shinpt->buf, BUFSIZ);
  if (readbc <= 0)
    return 0;
  cur_shinpt->nchar = cur_shinpt->buf;
  cur_shinpt->nleft = readbc;
  return readbc;
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
  new->buf = NULL; // I GUESS????
  new->strn = s;
  new->strnpos = 0;
  new->strnleft = len;
  new->fd = -1;
  new->nleft = len;
  new->nchar = s;
  new->eof = 0;
  new->linenum = 1;
  new->unget = 0;
  new->strpush = NULL;
  cur_shinpt = new;
}

void
setinputf(int fd)
{
  shinput *new;
  new = st_alloc(sizeof(shinput));
  new->prev = cur_shinpt;
  new->buf = st_alloc(BUFSIZ);
  new->strn = NULL;
  new->strnpos = 0;
  new->strnleft = 0;
  new->fd = fd;
  new->nleft = 0;
  new->nchar = new->buf;
  new->eof = 0;
  new->linenum = 1;
  new->unget = 0;
  new->strpush = NULL;
  cur_shinpt = new;
}
