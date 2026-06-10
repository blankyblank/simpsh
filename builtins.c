/* builtins.c - builtin shell commands */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <err.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "arg.h"
#include "builtins.h"
#include "path.h"
#include "job.h"
#include "env.h"
#include "main.h"
#include "opts.h"
#include "test.h"
#include "utils.h"
#include "var.h"

static char *pwdpath(char *);

/* the array of builtin commands */ /* clang-format off */
const char *builtins[] = {
  "[",
  ":",
  "alias",
  "bg",
  "cd",
  "echo",
  "exec",
  "exit",
  "export",
  "false",
  "fg",
  "hash",
  "help",
  "jobs",
  "local",
  "pwd",
  "readonly",
  "set",
  "test",
  "true",
  "unalias",
  "unset",
};
int (* const builtin_funcs[])(char **) = {
  &testcmd,
  &truecmd,
  &aliascmd,
  &bgcmd,
  &cdcmd,
  &echocmd,
  &execcmd,
  &exitcmd,
  &exportcmd,
  &falsecmd,
  &fgcmd,
  &hashcmd,
  &helpcmd,
  &jobscmd,
  &localcmd,
  &pwdcmd,
  &readonlycmd,
  &setcmd,
  &testcmd,
  &truecmd,
  &unaliascmd,
  &unsetcmd,
};
int nbuiltins(void) {
  return sizeof(builtins) / sizeof(char *);
} /* clang-format on */

/**  initialize builtin hash table  */
void
init_builtins(void)
{
  size_t i, n;
  size_t idx;

  n = nbuiltins();
  for (i = 0; i < BUILTIN_BUCKETS; i++)
    builtin_tab[i] = -1;

  for (i = 0; i < n; i++) {
    idx = hash(builtins[i], BUILTIN_BUCKETS);
    while (builtin_tab[idx] >= 0)
      idx = (idx + 1) % BUILTIN_BUCKETS;
    builtin_tab[idx] = i;
  }
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

int
cdcmd(char **argv)
{
  unsigned int argc, prnt;
  char *bargv0, *dest, *end, *pwdval;
  char flag = '\0', respath[PATH_MAX];
  const char *dir;
  shvar *pwd, *oldpwd, *cdpth;

  prnt = 0;
  argc = array_len(argv);
  bargv0 = argv[0];
  ARGBEGIN
  {
    case 'L':
      flag = FLAG_L;
      break;
    case 'P':
      flag = FLAG_P;
      break;
    default:
      bad_opt(sh_argv0, argv0, ARGC());
      return 1;
  }
  ARGEND;
  if (argc > 1) {
    shwarnx(bargv0, "Too many arguements"); /*NOLINT*/
    return 1;
  }

  oldpwd = findvar("OLDPWD");
  pwd = findvar("PWD");
  if (pwd)
    pwdval = shvar_val(pwd);
  else
    pwdval = getcwd(respath, PATH_MAX);
  cdpth = findvar("CDPATH");
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
      shwarnx(bargv0, "OLDPWD not set"); /*NOLINT*/
      return 1;
    } else {
      dest = shvar_val(oldpwd);
      prnt = 1;
    }
  } else {
    dest = (char *)dir;
  }

  if (flag == FLAG_P) {
    if (!realpath(dest, respath)) {
      sherr(1, bargv0, dest);
    }
    if (chdir(respath) < 0) {
      sherr(1, bargv0, dest);
    } else {
      if (prnt) {
        printf("%s\n", respath);
      }
    }
    if (getcwd(respath, PATH_MAX))
      return 1;
  } else {
    size_t plen, dlen;
    if (chdir(dest) < 0) {
      sherr(1, bargv0, dest);
    } else if (prnt == 1) {
      printf("%s\n", dest);
    }
    dlen = strlen(dest);
    if (dest != NULL && dest[0] == '/') {
      memcpy(respath, dest, dlen);
      respath[dlen] = '\0';
    } else {
      if (!pwdval)
        return 1;
      plen = strlen(pwdval);
      end = mempcpy_(respath, pwdval, plen);
      *end++ = '/';
      end = mempcpy_(end, dest, dlen);
      *end = '\0';
    }
    if (!pwdpath(respath)) {
      shwarnx(bargv0, "path normalization failure"); /*NOLINT*/
      return 1;
    }
  }
  setvar("OLDPWD", pwdval, VEXPRT);
  setvar("PWD", respath, VEXPRT);
  return 0;
}


int
echocmd(char *argv[])
{
  int nf = 0;
  size_t argc;
  char *bargv0;

  bargv0 = argv[0];
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
      sherr(1, bargv0, "could not write to stdout");
    }
    if (i < argc - 1)
      if (fputc(' ', stdout) == EOF) {
        sherr(1, bargv0, "could not write to stdout");
    }
  }
  if (!(nf & FLAG_N))
    fputc('\n', stdout);
  if (fflush(stdout) == EOF) {
    sherr(1, bargv0, "could not write to stdout");
  }
  return 0;
}

int
execcmd(char **argv)
{
  if (!argv[1])
    return 0;
  char *fullpath;
  char **env = build_env(NULL);

  if (!env)
    shwarnx(argv[0], "failed to get environ"); /*NOLINT*/

  fullpath = getpath(argv[1]);
  if (!fullpath)
    goto fail;
  if (execve(fullpath, &argv[1], env) < 0)
    goto fail;
  return 0;

fail:
  perror(argv[0]);
  if (env) {
    free(env);
  }
  free(fullpath);
  return 1;
}

int
exitcmd(char **argv)
{
  size_t argc;
  int exnum;

  exnum = 0;
  argc = array_len(argv);
  if (argc > 2) {
    shwarnx(argv[0], "too many arguements"); /*NOLINT*/
    return 1;
  } else if (argc == 2) {
    for (size_t i = 0; argv[1][i]; i++) {
      if (!isdigit_(argv[1][i])) {
        shwarn_arg(argv[0], argv[1], "a number is required");
        return 1;
      }
    }
    exnum = atoi_(argv[1]);
  }
  _exit(exnum);
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
  int i, n = nbuiltins();

  printf("simpsh version (pre alpha) - "
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
  int argc = 0;
  (void)argc;
  char flag = '\0';
  char pwdbuf[PATH_MAX + 1];

  ARGBEGIN
  {
    case 'L':
      flag = FLAG_L;
      break;
    case 'P':
      flag = FLAG_P;
      break;
    default:
      bad_opt(sh_argv0, argv0, ARGC());
      return 1;
  }
  ARGEND;

  if (flag != FLAG_P) {
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

