#include "config.h"
#if ENABLE_WC
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>

#include "arg.h"
#include "errmsg.h"
#include "simd.h"
#include "utils.h"

static inline size_t
cntnl(const char *buf, size_t len)
{
  size_t n = 0;
  for (size_t i = 0; i < len; i++)
    if (buf[i] == '\n')
      n++;
  return n;
}

static inline size_t
cntwords(const char *buf, size_t len, int *inwrd)
{
  size_t n = 0;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)buf[i];
    if (c == ' ' || c == '\t' || c == '\n')
      *inwrd = 0;
    else if (!*inwrd) {
      n++;
      *inwrd = 1;
    }
  }
  return n;
}

#ifdef __SSE2__
static inline size_t
scntnl(const char *buf, size_t len)
{
  sint input, nlv, nlr;
  int mask;
  size_t i, n = 0;

  nlv = _mm_set1_epi8('\n');
  for (i = 0; i + 16 <= len; i += 16) {
    input = _mm_loadu_si128((const sint *)(buf + i));
    nlr = _mm_cmpeq_epi8(input, nlv);
    mask = _mm_movemask_epi8(nlr);
    n += __builtin_popcount((unsigned)mask);
  }
  if (i < len) {
    char tmp[16] __attribute__((aligned(16)));
    size_t rem = len - i;
    memcpy(tmp, buf + i, rem);
    input = _mm_load_si128((const sint *)tmp);
    nlr = _mm_cmpeq_epi8(input, nlv);
    mask = _mm_movemask_epi8(nlr) & ((1 << rem) - 1);
    n += __builtin_popcount((unsigned)mask);
  }
  return n;
}

static inline size_t
scntwords(const char *buf, size_t len, int *inwrd)
{
  sint input, spv, tabv, nlv, wsm;
  int mask, non, starts;
  size_t i, n = 0;

  spv = _mm_set1_epi8(' ');
  tabv = _mm_set1_epi8('\t');
  nlv = _mm_set1_epi8('\n');
  for (i = 0; i + 16 <= len; i += 16) {
    input = _mm_loadu_si128((const sint *)(buf + i));
    wsm = _mm_or_si128(_mm_cmpeq_epi8(input, spv),
                       _mm_or_si128(_mm_cmpeq_epi8(input, tabv),
                                    _mm_cmpeq_epi8(input, nlv)));
    mask = _mm_movemask_epi8(wsm);
    non = ~mask & 0xFFFF;
    starts = non & ~(non << 1);
    if (*inwrd)
      starts &= ~1;
    *inwrd = (non >> 15) & 1;
    n += __builtin_popcount((unsigned)starts);
  }
  if (i < len) {
    char tmp[16] __attribute__((aligned(16)));
    size_t rem = len - i;
    memcpy(tmp, buf + i, rem);
    input = _mm_load_si128((const sint *)tmp);
    wsm = _mm_or_si128(_mm_cmpeq_epi8(input, spv),
                       _mm_or_si128(_mm_cmpeq_epi8(input, tabv),
                                    _mm_cmpeq_epi8(input, nlv)));
    mask = _mm_movemask_epi8(wsm) & ((1 << rem) - 1);
    non = ~mask & ((1 << rem) - 1);
    starts = non & ~(non << 1);
    if (*inwrd)
      starts &= ~1;
    *inwrd = (non >> 15) & 1;
    n += __builtin_popcount((unsigned)starts);
  }
  return n;
}
#else
#define scntnl(buf, len) (cntnl((buf), (len)))
#define scntwords(buf, len, inwrd) (cntwords((buf), (len), (inwrd)))
#endif /* __SSE2__ */

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
      nbyt += (int)n;
      nln += (int)scntnl(buf, n);
      nwrd += (int)scntwords(buf, n, &inwrd);
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
