#include "simpsh.h"

int
main(int argc, char **argv) {
  char *line = (char *)NULL, *cmd;
  char **args = (char **)NULL;
  int estatus, cflag = 0, tflag = 0, c = 0;

  /* check of command line flags */
  if (argc > 1) {
    if (strcmp(argv[1], "-c") == 0 && argc > 2) {
      cflag = 1;
      c += 2;
      argc -= 2;
    }
  }
  /* check if stdin is a terminal */
  if (!isatty(STDOUT_FILENO)) {
    tflag = 1;
  }

  /* set up locale */
  setlocale(LC_ALL, "");

  /* if using -c don't enter the loop*/
  if (cflag > 0 || tflag > 0) {
    cmd = argv[c];
    args = readinput(cmd, " \n");

    if (getbuiltin(args) == 1) {
      if (builtin_launch(args) == -1)
        estatus = 1;
      else
        estatus = 0;
    } else {
      estatus = shexec(args);
    }

    exit(estatus);
  } else {
    /* set up histor y*/
    using_history();

    /* the main loop for the interactive shell */
    while (1) {
      if (args != NULL) {
        freeptr(args);
        args = NULL;
      }
      line = lineread();
      args = readinput(line, " \n");
      /* check if it's a builtin command or not and run it */
      if (getbuiltin(&args[0]) == 1) {
        builtin_launch(args);
        free(line);
        line = NULL;
      } else {
        shexec(args);
        free(line);
        line = NULL;
      }
      if (args != NULL) {
        freeptr(args);
        args = NULL;
      }
    }

    if (line != NULL) {
      free(line);
      line = NULL;
    }
    exit(0);
  }
}

void
freeptr(char **args) {
  if (args != NULL) {
    int i = 0;
    while (args[i] != NULL) {
      free(args[i]);
      i++;
    }
    free(args);
  }
}
