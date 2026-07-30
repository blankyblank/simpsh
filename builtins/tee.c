#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <sys/stat.h>

#include "arg.h"
#include "builtins.h"
#include "errmsg.h"
#include "utils.h"

int
teecmd(char *argv[])
{
  int append = 0, status = 0;
  int bufsize = BUFSIZ, n = 0;
  char *buf;
  size_t argc = 0;
  struct stat st;
  FILE **files = NULL;

  array_len(argv, argc);
  ARGBEGIN
  {
    case 'a':
      append = 1;
      break;
    default:
      return bad_opt(argv0, ARGC());
  }
  ARGEND

  if (argc) {
    files = st_alloc(argc * sizeof(FILE *));
    for (size_t i = 0; i < argc; i++) {
      if (!(files[i] = fopen(argv[i], append ? "a" : "w")))
        status = sherr(1, argv[i], "could not open");
    }
    for (size_t i = 0; i < argc; i++)
      if (files[i]) {
        bufsize = GETBLKSIZE(files[i], st);
        break;
      }
    buf = salloc(bufsize);
    while ((n = fread(buf, 1, bufsize, shin)) > 0) {
      fwrite(buf, 1, n, shout);
      for (size_t i = 0; i < argc; i++)
        if (files[i])
          fwrite(buf, 1, n, files[i]);
    }
    if (ferror(shin))
      status = sherr(1, argv0, "read error\n");
    for (size_t i = 0; i < argc; i++)
      fclose(files[i]);
    if (buf)
      sfree(buf);
    return status;
  }
  buf = salloc(bufsize);
  while ((n = fread(buf, 1, bufsize, shin)) > 0) {
    fwrite(buf, 1, n, shout);
  }
  if (ferror(shin))
    status = sherr(1, argv0, "read error");
  if (buf)
    sfree(buf);
  return status;
}
