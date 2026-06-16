#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "alloc.h"
#include "main.h"
#include "opts.h"
#include "utils.h"
#include "var.h"

int nounseterr = 0;

char shopts[OPTC];
const char *shoptname[OPTC] = {
  "allexport",
  "notify",
  "noclobber",
  "errexit",
  "noglob",
  "hashall",
  "interactive",
  "ignoreeof",
  "monitor",
  "noexec",
  "stdin",
  "nounset",
  "verbose",
  "vi",
  "xtrace",
  "emacs",
  "nolog",
  "pipefail",
  "debug",
};

const char shoptch[OPTC] = {
  'a',
  'b',
  'C',
  'e',
  'f',
  'h',
  'i',
  'I',
  'm',
  'n',
  's',
  'u',
  'v',
  'V',
  'x',
};

static int setopts(char *, int, char *);
static int cmpname(const void *, const void *);

static int
setopts(char *arg, int n, char *argv0)
{
  size_t i;
  i = 0;

  while (arg[i]) {
    switch (arg[i]) {
      case 'a':
        aflag = n;
        break;
      case 'b':
        bflag = n;
        break;
      case 'C':
        Cflag = n;
        break;
      case 'e':
        eflag = n;
        break;
      case 'f':
        fflag = n;
        break;
      case 'h':
        hflag = n;
        break;
      case 'i':
        iflag = n;
        break;
      case 'I':
        Iflag = n;
        break;
      case 'm':
        mflag = n;
        break;
      case 'n':
        nflag = n;
        break;
      case 's':
        sflag = n;
        break;
      case 'u':
        uflag = n;
        break;
      case 'v':
        vflag = n;
        break;
      case 'V':
        Vflag = n;
        break;
      case 'x':
        xflag = n;
        break;
      default:
        bad_opt(sh_argv0, argv0, arg[i]);
        return -1;
    }
    i++;
  }
  return 0;
}

int
chkopt(char *argv)
{
  for (size_t i = 0; i < OPTC; i++) {
    if (strcasecmp(argv, shoptname[i]) == 0)
      return i;
  }
  return -1;
}

void
listopts(int m)
{
  const char *onoff[] = { "off", "on" };
  if (m) {
    puts("Current Shell Option Settings");
    for (size_t i = 0; i < OPTC; i++) {
      printf("%-12s\t\t\t\t%s\n",shoptname[i], onoff[(int)shopts[i]]);
    }
  } else {
    for (size_t i = 0; i < OPTC; i++) {
      char s;
      s = (shopts[i]) ? '-' : '+';
      printf("set %co %s\n", s, shoptname[i]);
    }
  }
}

void
init_opts(void)
{
  for (int i = 0; i < OPTC; i++)
    shopts[i] = 0;

  iflag = isatty(STDIN_FILENO);
  mflag = iflag;
  hflag = iflag;
}

void
freeshargv(void)
{
  if (alloc_sh_argv) {
    for (int i = 0; i < sh_argc; i++)
      free(sh_argv[i]);
    free(sh_argv);
  }
  sh_argv = NULL;
  sh_argc = 0;
  alloc_sh_argv = 0;
}

static int
cmpname(const void *va, const void *vb)
{
  const char *a, *b;
  a = *(const char **)va;
  b = *(const char **)vb;
  while (*a && *a != '=' && *b && *b != '=') {
    if (*a != *b)
      return (unsigned char)*a - (unsigned char)*b;
    ++a;
    ++b;
  }
  if (*a == '\0' || *a == '=') {
    if (*b == '\0' || *b == '=')
      return 0;
    return -1;
  }
  return 1;
}

int
setcmd(char **argv)
{
  size_t argc = 0;
  array_len(argv, argc);

  if (argc < 2) {
    char **enva;
    shvar *v;
    size_t c, f;

    c = 0;
    for (size_t i = 0; i < var_tab_size; i++) {
      v = &var_tab[i];
      if (!v->var || v->var == TOMBSTONE)
        continue;
      c++;
    }
    if (c == 0)
      return 0;
    enva = st_alloc((c + 1) * sizeof(char *));
    f = 0;
    for (size_t i = 0; i < var_tab_size; i++) {
      v = &var_tab[i];
      if (!v->var || v->var == TOMBSTONE)
        continue;
      enva[f++] = v->var;
    }
    enva[c] = NULL;
    qsort(enva, c, sizeof(char *), cmpname);

    for (size_t i = 0; i < c; i++) {
      char *n, *v;
      st_read_assn(enva[i], &n, &v);
      printf("%s=%s\n", n, quotestrn(v));
    }
    return 0;
  }
  char *argv0, **pos;
  int minus, pparams, pcnt;

  argv0 = argv[0];
  pos = NULL;
  pcnt = 0;
  pparams = 0;

  for (size_t i = 1; i < argc; i++) {
    char *arg;
    arg = argv[i];

    if (pparams) {
      if (!pos)
        pos = malloc(argc * sizeof(char *));
      pos[pcnt++] = strdup_(arg);
      continue;
    }

    if (arg[0] != '-' && arg[0] != '+') {
      pparams = 1;
      if (!pos)
        pos = malloc(argc * sizeof(char *));
      pos[pcnt++] = strdup_(arg);
      continue;
    }

    minus = (arg[0] == '-');

    if (!arg[1]) {
      pparams = 1;
      continue;
    } else if (arg[1] == '-' && !arg[2]) {
      pparams = 1;
      continue;
    } else if (arg[1] == 'o') {
      int idx;
      char *o;
      if (arg[2])
        o = arg + 2;
      else if (++i < argc)
        o = argv[i];
      else {
        listopts(minus);
        continue;
      }
      idx = chkopt(o);
      if (idx < 0)
        return 1;
      shopts[idx] = minus;
      continue;
    }

    if (setopts(arg + 1, minus, argv0) < 0)
      goto err;
  }

  if (pparams) {
    if (!pos)
      pos = malloc(argc * sizeof(char *));
    pos[pcnt] = NULL;
    freeshargv();
    alloc_sh_argv = 1;
    sh_argv = pos;
    sh_argc = pcnt;
  } else {
    if (pos)
      free(pos);
  }
  return 0;

err:
  if (pos) {
    for (int j = 0; j < pcnt; j++)
      free(pos[j]);
    free(pos);
  }
  return 1;
}

