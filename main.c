#include "simpsh.h"

#define MAX_CMDS 256

int
main(int argc, char **argv) {
  const unsigned MAX_LENGTH = 256;
  char *line = (char *)NULL, *cmd = NULL, buf[MAX_LENGTH];
  char **args = (char **)NULL;
  int estatus, cflag = 0, tflag = 0, c_arg = 0;
  int tok_c, scan_s;
  cmd_tok toks[MAX_CMDS];
  cmd_tree *r;

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

  setlocale(LC_ALL, "");

  /* if using -c or aren't we connected to a terminal don't enter the loop*/
  if (cflag > 0 || tflag > 0) {
    args = getinput(cmd, " \n");
    if (getbuiltin(args) == 1) {
      if (builtin_launch(args) == -1)
        estatus = 1;
      else
        estatus = 0;
    } else
      estatus = shexec(args);
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

      /* parse user input */
      scan_s = scan_input(line, toks, &tok_c);

      /* if input is empty */
      if (scan_s > 0) {
        free(line);
        continue;
        /* if something failed*/
      } else if (scan_s < 0) {
        perror("failed to read input");
        free(line);
        continue;
      }

      /* take tokens from parsed input and build a tree
       * from them from them */
      r = build_tree(toks, tok_c, 0);

      /* run the commands in the tree */
      estatus = run_commands(r);

      freectree(r);
      freetoks(toks, tok_c);
      free(line);
    }

    if (line != NULL) {
      free(line);
      line = NULL;
    }
    exit(0);
  }
}
