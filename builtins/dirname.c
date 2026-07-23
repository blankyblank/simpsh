#define _POSIX_C_SOURCE 200809L

#include <libgen.h>
#include <stdio.h>

#include "errmsg.h"

int
dirnamecmd(char *argv[])
{
  char *argv0;
  argv0 = *argv++;
  if (!*argv)
    return shwarn(argv0, "missing argument");

  for (char *d = *argv++; d; d = *argv++) {
    char *path;
    if ((path = dirname(d)))
      puts(path);
  }
  return 0;
}
