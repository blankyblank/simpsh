#include "simpsh.h"
#include <stdio.h>

int builtinnum(void);
int cdsetpwd(char *);

// clang-format off
char *builtins[] = {
  "cd",
  "exit",
  "false",
  "help",
  "pwd",
};
int (*builtin_funcs[])(char **) = {
  &cdcmd,
  &exitcmd,
  &falsecmd,
  &helpcmd,
  &pwdcmd,
};
int builtinnum(void) {
  return sizeof(builtins) / sizeof(char *);
}

// clang-format on

int
getbuiltin(char **args) {
  int n = builtinnum();
  int i;

  for (i = 0; i < n; i++) {
    if (!strcmp(args[0], builtins[i])) {
      return 1;
    }
  }
  return 0;
}

int
builtin_launch(char **args) {
  int n = builtinnum();
  int i;
  for (i = 0; i <= n; i++) {
    if (!strcmp(*args, builtins[i])) {
      return (*builtin_funcs[i])(args);
    }
  }
  printf("something didn't work");
  return 1;
}

int
cdsetpwd(char *arg) {
  char respath[PATH_MAX];
  char *newpwd = NULL;
  // struct stat filepath;
  char *oldpwd = getenv("PWD");
  // char *oldpwd = getenv("PWD");
  getcwd(oldpwd, sizeof(oldpwd + 1));
  if (!strcmp(arg, "-")) {
    newpwd = getenv("OLDPWD");
  } else {
    if (realpath(arg, respath) != NULL) {
      newpwd = realpath(arg, respath);
    } else {
      perror(arg);
      return -1;
    }
  }

  setenv("OLDPWD", oldpwd, 1);
  setenv("PWD", newpwd, 1);
  if (chdir(newpwd) == -1) {
    return -1;
  }
  return 0;
}

int
cdcmd(char **args) {
  char *homedir = getenv("HOME");
  char **dir = args;
  if (dir[1] == 0) {
    cdsetpwd(homedir);
  } else if (cdsetpwd(dir[1]) == -1) {
    return -1;
  }
  return 0;
}

int
exitcmd(char **argv) {
  char **args = (char **)NULL;
  (void)argv;
  if (args != NULL) {
    freeptr(args);
  }
  exit(0);
}

int
falsecmd(char **args) {
  (void)args;
  return 1;
}

int
helpcmd(char **args) {
  (void)args;
  char **helparray = builtins;
  int i;

  printf("These are the builtin commands included with simpsh:\n\n");
  for (i = 0; i < 5; i++) {
    printf("%s \n", helparray[i]);
  }
  return 0;
}

int
pwdcmd(char **args) {
  (void)args;
  // char pwdbuf[PATH_MAX];
  char *pwdbuf = NULL;
  char *pwd;
  pwd = getcwd(pwdbuf, 0);
  // if (getcwd(pwdbuf, PATH_MAX)) {
  if (pwd) {
    printf("%s\n", pwd);
    free(pwd);
    return 0;
  }
  return 1;
}
