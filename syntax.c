#include "simpsh.h"

// char **listcmd(char **);
// int orcmd(char **);
// int andcmd(char **);

char *tokens;

char **parseargs(char **inputline, char * delim) {
  int i;
  char **lineargs;

  char *linecpy = *inputline;
  tokens = strtok(linecpy, delim);
  lineargs = malloc(sizeof(char*) * 1024);
  i = 0;
  while (tokens) {
    lineargs[i] = tokens;
    tokens = strtok(NULL, delim);
    i++;
  }
  lineargs[i] = NULL;
  free(tokens);
  return lineargs;
}

// char **listcmd(char **args) {
//   int i;
//   char *cmdcpy;
//   char **splcmd = NULL;
//   char **listbuf = NULL;
//
//   cmdcpy = strdup(*args);
//   *splcmd = strtok(cmdcpy, ";");
//
//   while (splcmd) {
//     if (listbuf) {
//       free(listbuf);
//       listbuf = NULL;
//     }
//     listbuf = malloc(strlen(*splcmd) + 1);
//     if (!listbuf) {
//       perror("Error: malloc failed");
//       exit(EXIT_FAILURE);
//     }
//     splcmd = strtok(NULL, ";");
//   }
//   return 0;
// }
