#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>

#include "input.h"
#include "malloc.h"
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
  cur_shinpt = &base_shinput;
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
 
  if (shreadbuf() == 0)
    return SHEOF;
  cur_shinpt->nleft--;
  if (*cur_shinpt->nchar == '\n')
    cur_shinpt->linenum++;
  
  return *cur_shinpt->nchar++;
}

int
shreadbuf(void)
{
  int readbc;
  int i = 0;
  if (cur_shinpt->strn && cur_shinpt->strnpos < cur_shinpt->strnleft) {
    while (cur_shinpt->strnpos < cur_shinpt->strnleft) {
      cur_shinpt->buf[i] = cur_shinpt->strn[cur_shinpt->strnpos];
      cur_shinpt->strnpos++;
      i++;
    }
    cur_shinpt->nchar = cur_shinpt->buf;
    cur_shinpt->nleft = i;
    return i;
  }

  if (cur_shinpt->fd >= 0) {
    readbc = read(cur_shinpt->fd, cur_shinpt->buf, BUFSIZ);
    if (readbc == 0) {
      return 0;
    } else {
      cur_shinpt->nchar = cur_shinpt->buf;
      cur_shinpt->nleft = readbc;
      return readbc;
    }
  }

  return 0;
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
  new->buf = st_alloc(len);
  new->strn = s;
  new->strnpos = 0;
  new->strnleft = len;
  new->fd = -1;
  new->nleft = 0;
  new->nchar = new->buf;
  new->eof = 0;
  new->linenum = 1;
  new->unget = 0;
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
  cur_shinpt = new;
}
