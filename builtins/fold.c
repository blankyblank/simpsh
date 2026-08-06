#include "config.h"
#if ENABLE_FOLD
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>

#include "arg.h"
#include "errmsg.h"
#include "utils.h"

int
foldcmd(char *argv[])
{
  enum {
    spc = 1 << 0,
    byt = 1 << 1,
  };
  size_t nsrc, argc = 0, w = 80;
  int status = 0, flags = 0;

  array_len(argv, argc);
  ARGBEGIN
  {
    case 'w':
      {
        char *wdth = EARGF(usage(argv0, helpmsgs[FOLDH].usage));
        long tmpw;
        if ((tmpw = atoi_(wdth)) <= 0)
          return shwarn_arg(argv0, wdth, "must be a positive integer");
        w = (size_t)tmpw;
      }
      break;
    case 's':
      flags |= spc;
      break;
    case 'b':
      flags |= byt;
      break;
    default:
      return bad_opt(argv0, ARGC());
  }
  ARGEND

  nsrc = (argc) ? argc : 1;
  
  for (size_t i = 0; i < nsrc; i++) {
    char *name, buf[BUFSIZ];
    FILE *fp = NULL;
    size_t n = 0, col = 0, ws = 0, strt = 0;
    size_t r = 0;
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
    while ((n = fread(buf + r, 1, BUFSIZ - r, fp)) > 0) {
      r += n;
      for (size_t j = 0; j < r; j++) {
        switch (buf[j]) {
          case '\n':
            fwrite(buf + strt, 1, j - strt, shout);
            fputc('\n', shout);
            strt = j + 1;
            col = ws = 0;
            break;
          case '\t':
            if (flags & byt)
              col++;
            else
              col = ((col / 8) + 1) * 8;
            ws = j;
            break;
          case '\b':
            if (flags & byt)
              col++;
            else if (col)
              col--;
            break;
          case ' ':
            ws = j;
            col++;
            break;
          default:
            col++;
            break;
        }
        if (col > w) {
          size_t brk;
          if ((flags & spc) && ws >= strt) {
            brk = ws;
            fwrite(buf + strt, 1, brk + 1 - strt, shout);
            fputc('\n', shout);
            strt = brk + 1;
            j = brk;
          } else {
            brk = j;
            if (brk > strt)
              fwrite(buf + strt, 1, brk - strt, shout);
            fputc('\n', shout);
            strt = brk;
            j = brk - 1;
          }
          col = ws = 0;
        }
      }
      if (strt < r) {
        memmove(buf, buf + strt, r - strt);
        ws = (ws >= strt) ? (ws - strt) : 0;
        r -= strt;
      } else {
        r = 0;
      }
      strt = 0;
    }
    if (r)
      fwrite(buf, 1, r, shout);
    if (fp && fp != shin)
      fclose(fp);
  }
  return status;
}
#endif /* if ENABLE_FOLD */
