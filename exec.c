#include "simpsh.h"
#include <stdio.h>

pid_t pid;
char *path;
int wstatus;

char *getpath(char **file) {
  char *fullpath;
  char *path = getenv("PATH");

  if (!path) {
    perror("Path not found");
    return (NULL);
  }

  fullpath = getfullpath(path, *file);
  if (fullpath == NULL) {
    perror("file not found");
    return (NULL);
  }

  return(fullpath);
}

char *getfullpath(char *path, char *file) {
  char *pathcpy, *splpath;
  struct stat filepath;
  char *pathbuf = NULL;

  pathcpy = strdup(path);
  splpath = strtok(pathcpy, ":");

  while (splpath) {

    if (pathbuf) {
      free(pathbuf);
      pathbuf = NULL;
    }
    pathbuf = malloc(strlen(splpath) + strlen(file) + 2);
    if (!pathbuf) {
      perror("Error: malloc failed");
      exit(EXIT_FAILURE);
    }
    strcpy(pathbuf, splpath);
    strcat(pathbuf, "/");
    strcat(pathbuf, file);
    strcat(pathbuf, "\0");

    if (stat(pathbuf, &filepath) == 0 && access(pathbuf, X_OK) == 0) {
      free(pathcpy);
      return (pathbuf);
    }
    splpath = strtok(NULL, ":");
  }
  free(pathcpy);
  if (pathbuf) {
    free(pathbuf);
  }
  return (NULL);
}

int shexec(char **args) {
  path = getpath(&args[0]);
  pid = fork();
  if (pid == -1) {
    perror("failed to create");
    exit(41);
  }

  if (pid == 0) {
    if (strchr(args[0], '/')) {
      execv(args[0], args);
    } else if (execv(path, args) == -1) {
      perror(args[0]);
      exit(97);
    }
  }
  else {
    wait(&wstatus);
  }
  return 1;
}

