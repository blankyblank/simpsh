#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "alloc.h"
#include "errmsg.h"
#include "input.h"
#include "main.h"
#include "opts.h"
#include "utils.h"
#include "var.h"

static const char *shoptname[OPTC] = {
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
  'E',
};

static int setopts(char *, int, char *);

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
        bad_opt(argv0, arg[i]);
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
      printf("%-12s\t\t\t\t%s\n",shoptname[i], onoff[(int)SHOPTS[i]]);
    }
  } else {
    for (size_t i = 0; i < OPTC; i++) {
      char s;
      s = (SHOPTS[i]) ? '-' : '+';
      printf("set %co %s\n", s, shoptname[i]);
    }
  }
}

void
init_opts(void)
{
  for (int i = 0; i < OPTC; i++)
    SHOPTS[i] = 0;
  hflag = 1;
}

void
freeshargv(void)
{
  if (ALLOCED) {
    for (int i = 0; i < SHARGC; i++)
      slfree(SHARGV[i]);
    slfree(SHARGV);
  }
  SHARGV = NULL;
  SHARGC = 0;
  ALLOCED = 0;
}

int
setcmd(char **argv)
{
  size_t argc = 0;
  array_len(argv, argc);

  if (argc < 2) {
    printvars("", 0);
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
        pos = salloc(argc * sizeof(char *));
      pos[pcnt++] = strdup_(arg);
      continue;
    }

    if (arg[0] != '-' && arg[0] != '+') {
      pparams = 1;
      if (!pos)
        pos = salloc(argc * sizeof(char *));
      pos[pcnt++] = strdup_(arg);
      continue;
    }

    minus = (arg[0] == '-');

    if (!arg[1] || (arg[1] == '-' && !arg[2])) {
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
      SHOPTS[idx] = minus;
      continue;
    }

    if (setopts(arg + 1, minus, argv0) < 0)
      goto err;
  }

  if (pparams) {
    if (!pos)
      pos = salloc(argc * sizeof(char *));
    pos[pcnt] = NULL;
    freeshargv();
    ALLOCED = 1;
    SHARGV = pos;
    SHARGC = pcnt;
  } else {
    if (pos)
      slfree(pos);
  }
  return 0;

err:
  if (pos) {
    for (int j = 0; j < pcnt; j++)
      slfree(pos[j]);
    slfree(pos);
  }
  return 1;
}

static int
checkopts(char *optstr, char **argv, char *o, int opterr)
{
  int narg, has = 0, quiet = 0;
  char *s, *p, c;

  if (!(p = *(argv + OPTIND - 1)))
    return 1;
  if (OPTOFF < 0) {
    if (*p == '-') {
      if (*(p + 1) == '-' && *(p + 2) == '\0') {
        OPTIND++;
        return 1;
      }
      if (*(p + 1) == '\0')
        return 1;
      OPTIND++, OPTOFF = 1;
    } else {
      return 1;
    }
  } else {
    p = *(argv + OPTIND - 2);
  }

  s = *(argv + OPTIND - 1);
  c = *(p + OPTOFF++);
  quiet = (*optstr == ':');
  for (char *op = optstr + quiet; *op; op++) {
    if (*op == ':')
      continue;
    if (*op == c) {
      narg = (*(op + 1) == ':');
      has = 1;
      break;
    }
  }

  if (!has) {
    if (quiet) {
      *o = '?';
      setvar(oargn, (char[2]) { c, '\0' }, 0);
    } else {
      *o = '?';
      rmvar(oargn);
      if (opterr)
        fprintf(stderr, "%s: illegal option -- %c\n", shinpt->name, c);
    }
    if (!*(p + OPTOFF))
      OPTOFF = -1;
    *o = '?';
    return 0;
  }

  if (narg) {
    if (*(p + OPTOFF)) {
      setvar(oargn, p + OPTOFF, 0);
      OPTOFF = -1;
    } else if (s) {
      setvar(oargn, s, 0);
      OPTIND++, OPTOFF = -1;
    } else {
      if (!*(p + OPTOFF)) {
        OPTOFF = -1;
        if (quiet) {
          *o = ':';
          setvar(oargn, (char[2]) { c, '\0' }, 0);
        } else {
          *o = '?';
          rmvar(oargn);
          if (opterr)
            fprintf(stderr, "%s: option requires argument -- %c\n", shinpt->name, c);
        }
        return 0;
      }
    }
    *o = c;
    return 0;
  }
  setvar(STR("OPTARG"), "", 0);
  if (!*(p + OPTOFF))
    OPTOFF = -1;
  *o = c;
  return 0;
}

int
getoptscmd(char **argv)
{
  int opterr = 1, argc = 0, argpc;
  char *oerr, *argv0 = *argv;
  char **argp;

  array_len(argv, argc);
  switch (argc) {
    case 1:
    case 2:
      usage(argv0, helpmsgs[GETOPTSH].usage);
      return 1;
    case 3:
      if (!SHARGC || !*SHARGV) {
        setvar(argv[2], "?", 0);
        return 1;
      }
      argp = SHARGV, argpc = SHARGC;
      break;
    default:
      argp = argv + 3, argpc = argc - 3;
      break;
  }

  char res = '\0', buf[24], nb[2];
  int status;

  if (OPTIND < 1 || OPTIND > argpc + 1)
    OPTIND = 1, OPTOFF = -1;

  if ((oerr = getvar(oerrn)))
    opterr = (!(oerr[0] == '0' && oerr[1] == '\0'));

  status = checkopts(argv[1], argp, &res, opterr);
  itoa(OPTIND, buf);
  setvar(oinn, buf, VNOCB);
  nb[0] = (status) ? '?' : res;
  nb[1] = '\0';
  setvar(argv[2], nb, 0);

  return status;
}
