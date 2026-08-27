#include "config.h"
#if ENABLE_HEAD
#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */
#include <stdio.h>

#include "arg.h"
#include "builtins.h"
#include "lineio.h"
#include "utils.h"
#include "errmsg.h"

static inline void
headbytc(lr_t *lr, int n)
{
  char buf[BUFSIZ];
  while (n > 0) {
    size_t r;
    r = fread(buf, 1, (size_t)n < sizeof(buf) ? (size_t)n : sizeof(buf), lr->fp);
    if (!r)
      break;
    fwrite(buf, 1, r, shout);
    n -= (int)r;
  }
}

int
headcmd(char *argv[])
{
  int cfl, svln, ln, status;
  char *f;
  size_t argc;
  lr_t lr;
  stmark hm;

  ln = 10;
  argc = cfl = status = 0;
  array_len(argv, argc);
  ARGBEGIN
  {
    char *arg;
    case 'c':
      if (!(arg = EARGF(usage(argv0, helpmsgs[HEADH].usage))))
        return 1;
      ln = bltin_atoi(arg, argv0, "requires a number");
      cfl = 1;
      break;
    case 'n':
      if (!(arg = EARGF(usage(argv0, helpmsgs[HEADH].usage))))
        return 1;
      ln = bltin_atoi(arg, argv0, "requires a number");
      break;
ARGNUM:
      ln = ARGNUMF();
      break;
    default:
      return bad_opt(argv0, ARGC());
  }
  ARGEND

  if (!ln)
    return 0;

  f = *argv;
  svln = ln;
  if (f) {
    hm = stack_mark();
    for (size_t i = 0; i < argc; i++) {
      FILE *fp;
      f = *argv++;
      ln = svln;
      if (!(fp = lropen(&lr, f))) {
        status = sherr(1, f, "could not access file");
        continue;
      }
      if (argc > 1)
        fprintf(shout,"%s==> %s <==\n", (i > 0) ? "\n" : "", f);
      if (cfl) {
        headbytc(&lr, ln);
      } else {
        while (ln > 0) {
          char *line;
          size_t len;
          if (!(line = lrread(&lr, &len)))
            break;
          fwrite(line, 1, len, shout);
          fputc('\n', shout);
          ln--;
          stack_restore(hm);
        }
      }
      fclose(fp);
    }
    stack_restore(hm);
    return status;
  }

  lr.fp = shin;
  lr.pos = lr.end = 0;
  hm = stack_mark();
  if (cfl) {
    headbytc(&lr, ln);
    return status;
  }
  while (ln > 0) {
    char *line;
    size_t len;
    if (!(line = lrread(&lr, &len)))
      break;
    fwrite(line, 1, len, shout);
    fputc('\n', shout);
    ln--;
    stack_restore(hm);
  }
  return status;
}
#endif /* if ENABLE_HEAD */
