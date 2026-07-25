#define _POSIX_C_SOURCE 200809L

#include <string.h>
#include "alloc.h"
#include "lineio.h"
#include "utils.h"

char *
lrread(lr_t *lr, size_t *len)
{
  char *seg, *line = NULL;
  size_t slen, off = 0, cap = 0;
  size_t nread;

  if (lr->pos >= lr->end) {
    if (!(nread = fread(lr->buf, 1, BUFSIZ, lr->fp)))
      return NULL;
    lr->end = nread;
    lr->pos = 0;
  }
  char *nl;

  if ((nl = memchr(lr->buf + lr->pos, '\n', lr->end - lr->pos))) {
    size_t llen = nl - (lr->buf + lr->pos);
    line = salloc(llen + 1);
    nmemcpy(line, lr->buf + lr->pos, llen);
    lr->pos = nl - lr->buf + 1;
    *len = llen;
    return line;
  }
  seg = lr->buf + lr->pos;
  slen = lr->end - lr->pos;
  cap = slen + 1;
  line = salloc(cap);
  memcpy(line, seg, slen);
  off = slen;

  for (;;) {
    if (!(nread = fread(lr->buf, 1, BUFSIZ, lr->fp))) {
      line[off] = '\0';
      *len = off;
      lr->pos = lr->end = 0;
      return line;
    }
    if ((nl = memchr(lr->buf, '\n', nread))) {
      size_t llen = 0;
      llen = nl - lr->buf;
      if (cap <= off + llen) {
        cap = off + llen + 1;
        line = srealloc(line, cap);
      }
      memcpy(line + off, lr->buf, llen);
      off += llen;
      line[off] = '\0';
      lr->pos = nl - lr->buf + 1;
      lr->end = nread;
      *len = off;
      return line;
    }
    if (cap <= off + nread) {
      cap = off + nread + 1;
      line = srealloc(line, cap);
    }
    memcpy(line + off, lr->buf, nread);
    off += nread;
  }
}

int
lrpeekc(lr_t *lr)
{
  size_t nread;

  if (lr->pos >= lr->end) {
    if (!(nread = fread(lr->buf, 1, BUFSIZ, lr->fp)))
      return EOF;
    lr->end = nread;
    lr->pos = 0;
  }
  return (unsigned char)lr->buf[lr->pos];
}

int
lrskip(lr_t *lr)
{
  size_t nread;
  char *nl;
  for (;;) {
    if (lr->pos >= lr->end) {
      if (!(nread = fread(lr->buf, 1, BUFSIZ, lr->fp)))
        return 0;
      lr->end = nread;
      lr->pos = 0;
    }
    if ((nl = memchr(lr->buf + lr->pos, '\n', lr->end - lr->pos))) {
      lr->pos = nl - lr->buf + 1;
      return 1;
    }
    lr->pos = lr->end = 0;
  }
}
