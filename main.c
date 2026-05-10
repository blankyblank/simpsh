/* simpsh - a simple posix shell */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <unistd.h>
#include <locale.h>
#include <stdio.h>
#ifdef READLINE
#include <readline/history.h>
#endif /* ifdef READLINE */

#include "main.h"
#include "simpsh.h"
#include "malloc.h"
#include "lex.h"
#include "exec.h"
#include "env.h"
#include "utils.h"
#include "arg.h"

/* shell variables */
int sh_argc;
pid_t sh_pid;
char *sh_pid_s = NULL;
char *sh_argv0;
char **sh_argv;
char *home;
int lstatus;

/* runtime state */
char histfile[265];
static int interactive = 1;
int builtin_tab[BUILTIN_BUCKETS];

static int simpsh_run(char *line);

/** shell entry point */
int
main(int argc, char **argv)
{
  (void)argc;
  char *line;
  char buf[MAX_LENGTH];
  int estatus, flags;

  flags = 0;
  estatus = -1;
  /* set up shell variables */
  sh_argv0 = "simpsh";
  sh_pid = getpid();
  home = getenv("HOME");
  sh_argc = 0;
  sh_argv = NULL;

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
  if (flags & (FLAG_c | FLAG_t))
    interactive = 0;
  setlocale(LC_ALL, "");
  init_stack();
  init_env();
  init_builtins();

  snprintf(histfile, 265, "%s/.local/state/simpsh/simpsh_history", home);
  sh_pid_s = malloc(16);
  snprintf(sh_pid_s, 16, "%d", sh_pid);

  if (flags & FLAG_c) {
    exit(simpsh_run(s_strdup(argv[0])));
  } else if (flags & FLAG_t) {
    while (fgets(buf, MAX_LENGTH, stdin) != NULL)
      estatus = simpsh_run(s_strdup(buf));
    exit(estatus);
  } else {
    /* run the main loop */

    #ifdef READLINE
    init_history(); /* set up history */
    #endif /* ifdef READLINE */
    for (;;) {
      line = lineread();
      if (!line)
        break;
      estatus = simpsh_run(line);
      lstatus = estatus;
    }

    free(line);
    exit(0);
  }
}

/** run a command line */
int
simpsh_run(char *line)
{
  int estatus, tok_c;
  sh_tok *toks;
  cmd_tree *c;
  stmark mark;

  mark = stack_mark();
  if (!(toks = tokenize(line, &tok_c)))
    estatus = 0;
  if (tok_c < 0) {
    stack_restore(mark);
    return 1;
  }

  c = build_tree(toks, tok_c, TEOF); /* build the actual command from parsed input */
  estatus = run_commands(c);   /* then run commands */
  if (interactive)             /* save command to history file */
    #ifdef READLINE
    append_history(1, histfile);
    #endif /* ifdef READLINE */

  free(line);
  stack_restore(mark);
  return estatus;
}
