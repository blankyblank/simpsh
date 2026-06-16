/* simpsh - a simple posix shell */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <unistd.h>
#ifdef READLINE
  #include <readline/history.h>
#endif /* ifdef READLINE */

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

char histfile[256];
int builtin_tab[BUILTIN_BUCKETS];

/* global shell variables */
int sh_argc;
pid_t sh_pid;
pid_t sh_ppid;
char *sh_ppid_s = NULL;
char *sh_pid_s = NULL;
char *sh_bgpid_s = NULL;
pid_t sh_bgpid;
char *sh_argv0;
char *sh_ps1;
char *sh_ps2;
char *sh_ps4;
char **sh_argv;
int alloc_sh_argv = 0;
char *home;
int lstatus;
int retval = 0;
int retnow = 0;

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
      getbuildinfo();
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


  /* setup locale then */
  /* set up allocator then */
  /* set up shell variables then */
  /* set up builtin table then */
  /* set up input layer then */
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

  snprintf(histfile, 256, "%s/.local/state/simpsh/simpsh_history", home);

  if (flags & FLAG_c) {
    if (vflag) {
      fputs(argv[0], stderr);
      fputc('\n', stderr);
    }
    sh_argv0 = argc > 1 ? argv[1] : argv0;
    sh_argv = argc > 2 ? argv + 2 : NULL;
    sh_argc = argc > 2 ? argc - 2 : 0;
    sh_ccmd(argv[0]);
    exit(lstatus);
  } else if (!sflag && *argv) {
    if (!(flags & FLAG_i)) {
      iflag = 0;
      mflag = 0;
    }
    if ((fd = open(*argv, O_RDONLY)) < 0) {
      perror("simpsh");
      exit(1);
    }
    sh_argv0 = argv0;
    sh_argv = argv + 1;
    sh_argc = argc - 1;
    sh_script(fd);
    exit(lstatus);
  } else if (!iflag || sflag) {
    sh_argv0 = argv0;
    sh_argv = argv;
    sh_argc = argc;
    sh_stdin();
    exit(lstatus);
  } else {
    sh_argv0 = argv0;
    /* run the main loop */
    exit(sh_interactive());
  }
}
