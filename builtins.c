/* builtins.c - builtin shell commands */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/uio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "alloc.h"
#include "arg.h"
#include "builtins.h"
#include "env.h"
#include "error.h"
#include "input.h"
#include "job.h"
#include "main.h"
#include "opts.h"
#include "path.h"
#include "simd.h"
#include "simpsh.h"
#include "test.h"
#include "utils.h"
#include "var.h"
#include "help.h"

/* builtins */
static int breakcmd(char **);
static int cdcmd(char **);
static int continuecmd(char **);
static int dotcmd(char **);
static int echocmd(char **);
static int execcmd(char **);
static int exitcmd(char **);
static int falsecmd(char **);
static int pwdcmd(char **);
static int readcmd(char **);
static int returncmd(char **);
static int truecmd(char **);
static int umaskcmd(char **);

static char *pwdpath(char *);

/* the array of builtin commands */ /* clang-format-off */
const builtin builtins[] = {
  { ".",        &dotcmd      },
  { "[",        &testcmd     },
  { ":",        &truecmd     },
  { "alias",    &aliascmd    },
  { "bg",       &bgcmd       },
  { "break",    &breakcmd    },
  { "cd",       &cdcmd       },
  { "continue", &continuecmd },
  { "echo",     &echocmd     },
  { "exec",     &execcmd     },
  { "exit",     &exitcmd     },
  { "export",   &exportcmd   },
  { "false",    &falsecmd    },
  { "fg",       &fgcmd       },
  { "hash",     &hashcmd     },
  { "help",     &helpcmd     },
  { "jobs",     &jobscmd     },
  { "local",    &localcmd    },
  { "pwd",      &pwdcmd      },
  { "read",     &readcmd     },
  { "readonly", &readonlycmd },
  { "return",   &returncmd   },
  { "set",      &setcmd      },
  { "test",     &testcmd     },
  { "true",     &truecmd     },
  { "umask",    &umaskcmd    },
  { "unalias",  &unaliascmd  },
  { "unset",    &unsetcmd    },
};

#define nbuiltins() (sizeof(builtins) / sizeof(builtin))
/* int
nbuiltins(void)
{
  return sizeof(builtins) / sizeof(builtin);
} */
/* clang-format on */

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
    idx = hash(builtins[i].name, BUILTIN_BUCKETS);
    while (builtin_tab[idx] >= 0)
      idx = (idx + 1) % BUILTIN_BUCKETS;
    builtin_tab[idx] = i;
  }
}

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

/**  normalize path to set PWD variable with logical path  */
static char *
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

static int
breakcmd(char **argv)
{
  size_t argc = 0;
  int n;

  if (!loopdepth) {
    shwarnx(argv[0], "not in a loop");
    return 1;
  }
  array_len(argv, argc);
  if (argc < 2) {
    n = 1;
  } else if (argc == 2) {
    char *breakn = argv[1];
    for (size_t i = 0; breakn[i]; i++) {
      if (!isdigit_(breakn[i])) {
        shwarn_arg(argv[0], argv[1], "a numeric arguement is required");
        return 1;
      }
    }
    if ((n = atoi_(argv[1])) <= 0) {
      shwarn_arg(argv[0], breakn, "must be a positive integer");
      return 1;
    }
  } else {
    shwarn_arg(argv[0], argv[2], "too many arguements");
    return 1;
  }

  if (n > loopdepth)
    n = loopdepth;
  loopbreak = n;
  return 0;
}

