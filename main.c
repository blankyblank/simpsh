#include "simpsh.h"
#include "parser.h"

int sh_argc;
int lstatus;
pid_t sh_pid;
char *sh_pid_s = NULL;
char *sh_argv0;
char **sh_argv;
static int simpsh_run(char *line);

int
main(int argc, char **argv) {
  char *line = (char *)NULL, *cmd = NULL, buf[MAX_LENGTH];
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

  if (cflag > 0)
    exit(simpsh_run(strdup(cmd)));
  else if (tflag > 0) {
    estatus = 1;
    while (fgets(buf, MAX_LENGTH, stdin) != NULL)
      estatus = simpsh_run(strdup(buf));
    exit(estatus);
  } else {
    using_history();

    /* the main loop */
    for (;;) {
      line = lineread();
      estatus = simpsh_run(line);
    }

    if (!line) {
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
  cmd_tree *c;

  /* variable expansion */
  // if (line && strchr(line, '$')) {
  //   if ((vline = exp_var_old(line)) == NULL) {
  //     free(line);
  //     line = vline;
  //     return 1;
  //   }
  //   free(line);
  //   line = vline;
  // }

  // toks = 
  // if ((scan_s = scan_input(line, toks, &tok_c)) > 0) {
  //   /* if input is empty */
  //   free(line);
  //   return 0;
  //   /* if something failed*/
  // } else if (scan_s < 0) {
  //   fprintf(stderr, "%s: Failed to read input\n", sh_argv0);
  //   free(line);
  //   return 1;
  // }

  if (!(toks = tokenize(line, &tok_c))) {
    perror("failed to get input (maybe)");
    estatus = 1;
  }

  /* build the actual command from parsed input */
  c = build_tree(toks, tok_c);
  /* then run it */
  estatus = run_commands(c);

  if (c) {
    freectree(c);
  }
  if (line) {
    free(line);
  }
  freetoks(toks, tok_c);
  return estatus;
}
