#include "config.h"
#if ENABLE_COMM
#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */

#include "arg.h"
#include "errmsg.h"
#include "lineio.h"
#include "utils.h"

static int show;

static int lcmp(const char *a, size_t alen, const char *b, size_t blen);
static void printline(int col, const char *line, size_t len);
static void drain(lr_t *lr, int col, char **line, size_t *len);

int
commcmd(char *argv[])
{
  size_t argc = 0, len1 = 0, len2 = 0;
  lr_t lr1, lr2;
  FILE *fp1 = NULL, *fp2 = NULL;
  int status = 0;

  show = 0x07;
  array_len(argv, argc);
  ARGBEGIN
  {
    case '1':
      show &= ~(1 << 0);
      break;
    case '2':
      show &= ~(1 << 1);
      break;
    case '3':
      show &= ~(1 << 2);
      break;
    default:
      return bad_opt(argv0, ARGC());
  }
  ARGEND

  if (argc != 2)
    return usage(argv0, helpmsgs[COMMH].usage), 1;
  if (*argv) {
    if ((*argv)[0] == '-' && (*argv)[1] == '\0') {
      fp1 = lropen(&lr1, NULL);
    } else {
      if (!(fp1 = lropen(&lr1, *argv)))
        return sherr(1, argv0, *argv);
    }
  }
  argv++;
  if (*argv) {
    if ((*argv)[0] == '-' && (*argv)[1] == '\0') {
      if (fp1 == shin)
        return shwarn_arg(argv0, *argv, "both files can't be -");
      fp2 = lropen(&lr2, NULL);
    } else {
      if (!(fp2 = lropen(&lr2, *argv)))
        return sherr(1, argv0, *argv);
    }
  }

  char *line1 = NULL, *line2 = NULL;

  for (;;) {
    int cmp;
    if (!line1)
      line1 = lrread(&lr1, &len1);
    if (!line2)
      line2 = lrread(&lr2, &len2);

    if (!line1) {
      drain(&lr2, 1, &line2, &len2);
      break;
    }
    if (!line2) {
      drain(&lr1, 0, &line1, &len1);
      break;
    }
    cmp = lcmp(line1, len1, line2, len2);
    if (!cmp) {
      printline(2, line1, len1);
      line1 = line2 = NULL;
    } else if (cmp < 0) {
      printline(0, line1, len1);
      line1 = NULL;
    } else {
      printline(1, line2, len2);
      line2 = NULL;
    }
  }
  if (fp1 != shin)
    fclose(fp1);
  if (fp2 != shin)
    fclose(fp2);

  return status;
}

static int
lcmp(const char *a, size_t alen, const char *b, size_t blen)
{
  size_t n;
  int r;

  n = alen < blen ? alen : blen;
  r = memcmp(a, b, n);
  if (r)
    return r;
  return (alen > blen) - (alen < blen);
}

static void
printline(int col, const char *line, size_t len)
{
  int i;

  if (!(show & (1 << col)))
    return;
  for (i = 0; i < col; i++)
    if (show & (1 << i))
      fputc('\t', shout);
  fwrite(line, 1, len, shout);
  fputc('\n', shout);
}

static void
drain(lr_t *lr, int col, char **line, size_t *len)
{
  while (*line) {
    printline(col, *line, *len);
    *line = lrread(lr, len);
  }
}


#endif /* if ENABLE_COMM */




