static int
cdcmd(char **argv)
{
  unsigned int prnt, argc = 0;
  char *bargv0, *dest, *end, *pwdval;
  char flag = '\0', respath[PATH_MAX];
  const char *dir;
  shvar *pwd, *oldpwd, *cdpth;
  size_t destlen = 0;

  prnt = 0;
  array_len(argv, argc);
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
      bad_opt(argv0, ARGC());
      return 1;
  }
  ARGEND;
  if (argc > 1) {
    shwarnx(bargv0, "Too many arguements"); /*NOLINT*/
    return 1;
  }

  oldpwd = findvar(oldpwdn);
  pwd = findvar(pwdn);
  if (pwd)
    pwdval = shvar_val(pwd);
  else
    pwdval = getcwd(respath, PATH_MAX);
  cdpth = findvar(cdpthn);
  if (cdpth) {
    if (*argv && argv[0][0] != '/' &&
        !(argv[0][0] == '-' && argv[0][1] == '\0') &&
        !(argv[0][0] == '.' && argv[0][1] == '\0') &&
        !(argv[0][0] == '.' && argv[0][1] == '/') &&
        !(argv[0][0] == '.' && argv[0][1] == '.')) {
      if ((dir = chkpath(shvar_val(cdpth), *argv, X_OK, 1))) {
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
    destlen = homelen;
  } else if (*dir == '-' && dir[1] == '\0') {
    if (!oldpwd) {
      shwarnx(bargv0, "OLDPWD not set"); /*NOLINT*/
      return 1;
    } else {
      dest = shvar_val(oldpwd);
      destlen = oldpwd->flen;
      prnt = 1;
    }
  } else {
    dest = (char *)dir;
    destlen = strlen(dest);
  }

  if (flag == FLAG_P) {
    if (destlen >= PATH_MAX)
      nts(respath, destlen - 1);
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
  setvar(oldpwdn, pwdval, VEXPRT);
  setvar(pwdn, respath, VEXPRT);
  return 0;
}

static int
continuecmd(char **argv)
{
  int n;
  size_t argc = 0;

  if (!loopdepth) {
    shwarnx(argv[0], "not in a loop");
    return 1;
  }
  array_len(argv, argc);
  if (argc < 2) {
    n = 1;
  } else if (argc == 2) {
    char *contn = argv[1];
    for (size_t i = 0; contn[i]; i++) {
      if (!isdigit_(contn[i])) {
        shwarn_arg(argv[0], argv[1], "a numeric arguement is required");
        return 1;
      }
    }
    if ((n = atoi_(argv[1])) <= 0) {
      shwarn_arg(argv[0], contn, "must be a positive integer");
      return 1;
    }
  } else {
    shwarn_arg(argv[0], argv[2], "too many arguements");
    return 1;
  }

  if (n > loopdepth)
    n = loopdepth;
  loopcontinue = n;
  return 0;
}


static int
dotcmd(char **argv)
{
  size_t argc = 0;
  char *o_argv0 = NULL, **o_argv = NULL;
  char *file;
  int o_argc = 0, st = 0, fd;

  array_len(argv, argc);

  if (argc < 2) {
    shwarn(argv[0], "filename arguement require");
    return 1;
  }

  if (strchr(argv[1], '/')) {
    if (access(argv[1], R_OK) < 0) {
      st = 1;
      goto restore;
    }
    file = argv[1];
  } else {
    char *fpath, *path;
    if ((path = getvar(pathn)))
      fpath = chkpath(path, argv[1], R_OK, 0);
    else
      fpath = chkpath(defpath, argv[1], R_OK, 0);
    if (fpath) {
      file = fpath;
    } else {
      if (access(argv[1], R_OK) < 0) {
        st = 1;
        goto restore;
      }
      file = argv[1];
    }
  }
  if (!file) {
    st = 1;
    goto restore;
  }
  if ((fd = open(file, O_RDONLY)) < 0) {
    st = 1;
    goto restore;
  }
  setinputf(fd, 0);

  if (argc == 2) {
    o_argv0 = strdup_(sh_argv0);
    sh_argv0 = strdup_(file);
  } else {
    o_argc = sh_argc;
    sh_argc = argc - 2;
    o_argv0 = strdup_(sh_argv0);
    sh_argv0 = strdup_(file);

    o_argv = slalloc(sizeof(char *) * (o_argc + 1));
    for (int i = 0; i < o_argc; i++)
      o_argv[i] = strdup_(sh_argv[i]);
    o_argv[o_argc] = NULL;
    if (alloc_sh_argv && sh_argv) {
      for (int i = 0; i < o_argc; i++)
        slfree(sh_argv[i]);
      slfree(sh_argv);
    }
    sh_argv = slalloc(sizeof(char *) * (argc + 1));
    size_t j = 0;
    for (size_t i = 2; argv[i]; i++)
      sh_argv[j++] = strdup_(argv[i]);
    sh_argv[sh_argc] = NULL;
    alloc_sh_argv = 1;
  }

  simpsh_run();
  popinput();

restore:
  if (o_argv) {
    for (size_t i = 0; sh_argv[i]; i++)
      slfree(sh_argv[i]);
    slfree(sh_argv);
    sh_argv = o_argv;
  }
  if (o_argv0) {
    slfree(sh_argv0);
    sh_argv0 = o_argv0;
  }
  if (o_argc)
    sh_argc = o_argc;
  if (st)
    perror(argv[1]);
  return st ? st : lstatus;
}

static int
echocmd(char *argv[])
{
  int nf = 0;
  size_t argc = 0;
  char *bargv0;

  bargv0 = argv[0];
  array_len(argv, argc);
  ARGBEGIN
  {
    case 'n':
      nf = FLAG_N;
      break;
    default:
      bad_opt(argv0, ARGC());
      return 1;
  }
  ARGEND;

  if (fcntl(STDOUT_FILENO, F_GETFD) < 0) {
    sherr(1, bargv0, "could not write to stdout");
  }
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
    if (fputc('\n', stdout) == EOF) {
      warn("%s: %s", bargv0, "could not write to stdout");
      return 1;
    }
  return 0;
}

static int
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
    slfree(env);
  }
  slfree(fullpath);
  return 1;
}

