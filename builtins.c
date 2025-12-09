#include "simpsh.h"

int builtinnum(void);
int cdsetpwd(char *);

char *builtins[] = {
  "cd",
  "pwd",
  "false",
  "exit"
};
int (*builtin_funcs[])(char **) = {
  &cdcmd,
  &pwdcmd,
  &falsecmd,
  &exitcmd
};

int builtinnum(void) {
  return sizeof(builtins) / sizeof(char *);
}
int getbuiltin(char **args) {
  int n = builtinnum();
  int i;

  for (i = 0; i < n; i++) {
    if (!strcmp(args[0], builtins[i])) {
      return 1;
    }
  }
  return 0;
}
int builtin_launch(char **args) {
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

int cdcmd(char **args) {
  char *homedir = getenv("HOME");
  char **dir = args;
  if (dir[1] == 0){
    cdsetpwd(homedir);
  }
  else if (cdsetpwd(dir[1]) == -1) {
    perror(*args);
  }
  return 0;
}
int cdsetpwd(char *arg) {
  char respath[PATH_MAX];
  char *newpwd;
  char *oldpwd = getenv("PWD");
  getcwd(oldpwd, PATH_MAX);
  if(!strcmp(arg, "-")) {
    newpwd = getenv("OLDPWD");
  } else {
    newpwd = realpath(arg, respath);
  }

  setenv("OLDPWD", oldpwd, 1);
  setenv("PWD", newpwd, 1);
  if (chdir(newpwd) == -1) {
    return -1;
  }
  return 0;
}

int pwdcmd(char **args) {
  (void)args;
  char pwdbuf[PATH_MAX];
  char *pwd;
  pwd = getcwd(NULL, 0);
  if (getcwd(pwdbuf, PATH_MAX)) {
    printf("%s\n", pwd);
    free(pwd);
    return 0;
  }
  return 1;
}

int falsecmd(char **args) {
  (void)args;
  return 1;
}
int exitcmd(char **args) {
  (void)args;
  
  return kill(getpid(), SIGTERM);
}

