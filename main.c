#include "simpsh.h"

int
main(int argc, char **argv) {
  const unsigned MAX_LENGTH = 256;
  char *line = (char *)NULL, *cmd = NULL, buf[MAX_LENGTH];
  char **args = (char **)NULL;
  int estatus, cflag = 0, tflag = 0, c_arg = 0;

  /* check for cli flags */
  if (argc > 1) {
    if (strcmp(argv[1], "-c") == 0 && argc > 2) {
      cflag = 1;
      /* if we found -c we move argv[] here */
      c_arg += 2;
      argc -= 2;
    }
    cmd = argv[c_arg];
  }

  /* check if stdin is a terminal */
  if (!isatty(STDIN_FILENO)) {
    tflag = 1;
    if ((cmd = fgets(buf, MAX_LENGTH, stdin)) == NULL) {
      perror("couldn't get command");
    }
  }

  /* set up locale */
  setlocale(LC_ALL, "");

  /* if using -c or aren't we connected to a terminal don't enter the loop*/
  if (cflag > 0 || tflag > 0) {
    args = getinput(cmd, " \n");
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
    for (;;) {
      if (args != NULL) {
        freeptr(args);
        args = NULL;
      }
      /* calls readline */
      line = lineread();

      /* takes user input */
      if ((args = getinput(line, " \n")) == NULL || args[0] == NULL) {
        /* the if statement checks for empty lines to handle them properly */
        free(line);
        line = NULL;
        continue;
      }

      /* check if it's a builtin command or not and run it */
      if (getbuiltin(&args[0]) == 1) {
        builtin_launch(args);
        free(line);
        line = NULL;
      } else {
        shexec(args);
      }

      freeptr(args);
      free(line);
      args = NULL;
      line = NULL;
    }

    if (line != NULL) {
      free(line);
      line = NULL;
    }
    exit(0);
  }
}
