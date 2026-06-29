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
#include "input.h"
#include "job.h"
#include "main.h"
#include "opts.h"
#include "sig.h"
#include "simpsh.h"
#include "var.h"

char histfile[PATH_MAX];
int builtin_tab[BUILTIN_BUCKETS];

const char ifsn[16] = "IFS";
const char envn[16] = "ENV";
const char pwdn[16] = "PWD";
const char oldpwdn[16] = "OLDPWD";
const char homen[16] = "HOME";
const char pathn[16] = "PATH";
const char ppidn[16] = "PPID";
const char shlvln[16] = "SHLVL";
const char shelln[16] = "SHELL";
const char shname[] = "simpsh";
const char linen[16] = "LINENO";
const char cdpthn[16] = "CDPATH";
const char ps1n[16] = "PS1";
const char ps2n[16] = "PS2";
const char ps4n[16] = "PS4";

/* global shell variables */
int sh_argc;
char *sh_argv0;
char **sh_argv;
ucharf alloc_sh_argv = 0;
int lstatus;
int retval = 0;
ucharf retnow = 0;
int loopdepth = 0;
int loopbreak = 0;
int loopcontinue = 0;

#define usage() fprintf(stderr, "Usage: simpsh [-abCefhiImnosvVx] [-o longopt] [-c 'cmd']\n")

/** shell entry point */
int
main(int argc, char **argv)
{
#ifdef TRACE
  mtrace();
#endif /* TRACE */

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
      oarg = EARGF(usage());
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

  sh_argv0 = argv0;
  init_rc(flags);

  if (flags & FLAG_c) {
    if (vflag) {
      fputs(argv[0], stderr);
      fputc('\n', stderr);
    } else if (!(flags & FLAG_i)) {
      iflag = 0;
      mflag = 0;
    }
    sh_argv0 = argc > 1 ? argv[1] : argv0;
    sh_argv = argc > 2 ? argv + 2 : NULL;
    sh_argc = argc > 2 ? argc - 2 : 0;
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
    sh_argv0 = argv0;
    sh_argv = argv + 1;
    sh_argc = argc - 1;
    sh_script(fd);
    exittrap(lstatus);
  } else if (!iflag || sflag) {
    sh_argv0 = argv0;
    sh_argv = argv;
    sh_argc = argc;
    sh_stdin();
    exittrap(lstatus);
  } else {
    sh_argv0 = argv0;
    /* run the main loop */
    exittrap(sh_interactive());
  }
}
