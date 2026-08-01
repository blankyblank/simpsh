#include "config.h"
#if ENABLE_CUT
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>

#include "arg.h"
#include "errmsg.h"
#include "lineio.h"
#include "utils.h"

typedef struct {
  int start;
  int end;
} range;

static range * getrange(char *list, size_t *restrict nrange);
static int find_field(const char *line, int fldno, char delim, const char **fpos, size_t *len);
static int cutfld(const char *line, size_t len, range *rngs, size_t nr, char delim, int sfl);
static int cutbt(const char *line, size_t len, range *rngs, size_t nr);

int
cutcmd(char **argv)
{
  range *rngs;
  size_t nr, argc = 0;
  char *list = NULL;
  char delim = '\t';
  int mode, flags = 0, status = 0;

  enum {
    bfl = 1 << 0,
    cfl = 1 << 1,
    dfl = 1 << 2,
    ffl = 1 << 3,
    sfl = 1 << 4,
  };

  array_len(argv, argc);
  ARGBEGIN
  {
    case 'b':
      {
        char *arg;
        if (!(arg = EARGF(usage(argv0, helpmsgs[CUTH].usage))))
          return 1;
        list = arg;
        flags |= bfl;
        break;
      }
    case 'c':
      {
        char *arg;
        if (!(arg = EARGF(usage(argv0, helpmsgs[CUTH].usage))))
          return 1;
        list = arg;
        flags |= cfl;
        break;
      }
    case 'd':
      {
        char *arg;
        if (!(arg = EARGF(usage(argv0, helpmsgs[CUTH].usage))))
          return 1;
        delim = *arg;
        flags |= dfl;
        break;
      }
    case 'f':
      {
        char *arg;
        if (!(arg = EARGF(usage(argv0, helpmsgs[CUTH].usage))))
          return 1;
        list = arg;
        flags |= ffl;
        break;
      }
    case 's':
      flags |= sfl;
      break;
    default:
      return bad_opt(argv0, ARGC());
  }
  ARGEND

  if ((flags & sfl) && !(flags & ffl))
    return shwarn(argv0, "-s only valid with -f");
  mode = flags & (bfl | cfl | ffl);
  if (mode & (mode - 1))
    return shwarn(argv0, "only one of -b, -c, -f allowed");
  if (!mode)
    return shwarn(argv0, "you must specify a list of bytes, characters, or fields");

  char *line = NULL;
  size_t llen;
  lr_t lr;

  if (!(rngs = getrange(list, &nr)))
    return 1;

  if (!*argv) {
    lr.fp = shin;
    lr.pos = lr.end = 0;
    while ((line = lrread(&lr, &llen))) {
      if (flags & ffl)
        cutfld(line, llen, rngs, nr, delim, (flags & sfl));
      else
        cutbt(line, llen, rngs, nr);
      sfree(line);
    }
    return 0;
  }

  for (size_t i = 0; i < argc; i++) {
    if (!lropen(&lr, argv[i])) {
      status = 1;
      sherr(1, argv[i], "could not access file");
      continue;
    }
    while ((line = lrread(&lr, &llen))) {
      if (flags & ffl)
        cutfld(line, llen, rngs, nr, delim, (flags & sfl));
      else
        cutbt(line, llen, rngs, nr);
      sfree(line);
    }
    fclose(lr.fp);
  }
  return status;
}

static int
cutfld(const char *line, size_t len, range *rngs, size_t nr, char delim, int sfl)
{
  int outp = 0;

  if (!(memchr(line, delim, len))) {
    if (sfl)
      return 0;
    fwrite(line, 1, len, shout);
    fputc('\n', shout);
    return 0;
  }
  for (size_t i = 0; i < nr; i++) {
    for (int f = rngs[i].start; f <= rngs[i].end || rngs[i].end == -1; f++) {
      const char *fpos;
      size_t flen;
      if (!find_field(line, f, delim, &fpos, &flen))
        break;
      if (outp)
        fputc(delim, shout);
      fwrite(fpos, 1, flen, shout);
      outp = 1;
    }
  }
  if (outp)
    fputc('\n', shout);
  return 1;
}

static int
cutbt(const char *line, size_t len, range *rngs, size_t nr)
{
  int outp = 0;

  for (size_t i = 0; i < nr; i++) {
    long bstart, bend;
    bstart = rngs[i].start - 1;
    if (bstart >= (long)len)
      continue;
    if (rngs[i].end > 0)
      bend = (rngs[i].end < (long)len) ? rngs[i].end : (long)len;
    else
      bend = len;
    fwrite(line + bstart, 1, bend - bstart, shout);
    outp = 1;
  }
  if (outp)
    fputc('\n', shout);
  return outp;
}


static int
find_field(const char *line, int fldno, char delim, const char **fpos, size_t *len)
{
  size_t cur = 1, pos, fstart = 0;

  for (pos = 0;; pos++) {
    if (line[pos] == delim || line[pos] == '\0') {
      if (cur == (size_t)fldno) {
        *fpos = line + fstart;
        *len = pos - fstart;
        return 1;
      }
      if (line[pos] == '\0')
        return 0;
      cur++;
      fstart = pos + 1;
    }
  }
  return 0;
}

static range *
getrange(char *list, size_t *restrict nrange)
{
  range *rngs;
  int ccnt = 0;
  size_t idx = 0;
  char *cur, *start, *end;

  for (cur = list; *cur; cur++)
    if (*cur == ',')
      ccnt++;

  rngs = salloc(++ccnt * sizeof(range));

  for (cur = list; *cur; cur++) {
    char *dash, sv;
    int s, e;
    start = cur;
    while (*cur != ',' && *cur != '\0')
      cur++;
    end = cur;
    dash = start;
    while (dash < end && *dash != '-')
      dash++;
    if (dash == end) {
      sv = *end, *end = '\0';
      if ((s = atoi_(start)) <= 0)
        return NULL;
      rngs[idx++] = (range) { s, s };
      *end = sv;
    } else if (dash == start) {
      start++;
      sv = *end, *end = '\0';
      if ((e = atoi_(start)) <= 0)
        return NULL;
      rngs[idx++] = (range) { 1, e };
      *end = sv;
    } else if (dash == (end - 1)) {
      sv = *dash, *dash = '\0';
      if ((s = atoi_(start)) <= 0)
        return NULL;
      rngs[idx++] = (range) { s, -1 };
      *dash = sv;
    } else {
      sv = *dash, *dash = '\0';
      s = atoi_(start);
      *dash = sv;
      sv = *end, *end = '\0';
      e = atoi_(dash + 1);
      *end = sv;
      if (s <= 0 || e <= 0 || e < s)
        return NULL;
      rngs[idx++] = (range) { s, e };
    }
  }

  *nrange = idx;
  return rngs;
}
#endif /* ENABLE_CUT */
