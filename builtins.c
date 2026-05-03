/* builtins.c - builtin shell commands */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <err.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include "main.h"
#include "utils.h"
#include "env.h"
#include "builtins.h"
#include "arg.h"

static int echo_print(int, int, char **);

/* builtins */
static int aliascmd(char **);
static int cdcmd(char **);
static int echocmd(char **);
static int execcmd(char **);
static int exitcmd(char **);
static int exportcmd(char **);
static int falsecmd(char **);
static int helpcmd(char **);
static int pwdcmd(char **);
static int truecmd(char **);
static int unaliascmd(char **);

/* the array of builtin commands */ /* clang-format off */
const char *builtins[] = {
  "alias",
  "cd",
  "echo",
  "exec",
  "exit",
  "export",
  "false",
  "help",
  "pwd",
  "true",
  "unalias",
};
int (* const builtin_funcs[])(char **) = {
  &aliascmd,
  &cdcmd,
  &echocmd,
  &execcmd,
  &exitcmd,
  &exportcmd,
  &falsecmd,
  &helpcmd,
  &pwdcmd,
  &truecmd,
  &unaliascmd,
};
int builtinnum(void) {
  return sizeof(builtins) / sizeof(char *);
} /* clang-format on */

int
aliascmd(char **args)
{
  int i;
  alias *e;
  char *delem, *n, *v;

  if (!args[1]) {
    for (i = 0; i < ENV_BUCKETS; i++) {
      if (alias_tab[i]) {
        e = alias_tab[i];
        while (e) {
          printf("alias %s=%s\n", e->name, e->value);
          e = e->next;
        }
      }
    }
    return 0;
  }

  if (!(delem = strchr(args[1], '='))) {
    if (!(e = find_alias(args[1])))
      return 1;
    else
      printf("alias %s=%s\n", e->name, e->value);
  } else {
    n = s_strndup(args[1], strlen(args[1]) - strlen(delem));
    v = s_strdup(delem + 1);
    set_alias(n, v);
    free(n);
    free(v);
  }

  return 0;
}

#define FLAG_L ( 1 << 0)
#define FLAG_P ( 1 << 1)

  /* TODO: add in cdpath functionality or convert my other path search functions
   * to be generic to use with this, or for running commands */

int
cdcmd(char **argv)
{
  int argc = array_len(argv);
  char *dest;
  char lflag = '\0', respath[PATH_MAX];
  shvar *pwd, *oldpwd;
  shvar_flags flags = EXPRT;

  ARGBEGIN
  {
    case 'L':
      lflag = 'L';
      break;
    case 'P':
      lflag = 'P';
      break;
    default:
      fprintf(stderr, "%s: bad option -%c\n", argv0, ARGC());
      return 1;
  }
  ARGEND;
  if (argc > 1) {
    fprintf(stderr, "cd: Too many arguements");
    return 1;
  }
 const char *dir = *argv;

  oldpwd = find_var("OLDPWD");
  pwd = find_var("PWD");

  if (!dir) {
    dest = home;
  } else if (*dir == '-' && dir[1] == '\0') {
    if (!oldpwd) {
      fprintf(stderr, "OLDPWD not set");
      return 1;
    } else {
      dest = shvar_val(oldpwd);
    }
  } else {
    dest = (char *)dir;
  }

  if (lflag == 'P') {
    if (!realpath(dest, respath)) {
      warn("cd: %s", dest);
      return 1;
    }
    if (chdir(respath) < 0) {
      warn("cd: %s", dest);
      return 1;
    }
    getcwd(respath, PATH_MAX);
    setvar("PWD", respath, flags);
  } else {
    if (chdir(dest) < 0) {
      warn("cd: %s", dest);
      return 1;
    }
  }

  setvar("OLDPWD", shvar_val(pwd), flags);
  putenv(oldpwd->var);
  putenv(pwd->var);
  return 0;
}

int
echocmd(char *args[])
{
  int n;
  int stat, argc;

  argc = array_len(args);

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
echo_print(int argc, int n, char *args[])
{
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
execcmd(char **args)
{
  char *fullpath;

  fullpath = getpath(args[1]);

  if (!args[1])
    goto fail;
  if (!fullpath)
    goto fail;
  if (execve(fullpath, &args[1], environ) < 0)
    goto fail;
  return 0;

fail:
  perror(args[0]);
  free(fullpath);
  return 1;
}

int
exitcmd(char **argv)
{
  char **args = (char **)NULL;
  (void)argv;
  if (args) {
    freeptr(args);
  }
  exit(0);
}

int
exportcmd(char **args)
{
  int argc, i;
  argc = array_len(args);

  // nvar = s_strndup(args[i], nvarlen + 1);
  if (argc > 1) {
    for (i = 1; i < argc; i++) {
      char *eq;
      char *name, *val;
      shvar *v;
      shvar_flags flags = EXPRT;

      eq = s_strchrnul(args[i], '=');
      if (*eq == '\0') {
        v = find_var(args[i]);
        if (v) {
          v->flags.exported = 1;
          putenv(v->var);
        } else {
          setvar(args[i], NULL, flags);
          setenv(args[i], "", 1);
        }
      } else {
        read_assn(args[i], &name, &val);
        setvar(name, val, flags);
        putenv(args[i]);
      }
    }
  }

  return 0;
}

int
falsecmd(char **args)
{
  (void)args;
  return 1;
}

int
helpcmd(char **args)
{
  (void)args;
  const char **helparray = builtins;
  int i, n = builtinnum();

  printf("simpsh version idk.2 (still pre alpha) - "
         "https://codeberg.org/someoneelse/simpsh.git\n\n"
         "These are the builtin commands included with simpsh:\n");
  for (i = 0; i < n; i++) {
    printf("%s \n", helparray[i]);
  }
  printf("\n");
  return 0;
}

int
pwdcmd(char **args)
{
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

int
truecmd(char **args)
{
  (void)args;
  return 0;
}

static int
unaliascmd(char **args)
{
  alias *e;
  /* int i;
   char *n, *v; */

  e = find_alias(args[1]);
  if (e) {
    rm_alias(args[1]);
  } else {
    fprintf(stderr, "unalias: %s: alias not found\n", args[1]);
    return 1;
  }

  return 0;
}
