#include "simpsh.h"
#include "parser.h"

int sh_argc;
int lstatus;
pid_t sh_pid;
char *progname = "simpsh";
char *sh_pid_s = NULL;
char *sh_argv0;
char **sh_argv;
char histfile[265];
static int simpsh_run(char *line);

int
main(int argc, char **argv) {
  char *line = (char *)NULL;
  char *cmd = NULL, buf[MAX_LENGTH];
  char *home;
  int estatus, cflag = 0, tflag = 0, c_arg = 0;
  setlocale(LC_ALL, "");

  /*
   * if using -c or aren't we connected to a terminal we
   * don't enter the loop so we check for those first and handle
   * it accordingly
   */

  if (!isatty(STDIN_FILENO))
    tflag = 1;
  if (argc > 1) {
    if (strcmp(argv[1], "-c") == 0 && argc > 2) {
      cflag = 1;
      /* if we found -c we move argv[] here */
      c_arg += 2;
      argc -= 2;
    }
    cmd = argv[c_arg];
  }

  sh_argv0 = strdup(argv[0]);
  sh_argc = 0;
  sh_argv = NULL;
  sh_pid = getpid();
  sh_pid_s = malloc(16);
  snprintf(sh_pid_s, 16, "%d", sh_pid);

  if (cflag)
    exit(simpsh_run(strdup(cmd)));
  else if (tflag) {
    estatus = 1;
    while (fgets(buf, MAX_LENGTH, stdin) != NULL)
      estatus = simpsh_run(strdup(buf));
    exit(estatus);
  } else {

    /* set up history */
    home = getenv("HOME");
    snprintf(histfile, 265, "%s/.local/state/simpsh/simpsh_history", home);
    using_history();
    if (access(histfile, W_OK) < 0) {
      if (!create_histfile(home, histfile)) /* create if needed */
        perror("Failed to create simpsh_history:");
    } else {
      history_truncate_file(histfile, 1000);
      if (read_history_range(histfile, 0, -1) != 0)
        perror("Failed to read .simpsh_history");
    }

    /* the main loop */
    for (;;) {
      line = lineread();
      estatus = simpsh_run(line);
      lstatus = estatus;
    }

    if (line) {
      free(line);
      line = NULL;
    }
    if (sh_argv0)
      free(sh_argv0);

    exit(0);
  }
}

int
simpsh_run(char *line) {
  int estatus, tok_c;
  sh_tok *toks;
  // char *eline;
  cmd_tree *c;

  line = expand_alias(line);
  if (!(toks = tokenize(line, &tok_c)))
    estatus = 0;
  if (tok_c < 0)
    return 1;

  /* build the actual command from parsed input */
  c = build_tree(toks, tok_c);
  /* then run it */
  estatus = run_commands(c);
  append_history(1, histfile);

  if (c)
    freectree(c);
  if (line)
    free(line);
  freetoks(toks, tok_c);
  return estatus;
}
