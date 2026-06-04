#ifndef INPUT_H
#define INPUT_H

#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include "lex.h"
#include "opts.h"

typedef struct strpush strpush;
struct strpush {
    strpush *prev;
    char *saved_nchar;
    size_t saved_nleft;
    int saved_unget;
    char saved_ungetbuf[4];
    int alias;
};

/** holds unified input stream data */
typedef struct shinput shinput;
struct shinput {
  char *nchar;     /* Pointer to next unconsumed char in buf */
  size_t nleft;    /* Remaining chars in buf */
  int unget;        /* number to pushback */
  char ungetbuf[4]; /* Small pushback buffer */
  char *buf;       /* The raw read buffer */
  shinput *prev;   /* Links to previous input */
  int fd;          /* File descriptor for script file input */
  int linenum;     /* Line counter for error reporting */
  size_t mapsize;  /* size of mmaped file */
  strpush *strpush;
};

extern shinput *cur_shinpt;
#define SHEOF -1
#define BASEBUFSIZE BUFSIZ
#define shinput_linenum() (cur_shinpt ? cur_shinpt->linenum : 0)
#define shinput_isfile() (cur_shinpt && cur_shinpt->fd >= 0)
// #define popinput() (cur_shinpt = cur_shinpt->prev)

#define popinput() \
  do { \
    if (cur_shinpt->mapsize) \
      munmap(cur_shinpt->buf, cur_shinpt->mapsize); \
    cur_shinpt = cur_shinpt->prev; \
  } while (0)

// extern int shgetchar(void);
extern int shungetc(int);
extern size_t shgetline(char *, size_t);
extern void setinputstrn(char *, int);
extern void setinputf(int);
extern void init_input(void);
static int shreadbuf(void);

static inline int
shgetchar(void)
{
  shinput *in = cur_shinpt;
  if ((in->unget > 0)) {
    in->unget--;
    return in->ungetbuf[cur_shinpt->unget];
  }
  if (in->nleft > 0) {
    in->nleft--;
    return *in->nchar++;
  }
  if (in->strpush) {
    popstring();
    return shgetchar();
  }

  if (in->mapsize)
    return SHEOF;
  if (shreadbuf() == 0)
    return SHEOF;
  in->nleft--;
  if (*in->nchar == '\n')
    in->linenum++;
  
  return *in->nchar++;
}

int
shreadbuf(void)
{
  int readbc;

  if (cur_shinpt->fd < 0)
    return 0;
  if ((readbc = read(cur_shinpt->fd, cur_shinpt->buf, BUFSIZ)) <= 0)
    return 0;
  if (vflag)
    if (write(STDERR_FILENO, cur_shinpt->buf, readbc) < 0)
      perror("write");
  cur_shinpt->nchar = cur_shinpt->buf;
  cur_shinpt->nleft = readbc;
  return readbc;
}

static inline size_t
shpeek(const char **p)
{
  if (cur_shinpt->unget > 0)
    return 0;
  if (cur_shinpt->nleft > 0) {
    *p = cur_shinpt->nchar;
    return cur_shinpt->nleft;
  }
  if (cur_shinpt->strpush)
    return 0;
  // if (cur_shinpt->mapsize)
  //   return 0;
  if (shreadbuf()) {
    *p = cur_shinpt->nchar;
    return cur_shinpt->nleft;
  }
  return 0;
}

static inline void
shadvance(size_t n)
{
  // XXX: move to simd_memchr_eq
  for (size_t i = 0; i < n; i++)
    if (cur_shinpt->nchar[i] == '\n')
      cur_shinpt->linenum++;
  cur_shinpt->nchar += n;
  cur_shinpt->nleft -= n;
}

#endif  // INPUT_H
