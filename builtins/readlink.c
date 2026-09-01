#include "config.h"
#if ENABLE_READLINK
#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */
#define _XOPEN_SOURCE 700

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "arg.h"
#include "errmsg.h"
#include "utils.h"

static int freadlink(const char *, char *);

int
readlinkcmd(char *argv[])
{
  size_t argc;
  int ffl;
  int nonl;

  ffl = nonl = argc = 0;
  array_len(argv, argc);
  ARGBEGIN
  {
    case 'n':
      nonl = 1;
      break;
    case 'f':
      ffl = 1;
      break;
    default:
      return bad_opt(argv0, ARGC());
  }
  ARGEND

  if (ffl) {
    for (size_t i = 0; i < argc; i++) {
      char buf[PATH_MAX];
      size_t blen;
      if (freadlink(argv[i], buf))
        return 1;
      blen = strlen(buf);
      if (nonl) {
        fwrite(buf, 1, blen, shout);
      } else {
        fputs(buf, shout);
        fputc('\n', shout);
      }
    }
    return 0;
  }

  for (size_t i = 0; i < argc; i++) {
    char buf[PATH_MAX];
    ssize_t len;
    if ((len = readlink(argv[i], buf, sizeof(buf) - 1)) < 0)
      return 1;
    buf[len] = '\0';
    if (nonl) {
      fwrite(buf, 1, len, shout);
    } else {
      fputs(buf, shout);
      fputc('\n', shout);
    }
  }
  return 0;
}

static int
freadlink(const char *path, char *buf)
{
  if (realpath(path, buf)) {
    return 0;
  }
  if (errno != ENOENT)
    return 1;

  char target[PATH_MAX];
  ssize_t len;

  if ((len = readlink(path, target, PATH_MAX - 1)) >= 0) {
    target[len] = '\0';
    if (*target != '/') {
      char cwdbuf[PATH_MAX], tmp[PATH_MAX];
      if (!getcwd(cwdbuf, sizeof(cwdbuf)))
        return 1;
      if ((snprintf(tmp, sizeof(tmp), "%s/%s", cwdbuf, target)) >=
          (int)sizeof(tmp))
        return 1;
      return freadlink(tmp, buf);
    }
    return freadlink(target, buf);
  }

  char work[PATH_MAX], *last;
  size_t blen, slen, wlen;

  if ((wlen = strlen(path)) >= sizeof(work))
    return 1;
  memcpy(work, path, wlen + 1);
  while (wlen > 1 && work[wlen - 1] == '/')
    work[--wlen] = '\0';
  last = strrchr(work, '/');
  if (!last)
    return 1;

  if (last == work) {
    if (!realpath("/", buf))
      return 1;
    blen = strlen(buf);
    slen = strlen(work + 1);
    if (blen + slen >= PATH_MAX)
      return 1;
    memcpy(buf + blen, work + 1, slen + 1);
  } else {
    *last  = '\0';
    if (!realpath(work, buf))
      return 1;
    *last = '/';
    blen = strlen(buf), slen = strlen(last);
    if (blen + slen >= PATH_MAX)
      return 1;
    memcpy(buf + blen, last, slen + 1);
  }
  return 0;
}

#endif /* if ENABLE_READLINK */
