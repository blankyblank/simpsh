#define _POSIX_C_SOURCE 200809L
#include <stdio.h>

#include "arg.h"
#include "builtins.h"
#include "lineio.h"
#include "utils.h"
#include "errmsg.h"

int
headcmd(char *argv[])
{
  int ln = 10, status = 0;
  char *f, *arg;
  size_t argc = 0;
  lr_t lr;

  array_len(argv, argc);
  ARGBEGIN
  {
    case 'n':
      if (!(arg = EARGF(usage(argv0, "head [-n number] [file...]"))))
        return 1;
      ln = bltin_atoi(arg, argv0, "requires a number");
      break;
ARGNUM:
      ln = atoi_(*argv);
      break;
    default:
      return bad_opt(argv0, ARGC());
  }
  ARGEND

  if (!ln)
    return 0;

  f = *argv;
  if (f) {
    for (size_t i = 0; i < argc; i++) {
      FILE *fp;
      f = *argv++;
      if (!(fp = lropen(&lr, f))) {
        status = sherr(1, f, "could not access file");
        continue;
      }
      if (argc > 1)
        fprintf(shout,"%s==> %s <==\n", (i > 0) ? "\n" : "", f);
      while (ln > 0) {
        char *line;
        size_t len;
        if (!(line = lrread(&lr, &len)))
          break;
        fwrite(line, 1, len, shout);
        fputc('\n', shout);
        ln--;
      }
      fclose(fp);
    }
    return status;
  }

  lr.fp = shin;
  lr.pos = lr.end = 0;
  while (ln > 0) {
    char *line;
    size_t len;
    if (!(line = lrread(&lr, &len)))
      break;
    fwrite(line, 1, len, shout);
    fputc('\n', shout);
    ln--;
  }
  return status;
}
