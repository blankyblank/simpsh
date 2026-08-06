#include "config.h"
#if ENABLE_WC
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>

#include "arg.h"
#include "errmsg.h"
#include "utils.h"

int
wccmd(char *argv[])
{
  /* TODO: add support for wide char counting */
  enum {
    ln = 1 << 0,
    wrd = 1 << 1,
    byt = 1 << 2,
    // chr = 1 << 3,
  };
  size_t argc = 0, nsrc;
  int status = 0, flags = 0;
  int tbyt = 0, tln = 0, twrd = 0;
  int nsel;
  // int tchr = 0;

  array_len(argv, argc);
  ARGBEGIN
  {
    case 'c':
      flags |= byt;
      break;
    case 'l':
      flags |= ln;
      break;
    // case 'm':
    //   flags |= chr;
    //   break;
    case 'w':
      flags |= wrd;
      break;
    default:
      return bad_opt(argv0, ARGC());
  }
  ARGEND
  if (!flags)
    flags |= ln | wrd | byt;

  nsrc = argc ? argc : 1;
  nsel = (flags & ln) ? 1 : 0;
  nsel += (flags & wrd) ? 1 : 0;
  nsel += (flags & byt) ? 1 : 0;

  for (size_t i = 0; i < nsrc; i++) {
    char *name, buf[BUFSIZ];
    FILE *fp;
    size_t n = 0;
    int nbyt = 0, nln = 0, nwrd = 0, inwrd = 0;
    // int nchr = 0;
    if (argc) {
      name = argv[i];
      if (!(fp = fopen(name, "r"))) {
        status = sherr(1, argv0, name);
        continue;
      }
    } else {
      name = NULL;
      fp = shin;
    }

    while ((n = fread(buf, 1, BUFSIZ, fp)) > 0) {
      for (size_t j = 0; j < n; j++) {
        unsigned char c = (unsigned char)buf[j];
        nbyt++;
        if (c == '\n')
          nln++;
        if (is_ws(c)) {
          inwrd = 0;
        } else if (!inwrd) {
          nwrd++;
          // nchr++;
          inwrd = 1;
        }
      }
    }
    tbyt += nbyt;
    tln += nln;
    // tchr += nchr;
    twrd += nwrd;

    if (flags & ln)
      printf("%4d", nln);
    if (flags & wrd)
      printf("%s%4d", (nsel > 1) ? " " : "", nwrd);
    if (flags & byt)
      printf("%s%4d", (nsel > 1) ? " " : "", nbyt);
    // if (flags & chr)
    //   printf("%7d", nchr);
    if (name)
      printf(" %s", name);
    putchar('\n');

    if (fp && fp != shin)
      fclose(fp);
  }
  if (argc > 1) {
    if (flags & ln)
      printf("%4d", tln);
    if (flags & wrd)
      printf("%s%4d", (nsel > 1) ? " " : "", twrd);
    if (flags & byt)
      printf("%s%4d", (nsel > 1) ? " " : "", tbyt);
    // if (flags & chr)
    //   printf("%4d", tchr);
    puts(" total");
  }
  return status;
}
#endif /* ENABLE_WC */
