#include "config.h"
#if ENABLE_EXPAND
#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */

#include <stdio.h>

#include "arg.h"
#include "errmsg.h"
#include "lineio.h"
#include "utils.h"

static size_t tabcol(size_t col, size_t *tbs, size_t ntbs);
static size_t *parsetb(char *, size_t *);

#define flushspace() \
  if (run) { \
    size_t pos = strt; \
    while (col - pos > 0) { \
      nxt = tabcol(pos, t, ntb); \
      if (nxt - pos > col - pos) \
        break; \
      fputc('\t', shout); \
      pos = nxt; \
    } \
    while (pos < col) { \
      fputc(' ', shout); \
      pos++; \
    } \
    run = 0; \
  }

int
expandcmd(char *argv[])
{
  size_t nsrc, argc = 0, ntb = 1;
  size_t *t = &((size_t) { 8 });
  int init = 0, status = 0;
  stmark em;

  array_len(argv, argc);
  ARGBEGIN
  {
    case 'i':
      init = 1;
      break;
    case 't':
      t = parsetb(EARGF(usage(argv0, helpmsgs[EXPANDH].usage)), &ntb);
      if (!ntb)
        return 1;
      break;
    default:
      return bad_opt(argv0, ARGC());
  }
  ARGEND

  nsrc = argc ? argc : 1;

  for (size_t i = 0; i < nsrc; i++) {
    lr_t lr;
    size_t llen = 0;
    FILE *fp = NULL;
    char *path, *line;

    path = (argc) ? argv[i] : NULL;
    if (!(fp = lropen(&lr, path))) {
      status = sherr(1, argv0, path ? path : "(stdin)");
      continue;
    }

    em = stack_mark();
    while ((line = lrread(&lr, &llen))) {
      size_t col = 0, lead = 1;
      for (size_t j = 0; j < llen; j++) {
        size_t nxt = 0;
        switch (line[j]) {
          case '\t':
            if (init && !lead)
              fputc('\t', shout);
            else {
              nxt = tabcol(col, t, ntb);
              while (col < nxt) {
                fputc(' ', shout);
                col++;
              }
            }
            break;
          case ' ':
            fputc(line[j], shout);
            col++;
            break;
          case '\b':
            if (col > 0)
              col--;
            fputc(line[j], shout);
            lead = 0;
            break;
          default:
            fputc(line[j], shout);
            col++;
            lead = 0;
            break;
        }
      }
      fputc('\n', shout);
      stack_restore(em);
    }
    if (path)
      fclose(fp);
  }
  return status;
}

int
unexpandcmd(char *argv[])
{
  size_t nsrc, argc = 0, ntb = 1;
  size_t *t = &((size_t) { 8 });
  int all = 0, status = 0;
  stmark uxm;

  array_len(argv, argc);
  ARGBEGIN
  {
    case 'a':
      all = 1;
      break;
    case 't':
      t = parsetb(EARGF(usage(argv0, helpmsgs[UNEXPANDH].usage)), &ntb);
      if (!ntb)
        return 1;
      break;
    default:
      return bad_opt(argv0, ARGC());
  }
  ARGEND

  nsrc = argc ? argc : 1;

  for (size_t i = 0; i < nsrc; i++) {
    lr_t lr;
    size_t llen = 0;
    FILE *fp = NULL;
    char *path, *line;

    path = (argc) ? argv[i] : NULL;
    if (!(fp = lropen(&lr, path))) {
      status = sherr(1, argv0, path);
      continue;
    }

    uxm = stack_mark();
    while ((line = lrread(&lr, &llen))) {
      size_t col = 0, lead = 1;
      size_t nxt = 0, run = 0, strt = 0;
      for (size_t j = 0; j < llen; j++) {
        switch (line[j]) {
          case '\t':
            flushspace();
            fputc('\t', shout);
            col = tabcol(col, t, ntb);
            break;
          case ' ':
            if (all || lead) {
              if (!run) {
                strt = col;
                run = 1;
              }
              col++;
            } else {
              fputc(' ', shout);
              col++;
            }
            break;
          case '\b':
            flushspace();
            if (col > 0)
              col--;
            fputc(line[j], shout);
            lead = 0;
            break;
          default:
            flushspace();
            fputc(line[j], shout);
            col++;
            lead = 0;
            break;
        }
      }
      flushspace();
      fputc('\n', shout);
      stack_restore(uxm);
    }
    if (path)
      fclose(fp);
  }
  return status;
}

static size_t *
parsetb(char *s, size_t *idx)
{
  int ccnt = 0;
  size_t *t, lidx = 0;
  for (size_t i = 0; s[i]; i++)
    if (s[i] == ',')
      ccnt++;

  if (!ccnt) {
    int n;
    t = st_alloc(1 * sizeof(size_t));
    if (!(n = atoi_(s)) || (n < 0)) {
      *idx = 0;
      return NULL;
    }
    *idx = 1;
    t[0] = n;
    return t;
  }
  ccnt++;

  char *strt, *cur;
  t = st_alloc(ccnt * sizeof(size_t));
  strt = s;
  for (size_t i = 0; ; i++) {
    int n;
    cur = s + i;
    if (*cur == ',') {
      char sv = *cur;
      *cur = '\0';
      if (!(n = atoi_(strt)) || (n < 0)) {
        *idx = 0;
        return NULL;
      }
      t[lidx] = (size_t)n;
      lidx++;
      *cur = sv;
      strt = s + i + 1;
    }
    if (*cur == '\0') {
      if (!(n = atoi_(strt)) || (n < 0)) {
        *idx =  0;
        return NULL;
      }
      t[lidx] = (size_t)n;
      lidx++;
      break;
    }
  }
  for (size_t i = 0; i < lidx; i++) {
    size_t tmp, j;
    tmp = t[i];
    for (j = i; j > 0 && t[j - 1] > tmp; j--)
      t[j] = t[j - 1];
    t[j] = tmp;
  }
  *idx = lidx;
  return t;
}

static size_t
tabcol(size_t col, size_t *tbs, size_t ntbs)
{
  if (ntbs == 1) {
    return (col / tbs[0] + 1) * tbs[0];
  }
  for (size_t i = 0; i < ntbs; i++) {
    if (tbs[i] > col)
      return tbs[i];
  }
  return col + 1;
}
#endif /* ENABLE_EXPAND */
