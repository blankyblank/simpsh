#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include "errmsg.h"

int
basenamecmd(char *argv[])
{
  char *path, *p, *suff = NULL;
  char *argv0;
  size_t len = 0, slen = 0;

  argv0 = *argv++;
  if (!*argv)
    return shwarn(argv0, "missing argument");

  path = *argv++;
  if (path[0] == '\0') {
    puts("");
    return 0;
  }
  len = strlen(path);
  while (len > 1 && path[len - 1] == '/')
    path[--len] = '\0';

  if ((p = strrchr(path, '/'))) {
    path = p + 1;
    len = strlen(path);
  }
  if (!len) {
    path = "/";
    len = 1;
  }

  if ((suff = *argv))
    slen = strlen(suff);
  if (slen > 0 && slen < len && !memcmp(path + len - slen, suff, slen))
    len -= slen;

  printf("%.*s\n", (int)len, path);
  return 0;
}
