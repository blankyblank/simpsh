#include "config.h"
#if ENABLE_TAIL
#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */
#define _DEFAULT_SOURCE

#include <sys/stat.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "alloc.h"
#include "arg.h"
#include "builtins.h"
#include "errmsg.h"
#include "utils.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

struct tinfo {
  char *name;
  FILE *fp;
  long pos;
  dev_t dev;
  ino_t ino;
};

const char *tailn = "tail";
static int pls;
static int quiet;

FILE *fline(int n, char *f);
int ftail(int, char **, size_t);
int sttail(int);

static inline void
shsleep(unsigned ms)
{
  struct timespec ts = { .tv_sec = ms / 1000,
                         .tv_nsec = (ms % 1000) * 1000000 };
  nanosleep(&ts, NULL);
}

int
tailcmd(char *argv[])
{
  int ln, f_flag;
  size_t argc;
  char *file;

  ln = -1;
  f_flag = pls = argc = quiet = 0;
  array_len(argv, argc);
  ARGBEGIN
  {
    char *arg;
    case 'f':
    case 'F':
      f_flag = 1;
      break;
    case 'n':
      if (!(arg = EARGF(usage(argv0, helpmsgs[TAILH].usage))))
        return 1;
      if (*arg == '+')
        pls = 1, arg++;
      ln = bltin_atoi(arg, argv0, "requires a number");
      break;
    case 'q':
      quiet = 1;
      break;
ARGNUM:
      ln = ARGNUMF();
      break;
    default:
      return bad_opt(argv0, ARGC());
  }
  ARGEND

  if (argv[0] && **argv == '+' && (*argv)[1] &&  isdigit_((*argv)[1])) {
    pls = 1;
    ln = bltin_atoi((*argv) + 1, argv0, "requires a number");
    argv++, argc--;
  }

  if (ln < 0)
    ln = 10;

  if (!argc)
    return sttail(ln);

  if (f_flag)
    return ftail(ln, argv, argc);

  char buf[BUFSIZ];
  for (size_t i = 0; argv[i]; i++) {
    FILE *fp;
    int n = 0;
    file = argv[i];
    if (!(fp = fline(ln, file)))
      return 1;
    if (argc > 1)
      if (!quiet)
        fprintf(shout,"%s==> %s <==\n", (i > 0) ? "\n" : "", file);
    while ((n = fread(buf, 1, BUFSIZ, fp)) > 0)
      fwrite(buf, 1, n, shout);
    if (ferror(fp))
      return sherr(1, tailn, "Bad file descriptor");
    fclose(fp);
  }
  return 0;
}

FILE *
fline(int ln, char *file)
{
  FILE *fp;
  char buf[BUFSIZ];
  long pos;
  int start = 0, rem, tnl = 0;

  if ((fp = fopen(file, "r")) == NULL) {
    shwarn("tail: failed to open", file);
    return NULL;
  }

  fseek(fp, 0, SEEK_END);
  pos = ftell(fp);

  if (pls) {
    int need, seen;
    long fpos, lp;
    size_t got;

    need = (ln > 1) ? ln -1 : 0;
    seen = fpos = lp = 0;

    fseek(fp, 0, SEEK_SET);
    while (seen < need && fpos < pos) {
      size_t chunk;
      chunk = min(BUFSIZ, (size_t)(pos - fpos));
      fseek(fp, fpos, SEEK_SET);
      got = fread(buf, 1, chunk, fp);
      for (size_t i = 0; i < got && seen < need; i++)
        if (buf[i] == '\n')
          seen++, lp = fpos + i + 1;
      fpos += got;
      if (!got)
        break;
    }
    fseek(fp, (seen < need) ? pos : lp, SEEK_SET);
    return fp;
  }
  if (pos > 0) {
    fseek(fp, -1, SEEK_END);
    if (fgetc(fp) == '\n')
      tnl = 1;
    fseek(fp, 0, SEEK_END);
  }
  rem = ln + tnl;
  while (rem > 0 && pos > 0) {
    int read;
    size_t chunk;
    chunk = min(BUFSIZ, pos);
    pos -= chunk;
    fseek(fp, pos, SEEK_SET);
    read = fread(buf, 1, chunk, fp);
    for (char *fpos = buf + read - 1; fpos >= buf; fpos--) {
      if (*fpos == '\n') {
        rem--;
        if (!rem) {
          start = pos + (fpos - buf + 1);
          break;
        }
      }
    }
  }
  fseek(fp, start, SEEK_SET);
  return fp;
}

