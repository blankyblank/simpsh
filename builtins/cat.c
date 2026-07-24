#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <sys/stat.h>

#include "errmsg.h"
#include "builtins.h"
#include "utils.h"

int
catcmd(char **argv)
{
  int n = 0, argc = 0, i = 1, bufsize = BUFSIZ;
  FILE *f = NULL;
  struct stat st;
  char *buf = NULL;

  array_len(argv, argc);

  if (argc == 1) {
    buf = salloc(bufsize);
    while ((n = fread(buf, 1, bufsize, shstdin)) > 0)
      fwrite(buf, 1, n, shstdout);
    if (ferror(shstdin))
      return sherr(1, argv[0], "Bad file descriptor\n");
    if (buf)
      sfree(buf);
    return 0;
  }

  while (i < argc) {
    if (!(f = fopen(argv[i], "r")))
      return sherr(1, argv[i], "could not access file");
    bufsize = GETBLKSIZE(f, st);
    buf = salloc(bufsize);
    while ((n = fread(buf, 1, bufsize, f)) > 0)
      fwrite(buf, 1, n, shstdout);
    fclose(f);
    i++;
  }
  if (buf)
    sfree(buf);
  return 0;
}
