#define _POSIX_C_SOURCE 200809L
#include <stdio.h>

#include "arg.h"
#include "builtins.h"
#include "utils.h"
#include "errmsg.h"

void print_lines(FILE *fp, int ln);

int
headcmd(char *argv[])
{
  int ln = 10, status = 0;
  char *f, *arg;
  size_t argc = 0;

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
      bad_opt(argv0, ARGC());
      return 1;
  }
  ARGEND

  if (!ln)
    return 0;

  f = *argv;
  if (f) {
    for (size_t i = 0; i < argc; i++) {
      FILE *fp;
      f = *argv++;
      if (!(fp = fopen(f, "r"))) {
        status = sherr(1, f, "could not access file");
        continue;
      }
      if (argc > 1)
        fprintf(shstdout,"%s==> %s <==\n", (i > 0) ? "\n" : "", f);
      print_lines(fp, ln);
      fclose(fp);
    }
    return status;
  }
  print_lines(shstdin, ln);
  return status;
}

void
print_lines(FILE *fp, int ln)
{
  char buf[BUFSIZ];
  while (ln > 0) {
    size_t n = fread(buf, 1, sizeof(buf), fp);
    if (n == 0)
      break;
    char *p = buf, *end = buf + n;
    while (p < end && ln > 0) {
      char *nl = memchr(p, '\n', end - p);
      if (nl) {
        fwrite(p, 1, nl - p + 1, shstdout);
        p = nl + 1;
        ln--;
      } else {
        fwrite(p, 1, end - p, shstdout);
        p = end;
      }
    }
  }
}
