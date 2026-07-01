#ifndef INPUT_H
#define INPUT_H

#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include "lex.h"
#include "opts.h"
#include "simd.h"

typedef struct strpush strpush;
struct strpush {
    strpush *prev;
    char *saved_nchar;
    size_t saved_nleft;
    int saved_unget;
    unsigned char saved_ungetbuf[4];
    int alias;
};

/** holds unified input stream data */
typedef struct shinput shinput;
struct shinput {
  char *nchar;      /* Pointer to next unconsumed char in buf */
  char *name;       /* current filename */
  size_t nleft;     /* Remaining chars in buf */
  int unget;        /* number to pushback */
  char ungetbuf[4]; /* Small pushback buffer */
  char *buf;        /* The raw read buffer */
  shinput *prev;    /* Links to previous input */
  int fd;           /* File descriptor for script file input */
  int linenum;      /* Line counter for error reporting */
  union {
    size_t mapsize; /* size of mmaped file */
    size_t lleft;
  } b;
  strpush *strpush;
};

extern shinput *shinpt;
#define SHEOF -1
#define BASEBUFSIZE BUFSIZ
#define curline (shinpt ? shinpt->linenum : 0)
#define isfd() (shinpt && shinpt->fd >= 0)

#define popinput() \
  do { \
    if (shinpt->fd < 0 && shinpt->b.mapsize) \
      munmap(shinpt->buf, shinpt->b.mapsize); \
    shinpt = shinpt->prev; \
  } while (0)

extern int shungetc(int);
extern size_t shgetline(char *, size_t);
extern void setinputstrn(char *, int);
extern void setinputf(int, const char *, int);
extern void init_input(void);
static int shreadbuf(void);

static inline int
shgetchar(void)
{
  shinput *in = shinpt;
  if ((in->unget > 0)) {
    in->unget--;
    return (unsigned char)in->ungetbuf[shinpt->unget];
  }
  if (in->nleft > 0) {
    if (*in->nchar == '\n')
      in->linenum++;
    in->nleft--;
    return (unsigned char)*in->nchar++;
  }
  if (in->strpush) {
    popstring();
    return (unsigned char)shgetchar();
  }

  if (in->fd < 0 && in->b.mapsize)
    return SHEOF;

  if (in->fd >= 0 && in->b.lleft > 0) {
    size_t nl;
    nl = memchr_(in->nchar, in->b.lleft, '\n');
    if (nl < in->b.lleft) {
      in->nleft = nl + 1, in->b.lleft -= in->nleft;
    } else {
      in->nleft = in->b.lleft, in->b.lleft = 0;
    }
    in->nleft--;
    if (*in->nchar == '\n')
      in->linenum++;
    return (unsigned char)*in->nchar++;

  }

  if (shreadbuf() == 0)
    return SHEOF;
  in->nleft--;
  if (*in->nchar == '\n')
    in->linenum++;
  
  return (unsigned char)*in->nchar++;
}

int
shreadbuf(void)
{
  int readbc;
  size_t nl;

  if (shinpt->fd < 0)
    return 0;
  if ((readbc = read(shinpt->fd, shinpt->buf, BUFSIZ)) <= 0)
    return 0;
  if (vflag)
    if (write(STDERR_FILENO, shinpt->buf, readbc) < 0)
      perror("write");
  shinpt->nchar = shinpt->buf;
  nl = memchr_(shinpt->buf, readbc, '\n');
  if (nl < (size_t)readbc)
    shinpt->nleft = nl + 1, shinpt->b.lleft = readbc - (nl + 1);
  else
    shinpt->nleft = readbc, shinpt->b.lleft = 0;
  return readbc;
}

static inline size_t
shpeek(const char **p)
{
  if (shinpt->unget > 0)
    return 0;
  if (shinpt->nleft > 0) {
    *p = shinpt->nchar;
    return shinpt->nleft;
  }
  if (shinpt->strpush)
    return 0;
  if (shreadbuf()) {
    *p = shinpt->nchar;
    return shinpt->nleft;
  }
  return 0;
}

/* move forward in input by n, using simd */
static inline void
shadvance(size_t n)
{
  const char *buf;
  size_t pos;

  buf = shinpt->nchar;
  pos = 0;

  while (pos < n) {
    size_t found;
    found = memchr_(buf + pos, n - pos, '\n');
    if (found < n - pos) {
      shinpt->linenum++;
      pos += found + 1;
    } else {
      break;
    }
  }
  shinpt->nchar += n, shinpt->nleft -= n;
}

static inline int
shinput_avail(void)
{
  return shinpt->unget > 0 || shinpt->nleft > 0 || shinpt->b.lleft > 0
         || shinpt->strpush;
}

#endif  // INPUT_H
