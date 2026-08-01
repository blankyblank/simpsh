#include "config.h"
#if ENABLE_REALPATH
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdlib.h>
#include <limits.h>
#include <stdio.h>

#include "arg.h"
#include "errmsg.h"
#include "utils.h"

int
realpathcmd(char *argv[])
{
  size_t argc = 0;
  int quiet = 0;

  array_len(argv, argc);
  ARGBEGIN
  {
    case 'q':
      quiet = 1;
      break;
    default:
      return bad_opt(argv0, ARGC());
  }
  ARGEND

  int status = 0;
  for (size_t i = 0; i < argc; i++) {
    char buf[PATH_MAX];
    if (!realpath(argv[i], buf)) {
      status = (quiet) ? 1 : sherr(1, argv0, argv[i]);
      continue;
    }
    puts(buf);
  }
  return status;
}
#endif /* ENABLE_REALPATH */
