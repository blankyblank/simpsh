#include "simpsh.h"

char *
lineread(void) {
  char *line = (char *)NULL;

  line = readline(" $ ");
  if (line && *line)
    add_history(line);

  return line;
}

char **
readinput(char *inputline, char *delim) {
  char *tokens;
  int i = 0;
  char **lineargs = malloc(sizeof(char *) * 1024);
  if (!lineargs) {
    perror("malloc failed");
    return NULL;
  }

  // lineargs = malloc(sizeof(char*) * 1024);
  tokens = strtok(inputline, delim);

  while (tokens) {
    lineargs[i] = strdup(tokens);
    if (!lineargs[i]) {
      perror("strdup failed");
      for (int j = 0; j < i; j++)
        free(lineargs[j]);
      free(lineargs);
      return NULL;
    }
    tokens = strtok(NULL, delim);
    i++;
  }
  lineargs[i] = NULL;
  return lineargs;
}

int
startsWithSlash(const char *str) {
  if (str != NULL || str[0] == '/')
    return (1);

  return (0);
}

char *
getfullpath(char *path, char *file) {
  char *pathcpy, *token;
  struct stat filepath;
  char *pathbuf = NULL;

  pathcpy = strdup(path);
  token = strtok(pathcpy, ":");

  while (token) {
    if (pathbuf) {
      free(pathbuf);
      pathbuf = NULL;
    }
    pathbuf = malloc(strlen(token) + strlen(file) + 2);
    if (!pathbuf) {
      perror("Error: malloc failed");
      exit(EXIT_FAILURE);
    }
    strcpy(pathbuf, token);
    strcat(pathbuf, "/");
    strcat(pathbuf, file);
    strcat(pathbuf, "\0");

    if (stat(pathbuf, &filepath) == 0 && access(pathbuf, X_OK) == 0) {
      free(pathcpy);
      return (pathbuf);
    }
    token = strtok(NULL, ":");
  }
  free(pathcpy);
  if (pathbuf)
    free(pathbuf);
  return (NULL);
}

char *
getpath(char **file) {
  char *fullpath;
  char *path = getenv("PATH");

  if (startsWithSlash(*file) && access(*file, X_OK) == 0)
    return (strdup(*file));

  if (!path) {
    perror("PATH not set");
    return (NULL);
  }

  fullpath = getfullpath(path, *file);
  if (fullpath == NULL) {
    return (NULL);
  }
  return (fullpath);
}

int
shexec(char **args) {
  int wstatus;
  int estatus;
  pid_t pid;
  char *fullpath;

  fullpath = getpath(&args[0]);
  if (fullpath == NULL) {
    perror("command not found");
    if (fullpath)
      free(fullpath);
    return 1;
  }

  pid = fork();
  if (pid == -1) {
    perror("failed to create");
    free(fullpath);
    exit(41);
  }

  if (pid == 0) {
    if (execve(fullpath, args, NULL) == -1) {
      perror(args[0]);
      free(fullpath);
      exit(97);
    }
  } else {
    estatus = waitpid(-1, &wstatus, 0);

    return estatus;
  }
  wait(&wstatus);
  free(fullpath);

  return 1;
}
