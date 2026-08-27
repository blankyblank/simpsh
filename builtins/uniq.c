#include "config.h"
#if ENABLE_UNIQ
#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */

#include <stdio.h>

#include "arg.h"
#include "builtins.h"
#include "errmsg.h"
#include "lineio.h"
#include "utils.h"

static size_t nf;
static size_t nc;
static int flags;

enum {
  count = 1 << 0,
  dupe = 1 << 1,
  uniq = 1 << 2,
};

static size_t getkey(const char *line, size_t len);
static void prntuniq(const char *line, size_t len, size_t n);

int
uniqcmd(char *argv[])
{
  size_t argc = 0;
  int status = 0;
  FILE *ifp = NULL, *ofp = NULL;
  lr_t lr;

  nf = nc = flags = 0;
  array_len(argv, argc);
  ARGBEGIN
  {
    case 'c':
      flags |= count;
      break;
    case 'd':
      flags |= dupe;
      break;
    case 'u':
      flags |= uniq;
      break;
    case 'f':
      {
        char *nfstr;
        if (!(nfstr = EARGF(usage(argv0, helpmsgs[UNIQH].usage))))
          return 1;
        int tmpnf;
        if ((tmpnf = bltin_atoi(nfstr, argv0, "must be a positive integer")) <= 0) {
          if (!tmpnf)
            shwarn_arg(argv0, nfstr, "must be a positive integer");
          return 1;
        }
        nf = (size_t)tmpnf;
      }
      break;
    case 's':
      {
        char *ncstr;
        int tmpnc;
        if (!(ncstr = EARGF(usage(argv0, helpmsgs[UNIQH].usage))))
          return 1;
        if ((tmpnc = bltin_atoi(ncstr, argv0, "must be a positive integer")) <= 0) {
          if (!tmpnc)
            shwarn_arg(argv0, ncstr, "must be a positive integer");
          return 1;
        }
        nc = (size_t)tmpnc;
      }
      break;
    default:
      return bad_opt(argv0, ARGC());
  }
  ARGEND
  {
    int m = flags & (count|dupe|uniq);
    if (m & (m - 1))
      return shwarn(argv0, "Only one of -c, -d, -u, allowed");
  }

  switch (argc) {
    case 0:
      ifp = lropen(&lr, NULL);
      ofp = shout;
      break;
    case 1:
      if (!(ifp = lropen(&lr, *argv)))
        return sherr(1, argv0, *argv);
      ofp = shout;
      break;
    case 2:
      if (!(ifp = lropen(&lr, *argv)))
        return sherr(1, argv0, *argv);
      if (!(ofp = fopen(argv[1], "w")))
        return sherr(1, argv0, argv[1]);
      break;
    default:
      return usage(argv0, helpmsgs[UNIQH].usage);
  }


  char *line, *oline = NULL;
  size_t llen = 0, olen = 0, key, okey = 0, cnt = 0;


  while ((line = lrread(&lr, &llen))) {
    key = getkey(line, llen);
    if (oline && llen - key == olen - okey &&
        !memcmp(line + key, oline + okey, llen - key)) {
      cnt++;
      continue;
    }
    prntuniq(oline, olen, cnt);
    oline = line;
    olen = llen;
    okey = key;
    cnt = 1;
  }
  prntuniq(oline, olen, cnt);

  if (ifp != shin)
    fclose(ifp);
  if (ofp != shout)
    fclose(ofp);
  return status;
}

static size_t
getkey(const char *line, size_t len)
{
  size_t off;

  off = 0;
  for (size_t i = 0; i < nf; i++) {
    while (off < len && is_ws(line[off]))
      off++;
    while (off < len && !is_ws(line[off]))
      off++;
  }
  off += nc;
  if (off > len)
    off = len;
  return off;
}

static void
prntuniq(const char *line, size_t len, size_t n)
{
  if (!n)
    return;
  if ((flags & dupe) && n == 1)
    return;
  if ((flags & uniq) && n > 1)
    return;

  char buf[21];

  if ((flags & count)) {
    size_t clen = lltoa((i64)n, buf);
    fwrite(buf, 1, clen, shout);
    fputc(' ', shout);
  }
  fwrite(line, 1, len, shout);
  fputc('\n', shout);
}


#endif /* ENABLE_UNIQ */



















