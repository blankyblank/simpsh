/* simpsh - a simple posix shell */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include "alloc.h"
#include "arg.h"
#include "builtins.h"
#include "errmsg.h"
#include "input.h"
#include "job.h"
#include "main.h"
#include "opts.h"
#include "sig.h"
#include "simpsh.h"
#include "var.h"

const char shusg[43] = "[-abCefhiImnosvVx] [-o longopt] [-c 'cmd']";
const char defpathn[80] = "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
GSTATE gstate = { .shparm.optind = 1, .shparm.optoff = -1 };
FILE *shin;
FILE *shout;

int builtin_tab[BUILTIN_BUCKETS];
 /* __attribute__((visibility("default"))) */
/** shell entry point */
int
main(int argc, char **argv)
{
  (void)argc;
  int flags, fd, i;
  char *oarg;

  init_opts();
  flags = 0;
  ARGBEGIN
  {
    case 'a':
      aflag = 1;
      break;
    case 'b':
      bflag = 1;
      break;
    case 'c':
      flags |= FLAG_c;
      break;
    case 'C':
      Cflag = 1;
      break;
    case 'e':
      eflag = 1;
      break;
    case 'f':
      fflag = 1;
      break;
    case 'h':
      hflag = 1;
      break;
    case 'i':
      iflag = 1;
      flags |= FLAG_i;
      break;
    case 'I':
      Iflag = 1;
      break;
    case 'l':
      flags |= FLAG_l;
      break;
    case 'm':
      mflag = 1;
      break;
    case 'n':
      nflag = 1;
      break;
    case 'o':
      oarg = EARGF(usage(SHARGV0, shusg));
      i = chkopt(oarg);
      if (i >= 0)
        SETSHOPT(i);
      else {
        fprintf(stderr, "%s: -o: %s: invalid option name\n", argv0, oarg);
        exit(1);
      }
      break;
    case 's':
      sflag = 1;
      break;
    case 'u':
      uflag = 1;
      break;
    case 'v':
      vflag = 1;
      break;
    case 'V':
      Vflag = 1;
      break;
    case 'x':
      xflag = 1;
      break;
    default:
      fprintf(stderr, "%s: %c: bad option\n", argv0, ARGC());
      exit(1);
  }
  ARGEND

  flags |= ((argv0[0] == '-') || (flags & FLAG_l)) ? LOGIN : 0;

  if (flags & FLAG_i)
    iflag = mflag = 1;
  else if ((flags & FLAG_c) || (!sflag && *argv))
    iflag = mflag = 0;
  else
    iflag = mflag = (isatty(STDIN_FILENO) && isatty(STDERR_FILENO));

  /* all the set up functions for the shell */
  if (iflag)
    setlocale(LC_ALL, "C");
  init_stack();
  init_env();
  init_input();
  init_sig();
  init_builtins();
  if (mflag) {
    init_pgrp();
    init_job();
  }

  shin = stdin;
  shout = stdout;
  SHARGV0 = argv0;
  init_rc(flags);

  if (flags & FLAG_c) {
    if (vflag) {
      fputs(argv[0], stderr);
      fputc('\n', stderr);
    }
    SHARGV0 = argc > 1 ? argv[1] : argv0;
    SHARGV = argc > 2 ? argv + 2 : NULL;
    SHARGC = argc > 2 ? argc - 2 : 0;
    sh_ccmd(argv[0]);
    exittrap(LSTATUS);
  } else if (!sflag && *argv) {
    if ((fd = open(*argv, O_RDONLY)) < 0) {
      exittrap(sherrx(1, "open"));
    }
    SHARGV0 = *argv;
    SHARGV = argv + 1;
    SHARGC = argc - 1;
    sh_script(fd, *argv);
    exittrap(LSTATUS);
  } else if (iflag && !sflag) {
    SHARGV0 = argv0;
    /* run the main loop */
    exittrap(sh_interactive());
  } else {
    SHARGV0 = argv0;
    SHARGV = argv;
    SHARGC = argc;
    sh_stdin();
    exittrap(LSTATUS);
  }
}
