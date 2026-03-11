#include "simpsh.h"

int argcount(char **);
int builtinnum(void);
int cdsetpwd(char *);
int echo_print(int, int, char **);
int exportcmd(char **);
// clang-format off
/* the array of builtin commands */
char *builtins[] = {
  "cd",
  "echo",
  "exec",
  "exit",
  "export",
  "false",
  "help",
  "pwd",
};
int (*builtin_funcs[])(char **) = {
  &cdcmd,
  &echocmd,
  &execcmd,
  &exitcmd,
  &exportcmd,
  &falsecmd,
  &helpcmd,
  &pwdcmd,
};
int builtinnum(void) {
  return sizeof(builtins) / sizeof(char *);
}

// clang-format on

int
argcount(char **args) {
  int argc = 0;

  while (args[argc] != NULL)
    argc++;

  return argc;
}

int
getbuiltin(char **args) {
  /* check if string matches builtin command */
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
  /* launch builtin */
  int n = builtinnum();
  int i;
  for (i = 0; i < n; i++) {
    if (!strcmp(*args, builtins[i])) {
      return (*builtin_funcs[i])(args);
    }
  }
  printf("something didn't work");
  return 1;
}

int
cdsetpwd(char *arg) {
  char respath[PATH_MAX], oldpwd[PATH_MAX];
  char *newpwd = NULL;

  /* char *oldpwd = getenv("PWD"); */
  getcwd(oldpwd, sizeof(oldpwd));
  if (!strcmp(arg, "-")) {
    newpwd = getenv("OLDPWD");
    if (newpwd == NULL)
      return -1;
  } else {
    if (realpath(arg, respath) != NULL)
      newpwd = realpath(arg, respath);
    else {
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
    return 1;
  }
  return 0;
}

int
echocmd(char *args[]) {
  int n;
  int stat, argc;

  argc = argcount(args);

  if (argc >= 2) {
    if (strcmp(args[1], "-n") == 0 && argc > 2) {
      n = 1;
      stat = echo_print(argc, n, args);
      if (stat == -1) {
        printf("something went wrong\n");
        return 1;
      }

    } else if (strcmp(args[1], "-n") == 0) {
      return 0;
    } else {
      n = 0;
      stat = echo_print(argc, n, args);
      if (stat == -1)
        return 1;
      return 0;
    }
  } else {
    printf("\n");
    return 0;
  }
  return 0;
}

int
echo_print(int argc, int n, char *args[]) {
  int i;
  char s = ' ';
  if (n == 1) {
    for (i = 2; i < argc; i++) {
      if (i < argc - 1) {
        printf("%s%c", args[i], s);
      } else {
        printf("%s", args[i]);
      }
    }
    return 0;
  } else {
    for (i = 1; i < argc; i++) {
      if (i < argc - 1) {
        printf("%s%c", args[i], s);
      } else {
        printf("%s", args[i]);
      }
    }
    printf("\n");
    return 0;
  }
  return 1;
}

int
execcmd(char **args) {
  char *fullpath;

  fullpath = getpath(&args[1]);

  if (args[1] == NULL) {
    perror("Missing command");
    return 1;
  }

  if (fullpath == NULL) {
    perror("command not found");
    if (fullpath)
      free(fullpath);
    return 1;
  }

  if (execve(fullpath, &args[1], environ) < 0) {
    perror(args[0]);
    free(fullpath);
    return 1;
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
exportcmd(char **args) {
  int argc, l;
  char *n, *t, *val;
  argc = argcount(args);

  if (argc > 1) {
    t = strchr(args[1], '=');
    if (t == NULL) {
      setenv(args[1], "", 0);
    } else {
      l = t - args[1];
      n = strndup(args[1], l);
      val = strdup(t + 1);
      setenv(n, val, 1);
      free(val);
      free(n);
    }
  }

  return 0;
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
  int i, n = builtinnum();

  printf("These are the builtin commands included with simpsh:\n\n");
  for (i = 0; i < n; i++) {
    printf("%s \n", helparray[i]);
  }
  return 0;
}

int
pwdcmd(char **args) {
  (void)args;
  char *pwdbuf = NULL;
  char *pwd;
  pwd = getcwd(pwdbuf, 0);
  if (pwd) {
    printf("%s\n", pwd);
    free(pwd);
    return 0;
  }
  return 1;
}
