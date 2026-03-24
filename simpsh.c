#include "simpsh.h"
#include <readline/readline.h>

static int create_histfile(char *, char *);
static char *getfullpath(const char *, const char *);
static int startsWithSlash(const char *);
static int pmkdir(char *path);

char *
lineread(void) {
  char *line = readline(" $ ");
  if (line && *line)
    add_history(line);

  return line;
} /* get input for interactive shell */

int
create_histfile(char *home, char *histfile) {
  static const char *histdir = ".local/state/simpsh";
  char buf[256];

  snprintf(buf, 256, "%s/%s", home, histdir);
  if (!pmkdir(buf))
    return 0;
  if (access(histfile, W_OK) < 0)
    if (write_history(histfile) != 0)
      return 0;
  return 1;
}

void
init_history(void) {
  char histfile[265];
  snprintf(histfile, 265, "%s/.local/state/simpsh/simpsh_history", home);
  using_history();
  if (access(histfile, W_OK) < 0) {
    if (!create_histfile(home, histfile)) /* create if needed */
      perror("Failed to create simpsh_history:");
  } else {
    history_truncate_file(histfile, 1000);
    if (read_history_range(histfile, 0, -1) != 0)
      perror("Failed to read .simpsh_history");
  }
}

int
startsWithSlash(const char *str) {
  if (str != NULL && str[0] == '/')
    return (1);

  return (0);
} /* check if command contains a / */

static char *
getfullpath(const char *path, const char *file) {
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

  return NULL;
} /* takes the command and checks each directory on path until it finds the executable */

char *
getpath(char **file) {
  char *fullpath;
  char *path = getenv("PATH");

  if (startsWithSlash(*file) && access(*file, X_OK) == 0)
    return (strdup(*file));

  if (!path) {
    perror("PATH not set");
    return NULL;
  }

  fullpath = getfullpath(path, *file);
  if (!fullpath) 
    return NULL;
  return fullpath;
}

static int
pmkdir(char *path) {
  char *dir;

  dir = strchr(path +1, '/');
  while (dir) {
    *dir = 0;
    if (access(path, F_OK) < 0 && mkdir(path, S_IRWXU|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH) < 0)
      return 0;
    *dir = '/';
    dir = strchr(dir+1, '/');
  }
  if (mkdir(path, S_IRWXU|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH) < 0)
    return 0;
  return 1;
}

int
shexec(char **args) {
  int wstatus, estatus;
  pid_t pid;
  char *fullpath;

  /* test if the command has a /, if not return the first executable
     on PATH with the command name given */
  fullpath = getpath(&args[0]);
  if (!fullpath) {
    fprintf(stderr, "%s: %s: command not found\n", sh_argv0, args[0]);
    goto done;
  }

  pid = fork();
  if (pid == -1) {
    perror("failed to create");
    goto done;
  }

  if (pid == 0) { /* if fork was successful run the command */
    if (execve(fullpath, args, environ) == -1) {
      perror(args[0]);
      goto done;
    }
  } else {
    waitpid(-1, &wstatus, 0);
    estatus = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : 1;
    free(fullpath);
    return estatus;
  }

  goto done;

done:
  if (fullpath)
    free(fullpath);
  estatus = 1;
  return estatus;
} /* fork and exec the command passed to the shell */
