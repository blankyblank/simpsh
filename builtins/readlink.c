#include "config.h"
#if ENABLE_READLINK
#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "arg.h"
#include "errmsg.h"
#include "utils.h"

int
readlinkcmd(char *argv[])
{
  size_t argc = 0;
  int nonl = 0;

  array_len(argv, argc);
  ARGBEGIN
  {
    case 'n':
      nonl = 1;
      break;
    default:
      return bad_opt(argv0, ARGC());
  }
  ARGEND

  for (size_t i = 0; i < argc; i++) {
    char buf[PATH_MAX];
    ssize_t len;
    if ((len = readlink(argv[i], buf, sizeof(buf) - 1)) < 0)
      return 1;
    buf[len] = '\0';
    if (nonl)
      fwrite(buf, 1, len, shout);
    else
      fputs(buf, shout);
  }
  return 0;
}
#endif /* if ENABLE_READLINK */