static int
exitcmd(char **argv)
{
  size_t argc = 0;
  int exnum;

  exnum = 0;
  array_len(argv, argc);
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
  slclear();
  exit(exnum);
}

static int
falsecmd(char **args)
{
  (void)args;
  return 1;
}

static int
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
      bad_opt(argv0, ARGC());
      return 1;
  }
  ARGEND;

  if (flag != FLAG_P) {
    char *pwd = getvar(pwdn);
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

#define rfl 1 << 0
#define pfl 1 << 1

static int
readcmd(char **argv)
{
  size_t argc = 0;
  int flag = 0;
  char *prompt = NULL;

  array_len(argv, argc);
  ARGBEGIN
  {
    case 'r':
      flag |= rfl;
      break;
    case 'p':
      flag |= pfl;
      prompt = EARGF(no_opt(argv0, ARGC()));
      break;
    default:
      bad_opt(argv0, ARGC());
      return 1;
  }
  ARGEND;
  if (!argc) {
    shwarn_arg(argv0, "1", "requires variable name");
    return 1;
  }

  stmark rmark;
  int c, status = 0;
  size_t len = 0, ifslen, cleft, nws = 0;
  char *ifs = NULL;
  char ifsws[4],  *line, *p;

  setinputf(STDIN_FILENO, 1);
  rmark = stack_mark();

  if ((flag & pfl)) {
    fputs(prompt, stderr);
    fflush(stderr);
  }
  while ((c = shgetchar())) {
    switch (c) {
      case SHEOF:
        status = 1;
        goto rend;
      case '\0':
        continue;
      case '\\':
        if ((c = shgetchar()) == SHEOF) {
          status = 1;
          goto rend;
        }
        if (flag & rfl) {
          st_putc('\\'), st_putc(c), len += 2;
        } else if (c == '\n') {
          continue;
        } else {
          st_putc(c), len++;
          continue;
        }
      case '\n':
        goto rend;
      default:
        st_putc(c), len++;
    }
  }
rend:
  popinput();
  if (status) {
    stack_restore(rmark);
    return 1;
  }


  line = grab_str(len);
  ifs = getvar("IFS");
  if (!ifs) {
    ifs = " \t\n";
    ifsws[0] = ' ';
    ifsws[1] = '\t';
    ifsws[2] = '\n';
    ifslen = nws = 3;
  } else {
    ifslen = strlen(ifs);
    if (memchr_(ifs, ifslen, ' ') < ifslen)
      ifsws[nws++] = ' ';
    if (memchr_(ifs, ifslen, '\t') < ifslen)
      ifsws[nws++] = '\t';
    if (memchr_(ifs, ifslen, '\n') < ifslen)
      ifsws[nws++] = '\n';
  }

  p = line;
  cleft = len;
  for (size_t i = 0; argv[i]; i++) {
    size_t skip;

    skip = sskipdelims(p, cleft, ifsws, nws);
    p += skip, cleft -= skip;

    if (!cleft) {
      for (size_t j = i; argv[j]; j++)
        setvar(argv[j], "", 0);
      break;
    }
    if (!argv[i + 1]) {
      setvar(argv[i], st_strndup(p, cleft), 0);
      break;
    }
    skip = sscndelim(p, cleft, ifs, ifslen);
    setvar(argv[i], st_strndup(p, skip), 0);
    p += skip, cleft -= skip;

    if (cleft > 0) {
      int isws = 0;
      for (size_t j = 0; j < nws; j++)
        if (*p == ifsws[j]) {
          isws = 1;
          break;
        }
      if (isws) {
        skip = sskipdelims(p, cleft, ifsws, nws);
        p += skip, cleft -= skip;
      } else {
        p++, cleft--;
      }
    }
  }

  stack_restore(rmark);
  return status;
}

static int
returncmd(char **argv)
{
  size_t argc = 0;
  int status = 0;
  array_len(argv, argc);

  if (argc > 2) {
    shwarnx(argv[0], "too many arguements");
    return 1;
  }

  if (argc == 2) {
    for (size_t i = 0; argv[1][i]; i++) {
      if (!isdigit_(argv[1][i])) {
        shwarn_arg(argv[0], argv[1], "a numeric arguement is required");
        return 1;
      }
    }
    status = atoi_(argv[1]);
    // status = (status < 256) ? status : lstatus;
  }
  retval = status, retnow = 1;
  return status;
}

static int
truecmd(char **args)
{
  (void)args;
  return 0;
}

static int
umaskcmd(char **argv)
{
  int symb = 0;
  size_t argc = 0;

  array_len(argv, argc);
  ARGBEGIN
  {
    case 'S':
      symb = 1;
      break;
    default:
      bad_opt(argv0, ARGC());
      return 1;
  }
  ARGEND

  mode_t mask = umask(0);
  int usrp, grpp, othp;
  const char ugo[] = { 'u', 'g', 'o', '\0' };

  umask(mask);
  if (!argv[0]) {
    if (!symb) {
      printf("%.4o\n", mask);
      return 0;
    } else {
      mask = (~mask) & 0777;
      usrp = (mask >> 6) & 07;
      grpp = (mask >> 3) & 07;
      othp = mask & 07;

      int val[] = { usrp, grpp, othp };

      for (int i = 0; i <= 2; i++) {
        putc(ugo[i], stdout);
        putc('=', stdout);
        if (val[i] & 4)
          putc('r', stdout);
        if (val[i] & 2)
          putc('w', stdout);
        if (val[i] & 1)
          putc('x', stdout);
        if (i < 2)
          putc(',', stdout);
      }
      putc('\n', stdout);
      return 0;
    }
  } else {
    if (!symb) {
      mode_t val = 0;

      for (int i = 0; argv[0][i]; i++) {
        int c = argv[0][i];
        if (c < '0' || c > '7') {
          shwarn_arg(argv0, argv[0], "octal number out of range");
          return 1;
        }
        val = (val << 3) | (c - '0');
      }
      umask(val);
      return 0;
    } else {
      mask = (~mask) & 0777;

      for (char *c = argv[0]; *c; c++) {
        mode_t pos = 0, perms = 0, nm = 0;
        char op = '\0';

        while (*c == 'u' || *c == 'g' || *c == 'o' || *c == 'a') {
          if (*c == 'u')
            pos |= 1 << 6;
          else if (*c == 'g')
            pos |= 1 << 3;
          else if (*c == 'o')
            pos |= 1 << 0;
          else if (*c == 'a')
            pos |= 0111;
          c++;
        }

        op = *c;
        if (*c != '=' && *c != '+' && *c != '-') {
          fprintf(stderr, "%s: %s: %c:  not a valid operator \n", sh_argv0, argv0, *c);
          return 1;
        }
        if (!pos)
          pos = 0111;
        c++;


        while (*c == 'r' || *c == 'w' || *c == 'x' || *c == 'u' || *c == 'g' ||
               *c == 'o' || *c == 'X' || *c == 's') {
          switch (*c) {
            case 'r':
              perms |= 04, c++;
              continue;
            case 'w':
              perms |= 02, c++;
              continue;
            case 'X':
            case 'x':
              perms |= 01, c++;
              continue;
            case 'u':
              perms |= (mask >> 6) & 07, c++;
              continue;
            case 'g':
              perms |= (mask >> 3) & 07, c++;
              continue;
            case 'o':
              perms |= mask & 07, c++;
              continue;
            case 's':
              c++;
              continue;
            default:
              break;
          }
        }

        nm = perms * pos;

        if (op == '=') {
          mask = nm | (mask & ~(pos * 07));
        } else if (op == '+') {
          mask |= nm;
        } else if (op == '-') {
          mask &= ~nm;
        }

        if (*c == ',') {
          continue;
        } else if (*c == '\0') {
          break;
        }
      }
      umask((~mask) & 0777);
      return 0;
    }
  }
}

