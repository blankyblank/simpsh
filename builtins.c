/* builtins.c - builtin shell commands */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "arg.h"
#include "builtins.h"
#include "env.h"
#include "exec.h"
#include "main.h"
#include "simpsh.h"
#include "utils.h"

#define bad_opt(a,b,c) (fprintf(stderr, "%s: %s: bad option %c\n", a,b,c))

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
static int unsetcmd(char **);

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
  "unset",
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
  &unsetcmd,
};
int builtinnum(void) {
  return sizeof(builtins) / sizeof(char *);
} /* clang-format on */

static int
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

/**  normalize path to set PWD variable with logical path  */
static char *
pwdpath(char *path) {
  char *res = path, *src = path;

  /*
   * INFO:
   *     we collaps any // to a single /, for .. when encountered
   *     we move back a path segment (between slashes /here/)
   *     for . we collapse it like the // case. we get rid of any 
   *     trailing / also we get rid of any . in the beginning
   *     like ./dir 
   *     we are modifying the string in place using res as the result
   *     buffer chars are getting copied to (and overwriting things we
   *     want to get rid of) and src is the pointer we copy from. it moves
   *     up when we need to skip something while res stays in place, or res
   *     moves back when we get rid of a segment.
   */
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

/* TODO: add in cdpath functionality or convert my other path search functions
 * to be generic to use with this, or for running commands */

static int
cdcmd(char **argv)
{
  unsigned int argc, prnt;
  char *dest, *end, *pwdval;
  char fflag = '\0', respath[PATH_MAX];
  const char *dir;
  shvar *pwd, *oldpwd, *cdpth;

  prnt = 0;
  argc = array_len(argv);
  ARGBEGIN
  {
    case 'L':
      fflag = FLAG_L;
      break;
    case 'P':
      fflag = FLAG_P;
      break;
    default:
      bad_opt(sh_argv0, argv0, ARGC());
      return 1;
  }
  ARGEND;
  if (argc > 1) {
    fprintf(stderr, "cd: Too many arguements");
    return 1;
  }

  oldpwd = find_var("OLDPWD");
  pwd = find_var("PWD");
  if (pwd)
    pwdval = shvar_val(pwd);
  else
    pwdval = getcwd(respath, PATH_MAX);
  cdpth = find_var("CDPATH");
  if (cdpth) {
    if (*argv && argv[0][0] != '/' &&
        !(argv[0][0] == '-' && argv[0][1] == '\0') &&
        !(argv[0][0] == '.' && argv[0][1] == '\0') &&
        !(argv[0][0] == '.' && argv[0][1] == '/') &&
        !(argv[0][0] == '.' && argv[0][1] == '.')) {
      if ((dir = chkpath(shvar_val(cdpth), *argv, 1))) {
        if (!(dir[0] == '.' && dir[1] == '/'))
          prnt = 1;
      } else {
        dir = *argv;
      }
    } else {
      dir = *argv;
    }
  } else {
    dir = *argv;
  }

  if (!dir) {
    dest = home;
  } else if (*dir == '-' && dir[1] == '\0') {
    if (!oldpwd) {
      fprintf(stderr, "OLDPWD not set");
      return 1;
    } else {
      dest = shvar_val(oldpwd);
      prnt = 1;
    }
  } else {
    dest = (char *)dir;
  }

  if (fflag == FLAG_P) {
    if (!realpath(dest, respath)) {
      warn("cd: %s", dest);
      return 1;
    }
    if (chdir(respath) < 0) {
      warn("cd: %s", dest);
      return 1;
    } else {
      if (prnt) {
        printf("%s\n", respath);
      }
    }
    getcwd(respath, PATH_MAX);
  } else {
    size_t plen, dlen;
    if (chdir(dest) < 0) {
      warn("cd: %s", dest);
      return 1;
    } else {
      if (prnt == 1) {
        printf("%s\n", dest);
      }
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
  // NOTE: old fflag check else end
  setvar("OLDPWD", pwdval, VEXPRT);
  setvar("PWD", respath, VEXPRT);
  return 0;
}


static int
echocmd(char *argv[])
{
  int nf = 0;
  size_t argc;

  argc = array_len(argv);
  ARGBEGIN
  {
    case 'n':
      nf = FLAG_N;
      break;
    default:
      bad_opt(sh_argv0, argv0, ARGC());
      return 1;
  }
  ARGEND;

  for (size_t i = 0; i < argc; i++) {
    if (fputs(argv[i], stdout) == EOF) {
      fprintf(stderr, "could not write to stdout\n");
      return 1;
    }
    if (i < argc - 1) {
      if (fputc(' ', stdout) == EOF) {
        fprintf(stderr, "could not write to stdout\n");
        return 1;
      }
    }
  }
  if (!(nf & FLAG_N))
    fputc('\n', stdout);
  return 0;
}

static int
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
    free(env);
  }
  free(fullpath);
  return 1;
}

static int
exitcmd(char **argv)
{
  size_t argc;
  int exnum;
  char *end;
  long n;

  exnum = 0;
  argc = array_len(argv);
  if (argc > 2) {
    fprintf(stderr, "%s: %s: too many arguements\n", sh_argv0 ,argv[0]);
    return 1;
  } else if (argc == 2) {
    for (size_t i = 0; argv[1][i]; i++) {
      if (!isdigit(argv[1][i])) {
        fprintf(stderr, "%s: %s: a number is required\n", sh_argv0, argv[1]);
        return 1;
      }
    }
    n = strtol(argv[1], &end, 10);
    if (*end != '\0' || errno == ERANGE) {
      fprintf(stderr, "%s: exit: %s: out of range\n", sh_argv0,  argv[1]);
      return 1;
    }
    exnum = (int)n;
  }
  exit(exnum);
}

static int
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
        free(name);
        free(val);
      }
    }
  }

  return 0;
}

static int
falsecmd(char **args)
{
  (void)args;
  return 1;
}

static int
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

static int
pwdcmd(char **argv)
{
  int argc = 0;
  (void)argc;
  char fflag = '\0';
  char pwdbuf[PATH_MAX + 1];

  ARGBEGIN
  {
    case 'L':
      fflag = FLAG_L;
      break;
    case 'P':
      fflag = FLAG_P;
      break;
    default:
      bad_opt(sh_argv0, argv0, ARGC());
      return 1;
  }
  ARGEND;

  if (fflag != FLAG_P) {
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

static int
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

static int
unsetcmd(char **argv)
{
  shvar *v;
  size_t argc, c;
  unsigned int err;
  argc = array_len(argv);

  ARGBEGIN
  {
    case 'v':
    break;

    default:
      bad_opt(sh_argv0, argv0, ARGC());
    return 1;
  } ARGEND

  err = 0;
  c = 0;
  if (argc < 1) {
    return 0;
  } else {
    for (; c < argc; c++) {
      v = find_var(argv[c]);
      if (v) {
        if (v->flags & VREADONLY) {
          fprintf(stderr, "%s: unset: %s: cannot unset: readonly variable\n",
                  sh_argv0, argv[c]);
          err = 1;
        } else {
          unset_var(argv[c]);
        }
      }
    }
    if (err)
      return 1;

  }

  return 0;
}
