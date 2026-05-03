/* simpsh.c - functions for running the shell */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/limits.h>
#include "main.h"
#include "simpsh.h"
#include "utils.h"

void
getbuildinfo(void) {
  printf("%s build info:\n"
         "build date: %s %s\n"
         "ansi C standard conformance: %ld\n",
         sh_argv0, __DATE__, __TIME__, __STDC_ISO_10646__);
}

static int create_histfile(char *, char *);
static char *getfullpath(const char *, const char *);
// static int startsWithSlash(const char *);
/** make directory path */
static int pmkdir(char *path);

/** read line from interactive shell */
char *
lineread(void)
{
  char *line = readline(" $ ");
  if (line && *line)
    add_history(line);

  return line;
}

/** create history file */
int
create_histfile(char *home, char *histfile)
{
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

/** initialize history */
void
init_history(void)
{
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

/** check in path for file until stop at the first one with an executable */
static char *
getfullpath(const char *path, const char *file)
{
  struct stat filepath;
  char *pathcpy, *s, *c;

  pathcpy = s_strdup(path);
  s = pathcpy;

  for (c = strchr(s, ':'); c; c = strchr(s, ':')) {
    char buf[PATH_MAX];
    *c = '\0';
    /* add file to the end of the path the loop is currently checking */
    snprintf(buf, PATH_MAX, "%s/%s", s, file);
    /* see if file exists at current path, and if it's executable */
    if (stat(buf, &filepath) == 0 && access(buf, X_OK) == 0) {
      free(pathcpy);
      return st_strdup(buf);
    }
    s = c + 1;
  }

  free(pathcpy);
  return NULL;
}

/** get full path to executable */
char *
getpath(char *file)
{
  char *fullpath;

  if ((strchr(file, '/')) && access(file, X_OK) == 0)
    return (st_strdup(file));

  const char *path = getenv("PATH");
  if (path)
    fullpath = getfullpath(path, file);
  else
    fullpath = getfullpath(defpath, file);

  if (!fullpath)
    return NULL;
  return fullpath;
}

/**  created directories and their parents if they don't exist  */
static int
pmkdir(char *path)
{
  char *dir; /* clang-format off */

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
  /* clang-format on */
}
