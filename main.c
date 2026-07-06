/* simpsh - a simple posix shell */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <alloca.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include "alloc.h"
#include "arg.h"
#include "builtins.h"
#include "error.h"
#include "input.h"
#include "job.h"
#include "main.h"
#include "opts.h"
#include "sig.h"
#include "simpsh.h"
#include "var.h"

char histfile[PATH_MAX];
int builtin_tab[BUILTIN_BUCKETS];
const char shname[] = "simpsh";
const char shusg[43] = "[-abCefhiImnosvVx] [-o longopt] [-c 'cmd']";
GSTATE gstate;

/* global shell variables */

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
      oarg = EARGF(usage(shname, shusg));
      i = chkopt(oarg);
      if (i >= 0)
        shopts[i] = 1;
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
#ifndef MUSL
      getbuildinfo();
#endif /* ifndef MUSL */
      exit(0);
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

  /* all the set up functions for the shell */
  setlocale(LC_ALL, "");
  init_stack();
  init_env();
  init_builtins();
  init_input();
  init_sig();
  if (mflag) {
    init_pgrp();
    init_job();
  }

  shargv0 = argv0;
  init_rc(flags);

  if (flags & FLAG_c) {
    if (vflag) {
      fputs(argv[0], stderr);
      fputc('\n', stderr);
    } else if (!(flags & FLAG_i)) {
      iflag = 0;
      mflag = 0;
    }
    shargv0 = argc > 1 ? argv[1] : argv0;
    shargv = argc > 2 ? argv + 2 : NULL;
    shargc = argc > 2 ? argc - 2 : 0;
    sh_ccmd(argv[0]);
    exittrap(lstatus);
  } else if (!sflag && *argv) {
    if (!(flags & FLAG_i)) {
      iflag = 0;
      mflag = 0;
    }
    if ((fd = open(*argv, O_RDONLY)) < 0) {
      perror("simpsh");
      exittrap(1);
    }
    shargv0 = argv0;
    shargv = argv + 1;
    shargc = argc - 1;
    sh_script(fd);
    exittrap(lstatus);
  } else if (!iflag || sflag) {
    shargv0 = argv0;
    shargv = argv;
    shargc = argc;
    sh_stdin();
    exittrap(lstatus);
  } else {
    shargv0 = argv0;
    /* run the main loop */
    exittrap(sh_interactive());
  }
}
