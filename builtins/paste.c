#include "config.h"
#define _POSIX_C_SOURCE 200809L
#if ENABLE_PASTE

#include "arg.h"
#include "errmsg.h"
#include "lineio.h"
#include "utils.h"

#define MAX_DELIM 256

static int pastedelim(const char *, char *, size_t *);

static inline void
putpdelim(const char *d, size_t nd, size_t *di)
{
  char ch;
  if (!nd)
    return;

  ch = d[(*di)++ % nd];
  if (ch != 0x00)
    fputc(ch, shout);
}

int
pastecmd(char *argv[])
{
  size_t nf, nd = 0, argc = 0;
  size_t *slens, di;
  char delims[MAX_DELIM], **slots;
  int sfl = 0;
  lr_t *lr;
  stmark pm;

  delims[nd++] = '\t';
  array_len(argv, argc);
  ARGBEGIN
  {
    case 'd':
      {
        char *arg;
        int r;
        if (!(arg = EARGF(usage(argv0, helpmsgs[PASTEH].usage))))
          return 1;
        if ((r = pastedelim(arg, delims, &nd)) < 0)
          return shwarn_arg(argv0, arg, "delimiter list ends with unescaped backslash");
        else if (r == 1)
          return shwarn_arg(argv0, arg, "delimiter too long");
        break;
      }
    case 's':
      sfl = 1;
      break;
    default:
      return bad_opt(argv0, ARGC());
  }
  ARGEND

  nf = argc ? argc : 1;
  lr = st_alloc(nf * sizeof(lr_t));
  slens = st_alloc(nf * sizeof(size_t));
  slots = st_alloc(nf * sizeof(char *));
  FILE *fp[nf];

  for (size_t i = 0; i < nf; i++) {
    char *name;
    name = argc ? argv[i] : NULL;
    if (name && *name == '-' && name[1] == '\0')
      fp[i] = lropen(&lr[i], NULL);
    else if (!(fp[i] = lropen(&lr[i], name)))
      return sherr(1, argv0, name ? name : "(stdin)");
  }

  pm = stack_mark();
  if (!sfl) {
    for (;;) {
      int any = 0;
      for (size_t i = 0; i < nf; i++) {
        char *line;
        size_t len;
        if (!fp[i])
          continue;
        if (!(line = lrread(&lr[i], &len))) {
          if (fp[i] != shin)
            fclose(fp[i]);
          fp[i] = NULL;
        } else {
          slots[i] = line;
          slens[i] = len;
          any = 1;
        }
      }
      if (!any)
        break;
      di = 0;
      for (size_t i = 0; i < nf; i++) {
        if (i > 0)
          putpdelim(delims, nd, &di);
        if (fp[i])
          fwrite(slots[i], 1, slens[i], shout);
      }
      fputc('\n', shout);
      stack_restore(pm);
    }
  } else {
    for (size_t i = 0; i < nf; i++) {
      di = 0;
      for (int lnc = 0;; lnc++) {
        char *line;
        size_t len;
        if ((line = lrread(&lr[i], &len))) {
          if (lnc > 0)
            putpdelim(delims, nd, &di);
          fwrite(line, 1, len, shout);
        } else {
          if (fp[i] != shin)
            fclose(fp[i]);
          fp[i] = NULL;
          break;
        }
        stack_restore(pm);
      }
      fputc('\n', shout);
    }
  }
  return 0;
}

static int
pastedelim(const char *s, char *buf, size_t *n)
{
  size_t len = 0;

  for (int i = 0; s[i]; i++) {
    char c = s[i];
    if (len >= MAX_DELIM)
      return 1;

    if (c == '\\') {
      if (s[++i] == '\0') {
        return -1;
      } else if (s[i] == 'n') {
        buf[len++] = '\n';
        continue;
      } else if (s[i] == 't') {
        buf[len++] = '\t';
        continue;
      } else if (s[i] == '\\') {
        buf[len++] = '\\';
        continue;
      } else if (s[i] == '0') {
        buf[len++] = 0x00;
        continue;
      } else {
        buf[len++] = s[i];
        continue;
      }
    }
    buf[len++] = c;
  }
  *n = len;
  return 0;
}

#endif /* ENABLE_PASTE */
