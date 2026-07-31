#ifndef LINEIO_H
#define LINEIO_H

#include <stdio.h>

#include "main.h"

typedef struct {
  FILE *fp;
  char buf[BUFSIZ];
  size_t pos;
  size_t end;
} lr_t;

static inline FILE *
lropen(lr_t *lr, const char *path)
{
  FILE *fp = NULL;
  if (!path) {
    lr->fp = shin;
  } else {
    if (!(fp = fopen(path, "r")))
      return NULL;
    lr->fp = fp;
  }
  lr->pos = 0;
  lr->end = 0;
  return lr->fp;
}

extern char *lrread(lr_t *lr, size_t *len);
extern int lrskip(lr_t *lr);
extern int lrpeekc(lr_t *lr);
 
#endif /* LINEIO_H */
