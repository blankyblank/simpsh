#include "simpsh.h"
#include <string.h>

int
main(int argc, char **argv) {
  char *line = (char *)NULL, *cmd;
  char **args = (char **)NULL;
  (void)argc;
  int cflag = 0, tflag = 0, c = 0;
  /* (void)argv; */

  if (argc > 1) {
    if (strcmp(argv[1], "-c") == 0 && argc > 2) {
      cflag = 1;
      c += 2;
      argc -= 2;
    }
  }
  if (!isatty(STDOUT_FILENO)) {
    tflag = 1;
    cflag = 0;
  }

  setlocale(LC_ALL, "");
  using_history();

  if (cflag > 0) {
    cmd = argv[c];
    args = readinput(cmd, " \n");

    if (getbuiltin(args) == 1) {
      builtin_launch(args);
    } else {
      shexec(args);
    }
    exit(0);

  } else if (tflag > 0) {
    exitcmd(0);
  } else {
    while (1) {
      if (args != NULL) {
        freeptr(args);
        args = NULL;
      }
      line = lineread();
      args = readinput(line, " \n");
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
