/* simpsh - a simple posix shell */
#define _POSIX_C_SOURCE 200809L

#include "simpsh.h"
#include "malloc.h"
#include "lex.h"
#include "exec.h"
#include "env.h"
#include "utils.h"
#include <locale.h>
#include <readline/history.h>

int lstatus;
static int interactive = 1;
int builtin_tab[BUILTIN_BUCKETS];

/* shell variables */
int sh_argc;
pid_t sh_pid;
char *sh_pid_s = NULL;
char *sh_argv0;
char histfile[265];
char *progname = "simpsh";
char **sh_argv;
char *home;

static int simpsh_run(char *line);

/** shell entry point */
int
main(int argc, char **argv)
{
  char *line = (char *)NULL;
  char *cmd = NULL, buf[MAX_LENGTH];
  int estatus = -1, cflag = 0, tflag = 0;
  setlocale(LC_ALL, "");

  /* check if using -c flag or connected to stdin */
  if (!isatty(STDIN_FILENO))
    tflag = 1;
  if (argc > 1) {
    if (strcmp(argv[1], "-c") == 0 && argc > 2) {
      cflag = 1;
      argc -= 2;
    }
    cmd = argv[2]; /* if we found -c we move argv[] here */
  }
  if (cflag || tflag)
    interactive = 0;

  /* set up shell variables */
  sh_argv0 = s_strdup(argv[0]);
  sh_argc = 0;
  sh_argv = NULL;
  sh_pid = getpid();
  sh_pid_s = malloc(16);
  home = getenv("HOME");
  snprintf(histfile, 265, "%s/.local/state/simpsh/simpsh_history", home);
  snprintf(sh_pid_s, 16, "%d", sh_pid);

  init_env();
  init_builtins();

  if (cflag) {
    exit(simpsh_run(s_strdup(cmd)));
  } else if (tflag) {
    while (fgets(buf, MAX_LENGTH, stdin) != NULL)
      estatus = simpsh_run(s_strdup(buf));
    exit(estatus);
  } else {
    /* run the main loop */

    init_history(); /* set up history */
    for (;;) {
      line = lineread();
      estatus = simpsh_run(line);
      lstatus = estatus;
    }

    if (line)
      free(line);
    if (sh_argv0)
      free(sh_argv0);
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
  // size_t i = 0;

  mark = stack_mark();
  if (!(toks = tokenize(line, &tok_c)))
    estatus = 0;
  if (tok_c < 0) {
    stack_restore(mark);
    return 1;
  }

  // c = parse_cmd(toks, tok_c, TEOF, &i); /* build the actual command from parsed input */
  c = build_tree(toks, tok_c, TEOF); /* build the actual command from parsed input */
  estatus = run_commands(c);   /* then run commands */
  if (interactive)             /* save command to history file */
    append_history(1, histfile);

  if (line)
    free(line);
  stack_restore(mark);
  return estatus;
}
