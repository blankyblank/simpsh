#include "simpsh.h"

static char *line = (char *)NULL;
int i;

int shell_loop(char **argv) {
  (void)argv;
  char **args;
  while (1) {
    line = readline(" $ ");
    if (line && *line) {
      add_history(line);
    }
    args = parseargs(&line, " \n");

    if (getbuiltin(&args[0]) == 1) {
      builtin_launch(args);
    } else {
      shexec(args);
    }
  }
  free(line);
  return (0);
}
