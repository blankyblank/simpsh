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

#include "arg.h"
#include "env.h"
#include "exec.h"
#include "input.h"
#include "main.h"
#include "malloc.h"
#include "simpsh.h"

char histfile[265];
int builtin_tab[BUILTIN_BUCKETS];

/* global shell variables */
int sh_argc;
pid_t sh_pid;
char *sh_pid_s = NULL;
char *sh_argv0;
char *shps1;
char **sh_argv;
char *home;
int lstatus;

/** shell entry point */
int
main(int argc, char **argv)
{
  (void)argc;
  int flags, fd;

  flags = 0;
  ARGBEGIN
  {
    case 'c':
      flags |= FLAG_c;
      break;
    case 'V':
      getbuildinfo();
      exit(0);
    default:
      fprintf(stderr, "%s: %c: bad option\n", argv0, ARGC());
      exit(1);
  }
  ARGEND

  if (!isatty(STDIN_FILENO))
    flags |= FLAG_t;

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

  snprintf(histfile, 265, "%s/.local/state/simpsh/simpsh_history", home);

  if (flags & FLAG_c) {
    sh_ccmd(argv[0]);
    exit(lstatus);
  } else if (*argv) {
    if ((fd = open(*argv, O_RDONLY)) < 0) {
      perror("simpsh");
      exit(1);
    }
    sh_script(fd);
    exit(lstatus);
  } else if (flags & FLAG_t) {
    sh_stdin();
    exit(lstatus);
  } else {
    /* run the main loop */
    exit(sh_interactive());
  }
}
