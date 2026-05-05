/* builtins.c - builtin shell commands */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <err.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "main.h"
#include "utils.h"
#include "env.h"
#include "builtins.h"
#include "arg.h"

#define FLAG_L ( 1 << 0)
#define FLAG_P ( 1 << 1)
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

  /* TODO: add in cdpath functionality or convert my other path search functions
   * to be generic to use with this, or for running commands */

char *
pwdpath(char *path) {
  char *res = path, *src = path;

  while (*src) {
    switch (*src) {
      case '/':
        if (res != path && *(res - 1) == '/')
          src++;
        else if (*(src + 1) == '\0')
          src++;
        else
          *res++ = *src++;
        break;
      case '.':
        if (*(src + 1) && (*(src + 2) == '/' || *(src + 2) == '\0'))
          if (res > path + 1) {
            if (res > path + 1 && *(res - 1) == '/')
              res--;
            while (res > path && *(res - 1) != '/')
              res--;
            if (res > path + 1)
              res--;
            src += 2;
          } else {
            src += 2;
          }
        else if (*(src + 1) == '/')
          src += 2;
        else if (*(src + 1) == '\0')
          src++;
        else
          *res++ = *src++;
        break;
      default:
        *res++ = *src++;
        break;
    }
  }

  *res = '\0';
  return path;
}

int
cdcmd(char **argv)
{
  int argc = array_len(argv);
  char *dest, *end, *pwdval;
  char fflag = '\0', respath[PATH_MAX];
  shvar *pwd, *oldpwd;

  ARGBEGIN
  {
    case 'L':
      fflag = 'L';
      break;
    case 'P':
      fflag = 'P';
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
  if (pwd)
    pwdval = shvar_val(pwd);
  else
    pwdval = getcwd(respath, PATH_MAX);

  if (fflag == 'P') {
    if (!dir) {
      dest = home;
    } else if (*dir == '-' && dir[1] == '\0') {
      if (!oldpwd) {
        fprintf(stderr, "OLDPWD not set");
        return 1;
      } else {
        dest = shvar_val(oldpwd);
        printf("%s\n", dest);
      }
    } else {
      dest = (char *)dir;
    }
    if (!realpath(dest, respath)) {
      warn("cd: %s", dest);
      return 1;
    }
    if (chdir(respath) < 0) {
      warn("cd: %s", dest);
      return 1;
    }
    getcwd(respath, PATH_MAX);
  } else {
    size_t plen, dlen;
    if (!dir) {
      dest = home;
    } else if (*dir == '-' && dir[1] == '\0') {
      if (!oldpwd) {
        fprintf(stderr, "OLDPWD not set");
        return 1;
      } else {
        dest = shvar_val(oldpwd);
        printf("%s\n", dest);
      }
    } else {
      dest = (char *)dir;
    }

    if (chdir(dest) < 0) {
      warn("cd: %s", dest);
      return 1;
    }
    dlen = strlen(dest);
    if (dest != NULL && dest[0] == '/') {
      memcpy(respath, dest, dlen);
      respath[dlen] = '\0';
    } else {
      plen = strlen(pwdval);
      end = s_mempcpy(respath, pwdval, plen);
      *end++ = '/';
      end = s_mempcpy(end, dest, dlen);
      *end = '\0';
    }
    if (!pwdpath(respath)) {
      fprintf(stderr, "cd: path normalization failure\n");
      return 1;
    }
  }
  setvar("OLDPWD", pwdval, VEXPRT);
  setvar("PWD", respath, VEXPRT);
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
  char **env = build_env(NULL);
  if (!env) {
    warn("exec: failed to get environ");
  }

  if (!args[1])
    goto fail;
  if (!fullpath)
    goto fail;
  if (execve(fullpath, &args[1], env) < 0)
    goto fail;
  return 0;

fail:
  perror(args[0]);
  if (env) {
    free_env(env);
  }
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

  /* nvar = s_strndup(args[i], nvarlen + 1); */ 
  if (argc > 1) {
    for (i = 1; i < argc; i++) {
      char *eq;
      char *name, *val;
      shvar *v;

      eq = s_strchrnul(args[i], '=');
      if (*eq == '\0') {
        v = find_var(args[i]);
        if (v)
          v->flags |= VEXPRT;
        else
          setvar(args[i], NULL, VEXPRT);
      } else {
        read_assn(args[i], &name, &val);
        setvar(name, val, VEXPRT);
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
pwdcmd(char **argv)
{
  int argc = array_len(argv);
  char fflag = '\0';
  char pwdbuf[PATH_MAX + 1];

  ARGBEGIN
  {
    case 'L':
      fflag = 'L';
      break;
    case 'P':
      fflag = 'P';
      break;
    default:
      fprintf(stderr, "%s: bad option -%c\n", argv0, ARGC());
      return 1;
  }
  ARGEND;

  if (fflag != 'P') {
    char *pwd = getvar("PWD");
    struct stat sbuf, cwdsbuf;

    if (!pwd) {
      goto physical;
    }
    if (stat(pwd, &sbuf) < 0) {
      goto physical;
    }
    if (stat(".", &cwdsbuf) < 0) {
      warn("pwd");
      return 1;
    }
    if ((sbuf.st_ino != cwdsbuf.st_ino || sbuf.st_dev != cwdsbuf.st_dev))
      goto physical;

    printf("%s\n", pwd);
    return 0;
  }

physical:
  if (getcwd(pwdbuf, PATH_MAX + 1)) {
    printf("%s\n", pwdbuf);
    return 0;
  } else {
    warn("pwd");
    return 1;
  }
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