int
ftail(int ln, char **files, size_t argc)
{
  if (!argc)
    return 1;

  char buf[BUFSIZ];
  struct tinfo fe[argc];


  for (size_t i = 0; i < argc; i++) {
    struct stat st;
    fe[i].name = files[i];
    if (!(fe[i].fp = fline(ln, files[i])))
      return 1;
    fe[i].pos = ftell(fe[i].fp);
    fstat(fileno(fe[i].fp), &st);
    fe[i].dev = st.st_dev;
    fe[i].ino = st.st_ino;
  }

  while (1) {
    for (size_t i = 0; i < argc; i++) {
      struct stat st;
      int n = 0;
      FILE *fp = fe[i].fp;

      if (fp && fstat(fileno(fe[i].fp), &st) < 0) {
        fclose(fp);
        fp = fe[i].fp = NULL;
      } else if (st.st_ino != fe[i].ino || st.st_dev != fe[i].dev) {
        fclose(fp);
        fp = fe[i].fp = fopen(fe[i].name, "r");
        fe[i].pos = 0;
        fstat(fileno(fe[i].fp), &st);
        fe[i].ino = st.st_ino;
        fe[i].dev = st.st_dev;
      } else {
        fseek(fp, fe[i].pos, SEEK_SET);
      }

      if (fp) {
        while ((n = fread(buf, 1, BUFSIZ, fp)) > 0) {
          if (argc > 1)
            if (!quiet)
              fprintf(shout, "\n==> %s <==\n", fe[i].name);
          fwrite(buf, 1, n, shout);
        }
        fe[i].pos = ftell(fp);
        fflush_unlocked(shout);
        if (ferror(fp))
          return 1;
      }
    }
    shsleep(100);
  }

  for (size_t i = 0; i < argc; i++)
    fclose(fe[i].fp);
  return 0;
}

static int
sttail_beg(int ln)
{
  int need, c;
  char buf[BUFSIZ];
  size_t n;

  n = c = 0;
  need = (ln > 1) ? ln - 1 : 0;

  while (need > 0) {
    c = fgetc(shin);
    if (c == EOF)
      return 0;
    if (c == '\n')
      need--;
  }
  while ((n = fread(buf, 1, sizeof(buf), shin)) > 0)
    fwrite(buf, 1, n, shout);
  if (ferror(shin))
    return sherr(1, tailn, "Bad file descriptor");
  return 0;
}

int
sttail(int ln)
{
  int s, c, cnt, n;
  size_t off;
  char *rng[ln], buf[BUFSIZ];

  off = c = cnt = n = 0;
  if (pls)
    return sttail_beg(ln);
  for (int in = 0; in < ln; ++in)
    rng[in] = NULL;
  while ((n = fread(buf + off, 1, sizeof(buf) - off, shin)) > 0) {
    char *p = buf;
    char *end = buf + off + n;

    while (p < end) {
      char *nl = memchr(p, '\n', pntlen(p, end));
      if (!nl) {
        off = pntlen(p, end);;
        memmove(buf, p, off);
        break;
      }
      rng[c] = st_strndup(p, pntlen(p, nl) + 1);
      c = (c + 1) % ln;
      cnt++;
      p = nl + 1;
    }
  }
  if (ferror(shin))
    return sherr(1, tailn, "Bad file descriptor");
  if (off) {
    rng[c] = st_strndup(buf, off);
    cnt++;
  }

  s = (cnt < ln) ? 0 : c;
  for (int i = 0; i < ((cnt < ln) ? cnt : ln); i++) {
    fwrite(rng[(s + i) % ln], 1, strlen(rng[(s + i) % ln]), shout);
  }

  return 0;
}
#endif /* ENABLE_TAIL */
