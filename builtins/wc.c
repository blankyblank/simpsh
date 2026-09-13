#include "config.h"
#if ENABLE_WC
#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "arg.h"
#include "errmsg.h"
#include "simdext.h"
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
  size_t argc, nsrc;
  int status, flags;
  int tbyt, tln, twrd;
  int nsel, stdin_reg;
  int w, *lns, *wrds, *byts;
  struct stat st;
  // int tchr = 0;

  argc = status = flags = stdin_reg = 0;
  tbyt = tln = twrd = 0;

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
  lns = st_alloc(nsrc * sizeof(int));
  wrds = st_alloc(nsrc * sizeof(int));
  byts = st_alloc(nsrc * sizeof(int));
  nsel = (flags & ln) ? 1 : 0;
  nsel += (flags & wrd) ? 1 : 0;
  nsel += (flags & byt) ? 1 : 0;
  if (!argc && nsel > 1)
    stdin_reg = (!fstat(fileno(shin), &st) && S_ISREG(st.st_mode));
  for (size_t i = 0; i < nsrc; i++)
    lns[i] = wrds[i] = byts[i] = -1;

  for (size_t i = 0; i < nsrc; i++) {
    char *name, buf[BUFSIZ];
    int nbyt, nln, nwrd, inwrd;
    // int nchr = 0;
    FILE *fp;
    size_t n;

    n = nbyt = nln = nwrd = inwrd = 0;
    name = argv[i] ? argv[i] : NULL;
    fp = argc ? fopen(name, "r") : shin;
    if (!fp) {
      status = sherr(1, argv0, name);
      continue;
    }
    if (flags == byt && !fstat(fileno(fp), &st) && S_ISREG(st.st_mode)) {
      off_t pos;
      pos = (fp == shin) ? lseek(fileno(fp), 0, SEEK_CUR) : 0;
      nbyt = (pos >= 0 && st.st_size > pos) ? (int)(st.st_size - pos) : 0;
    } else {
      while ((n = fread(buf, 1, BUFSIZ, fp)) > 0) {
        nbyt += (int)n;
        if (flags & ln)
          nln += (int)scntnl(buf, n);
        if (flags & wrd)
          nwrd += (int)scntwords(buf, n, &inwrd);
      }
    }

    tbyt += nbyt;
    tln += nln;
    // tchr += nchr;
    twrd += nwrd;
    lns[i] = nln;
    wrds[i] = nwrd;
    byts[i] = nbyt;

    if (fp && fp != shin)
      fclose(fp);
  }

  {
    w = 1;
    if (!(nsel == 1 && argc <= 1)) {
      int m;
      m = 0;
      if ((flags & ln) && tln > m)
        m = tln;
      if ((flags & wrd) && twrd > m)
        m = twrd;
      if ((flags & byt) && tbyt > m)
        m = tbyt;
      while (m >= 10)
        m /= 10, w++;
      if (w < 7 && !argc && !stdin_reg && nsel > 1)
        w = 7;
    }
  }

  for (size_t i = 0; i < nsrc; i++) {
    if (lns[i] < 0)
      continue;
    if (flags & ln)
      fprintf(shout, "%*d", w, lns[i]);
    if (flags & wrd)
      fprintf(shout, "%s%*d", (nsel > 1) ? " " : "", w, wrds[i]);
    if (flags & byt)
      fprintf(shout, "%s%*d", (nsel > 1) ? " " : "", w, byts[i]);
    // if (flags & chr)
    //   printf("%7d", nchr);
    if (argc)
      fprintf(shout, " %s", argv[i]);
    fputc('\n', shout);
  }

  if (argc > 1) {
    if (flags & ln)
      fprintf(shout, "%*d", w, tln);
    if (flags & wrd)
      fprintf(shout, "%s%*d", (nsel > 1) ? " " : "", w, twrd);
    if (flags & byt)
      fprintf(shout, "%s%*d", (nsel > 1) ? " " : "", w, tbyt);
    // if (flags & chr)
    //   printf("%4d", tchr);
    fputs(" total", shout);
  }
  return status;
}
#endif /* ENABLE_WC */
