#include "simpsh.h"

char *getfullpath(char *, char *);
int startsWithSlash(const char *);

char *
lineread(void) {
  /* get input for interactive shell */
  char *line = (char *)NULL;

  line = readline(" $ ");
  if (line && *line)
    add_history(line);

  return line;
}

char *
exp_var(char *args) {
  char *var, *t, *res, *vt = NULL;
  size_t args_l = strlen(args);
  size_t bufsize = args_l * 2;
  size_t i, j, p = 0;
  size_t vt_l, var_l;

  res = malloc(bufsize);
  if (!res) {
    perror("malloc failed");
    return NULL;
  }

  for (i = 0; i < args_l; i++) {
    if (args[i] == '$') {
      for (j = i + 1;; j++) {
        if ((isalnum(args[j]) == 0 && args[j] != '_') || args[j] == '\0') {
          vt_l = j - (i + 1);
          if (vt_l == 0) {
            var = "";
            var_l = vt_l;
            break;
          } else {
            vt = strndup(&args[i + 1], vt_l);
            if ((var = getenv(vt)) == NULL)
              var = "";
            var_l = strlen(var);
            if (p + var_l > bufsize) {
              bufsize = p + var_l + 256;
              t = realloc(res, bufsize);
              if (!t) {
                free(res);
                if (vt)
                  free(vt);
              }
              res = t;
            }
            break;
          }
        }
      }
      memcpy(&res[p], var, var_l);
      if (vt)
        free(vt);
      p += var_l;
      i = j - 1;
    } else {
      res[p] = args[i];
      p++;
    }
  }

  res[p] = '\0';
  return res;
}
char **
getinput(char *inputline, char *delim) {
  char *tokens;
  int i = 0;
  char **lineargs = malloc(sizeof(char *) * 1024);

  if (inputline == NULL) {
    return (char **)NULL;
  }

  if (!lineargs) {
    perror("malloc failed");
    return NULL;
  }

  /* break input line up into tokens to use later */
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
  /* check if command contains a / */
  if (str != NULL && str[0] == '/')
    return (1);

  return (0);
}

char *
getfullpath(char *path, char *file) {
  /* takes the command and checks eacch directory on path until it finds the
   * executable */
  char *pathcpy, *token;
  struct stat filepath;
  char *pathbuf = NULL;
  size_t bufsize;

  /* get path variable and seperate each directory to check for file later */
  pathcpy = strdup(path);
  token = strtok(pathcpy, ":");

  while (token) {
    if (pathbuf) {
      free(pathbuf);
      pathbuf = NULL;
    }
    bufsize = strlen(token) + strlen(file) + 2;
    pathbuf = malloc(bufsize);
    if (!pathbuf) {
      perror("Error: malloc failed");
      free(pathcpy);
      exit(EXIT_FAILURE);
    }
    /* add file to the end of the path the loop is currently checking */
    snprintf(pathbuf, bufsize, "%s/%s", token, file);

    /* see if file exists at current path, and if it's executable */
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
  /* fork and exec the command passed to the shell */
  int wstatus, estatus;
  pid_t pid;
  char *fullpath;

  /* test if the command has a /, if not return the first executable
     on PATH with the command name given */
  fullpath = getpath(&args[0]);
  if (fullpath == NULL) {
    fprintf(stderr, "%s: %s: command not found\n", name, args[0]);
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
    /* if fork was successful run the command */
    if (execve(fullpath, args, environ) == -1) {
      perror(args[0]);
      free(fullpath);
      exit(97);
    }
  } else {
    waitpid(-1, &wstatus, 0);
    estatus = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : 1;

    free(fullpath);
    return estatus;
  }
  return 1;
}
