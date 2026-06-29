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
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "alloc.h"
#include "arg.h"
#include "builtins.h"
#include "env.h"
#include "exec.h"
#include "error.h"
#include "help.h"
#include "input.h"
#include "job.h"
#include "main.h"
#include "opts.h"
#include "path.h"
#include "sig.h"
#include "simd.h"
#include "simpsh.h"
#include "test.h"
#include "utils.h"
#include "var.h"

/* builtins */
static int breakcmd(char **);
static int cdcmd(char **);
static int commandcmd(char **);
static int continuecmd(char **);
static int dotcmd(char **);
static int echocmd(char **);
static int evalcmd(char **);
static int execcmd(char **);
static int exitcmd(char **);
static int falsecmd(char **);
static int pwdcmd(char **);
static int readcmd(char **);
static int returncmd(char **);
static int shiftcmd(char **);
static int timescmd(char **);
static int truecmd(char **);
static int typecmd(char **);
static int umaskcmd(char **);

static int bltin_atoi(char *, char *);
static int classify_cmd(char *, int, int);
static char *pwdpath(char *);

/* the array of builtin commands */
const builtin builtins[] = {
  { ".",        &dotcmd      },
  { "[",        &testcmd     },
  { ":",        &truecmd     },
  { "alias",    &aliascmd    },
  { "bg",       &bgcmd       },
  { "break",    &breakcmd    },
  { "cd",       &cdcmd       },
  { "command",  &commandcmd  },
  { "continue", &continuecmd },
  { "echo",     &echocmd     },
  { "eval",     &evalcmd     },
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
  { "shift",    &shiftcmd    },
  { "test",     &testcmd     },
  { "times",    &timescmd    },
  { "trap",     &trapcmd     },
  { "true",     &truecmd     },
  { "type",     &typecmd     },
  { "umask",    &umaskcmd    },
  { "unalias",  &unaliascmd  },
  { "unset",    &unsetcmd    },
};

static const char *keywd[] = {
  "if",
  "then",
  "else",
  "elif",
  "fi",
  "case",
  "esac",
  "for",
  "while",
  "until",
  "do",
  "done",
  "in",
  NULL,
};

#define nbuiltins() (sizeof(builtins) / sizeof(builtin))

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

static int
bltin_atoi(char *s, char *b)
{
  int n;
  for (intf i = 0; s[i]; i++) {
    if (!isdigit_(s[i])) {
      shwarn_arg(b, s, "a numeric arguement is required");
      return -1;
    }
  }
  if ((n = atoi_(s)) < 0) {
    shwarn_arg(b, s, "must be a positive integer");
    return -1;
  }
  return n;
}

static inline int
iskeywd(const char *str)
{
  for (int i = 0; keywd[i]; i++) {
    if (strcmp(str, keywd[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

int
classify_cmd(char *s, int vrb, int def)
{
  alias *a;
  shfunc *f;
  const builtin *b;
  char *e;

  if (vrb) {
    if ((a = findalias(s))) {
      printf("%s is aliased to '%s'\n", a->name, a->value);
      return 0;
    }
    if (iskeywd(s)) {
      printf("%s is a shell keyword\n", s);
      return 0;
    }
    if ((f = findfunc(s))) {
      printf("%s is a shell function\n", f->name);
      return 0;
    }
    if ((b = findbuiltin(s))) {
      printf("%s is a shell builtin\n", b->name);
      return 0;
    }
    if (!def) {
      if ((e = findchash(s))) {
        printf("%s is hashed (%s)\n", s, e);
        return 0;
      }
    }
    if ((e = chkpath((def) ? defpath : getvar(pathn), s, X_OK, 0))) {
      printf("%s is %s\n", s, e);
      return 0;
    }
    printf("%s: not found\n", s);
    return 1;
  }

  if ((a = findalias(s))) {
    printf("%s \n", a->name);
    return 0;
  }
  if (iskeywd(s)) {
    printf("%s\n", s);
    return 0;
  }
  if ((f = findfunc(s))) {
    printf("%s\n", f->name);
    return 0;
  }
  if ((b = findbuiltin(s))) {
    printf("%s\n", b->name);
    return 0;
  }
  if (!def) {
    if ((e = findchash(s))) {
      printf("%s\n", e);
      return 0;
    }
  }
  if ((e = chkpath((def) ? defpath : getvar(pathn), s, X_OK, 0))) {
    printf("%s\n", e);
    return 0;
  }
  return 1;
}

/**  normalize path to set PWD variable with logical path  */
static char *
pwdpath(char *path) {
  char *res = path, *src = path;

  while (*src) {
    switch (*src) {
      case '/':
        if ((res != path && *(res - 1) == '/') || *(src + 1) == '\0')
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
    if ((n = bltin_atoi(argv[1], argv[0])) <= 0) {
      if (!n)
        shwarn_arg(argv[0], argv[1], "must be a positive integer");
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
  char *bargv0, *dir, *end, *pwdval;
  char flag = '\0', respath[PATH_MAX];
  const char *dest;
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
      if ((dest = chkpath(shvar_val(cdpth), *argv, X_OK, 1))) {
        if (!(dest[0] == '.' && dest[1] == '/'))
          prnt = 1;
      } else {
        dest = *argv;
      }
    } else {
      dest = *argv;
    }
  } else {
    dest = *argv;
  }

  if (!dest) {
    dir = home;
    destlen = homelen;
  } else if (*dest == '-' && dest[1] == '\0') {
    if (!oldpwd) {
      shwarnx(bargv0, "OLDPWD not set"); /*NOLINT*/
      return 1;
    }
    dir = shvar_val(oldpwd);
    destlen = oldpwd->flen;
    prnt = 1;
  } else {
    dir = (char *)dest;
    destlen = strlen(dir);
  }

  if (flag == FLAG_P) {
    if (destlen >= PATH_MAX)
      nts(respath, destlen - 1);
    if (!realpath(dir, respath)) {
      sherr(1, bargv0, dir);
    }
    if (chdir(respath) < 0) {
      sherr(1, bargv0, dir);
    }
    if (prnt) {
      printf("%s\n", respath);
    }
    if (getcwd(respath, PATH_MAX))
      return 1;
  } else {
    size_t plen, dlen;
    if (chdir(dir) < 0) {
      sherr(1, bargv0, dir);
    }
    if (prnt == 1) {
      printf("%s\n", dir);
    }
    dlen = strlen(dir);
    if (dir != NULL && dir[0] == '/') {
      memcpy(respath, dir, dlen);
      respath[dlen] = '\0';
    } else {
      if (!pwdval)
        return 1;
      plen = strlen(pwdval);
      end = mempcpy_(respath, pwdval, plen);
      *end++ = '/';
      end = mempcpy_(end, dir, dlen);
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
commandcmd(char **argv)
{
  int flags = 0, argc = 0;
  int def = 0, status = 0;
  char *path;

  array_len(argv, argc);
  if (argc == 1)
    return 0;
  ARGBEGIN
  {
    case 'v':
      flags = FLAG_v;
      break;
    case 'V':
      flags = FLAG_V;
      break;
    case 'p':
      def = 1;
      break;
    default:
      bad_opt(argv0, ARGC());
      return 1;
  }
  ARGEND

  if (flags & (FLAG_v | FLAG_V)) {
    for (int i = 0; i < argc; i++)
      if (classify_cmd(argv[i], (flags == FLAG_V), def))
        status = 1;
    return status;
  }

  char *jcmd, *fpath, **env;
  const builtin *b;


  if ((b = findbuiltin(argv[0]))) {
    return builtin_launch(b, argv);
  }
  if ((path = (def) ? defpath : getvar(pathn)))
    path = defpath;
  if (!(fpath = chkpath(path, argv[0], X_OK, 0))) {
    shwarnx(argv[0], "command not found");
    return 1;
  }
  jcmd = join_strn(argv, NULL);
  env = build_env(NULL);
  status = forkexec(fpath, argv, env, jcmd, NULL);

  return status;
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
    if ((n = bltin_atoi(argv[1], argv[0])) <= 0) {
      if (!n)
        shwarn_arg(argv[0], argv[1], "must be a positive integer");
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

  eval_run();
  retnow = 0;
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

int
evalcmd(char **argv)
{
  size_t tlen = 0;
  char *cmdstrn;
  int status;
 
  if (!argv[1])
    return 0;

  for (size_t i = 1; argv[i]; i++)
    tlen += strlen(argv[i]);
  cmdstrn = join_strn(argv + 1, &tlen);

  setinputstrn(cmdstrn, tlen);
  status = eval_run();
  popinput();
  return status;
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
  }

  if (argc == 2)
    if ((exnum = bltin_atoi(argv[1], argv[0])) < 0)
      return 1;
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
shiftcmd(char **argv)
{
  int argc = 0;
  int n = 0;
  array_len(argv, argc);

  if (argc == 1) {
    n = 1;
  } else if (argc == 2) {
    if ((n = bltin_atoi(argv[1], argv[0])) < 0)
      return 1;
  } else {
    shwarn_arg(argv[0], argv[2], "too many arguements");
    return 1;
  }
  if (!n)
    return 0;
  if (n > sh_argc) {
    shwarnx(argv[0], "can't shift that many");
    return 1;
  }

  if (alloc_sh_argv)
    for (int i = 0; i < n; i++)
      slfree(sh_argv[i]);
  memmove(sh_argv, sh_argv + n, (sh_argc - n) * sizeof(char *));
  for (int i = sh_argc - n; i < sh_argc; i++)
    sh_argv[i] = NULL;
  sh_argc -= n;

  return 0;
}

static int
timescmd(char **argv)
{
  size_t argc = 0;
  array_len(argv, argc);
  ARGBEGIN
  {
    default:
      bad_opt(argv0, ARGC());
      return 1;
  }
  ARGEND
  struct rusage shell, chld;
  getrusage(RUSAGE_SELF, &shell);
  getrusage(RUSAGE_CHILDREN, &chld);

  printf("%dm%fs %dm%fs\n"
         "%dm%fs %dm%fs\n",
         (int)(shell.ru_utime.tv_sec / 60),
         (double)(shell.ru_utime.tv_sec % 60) + shell.ru_utime.tv_usec / 1e6,
         (int)(shell.ru_stime.tv_sec / 60),
         (double)(shell.ru_stime.tv_sec % 60) + shell.ru_stime.tv_usec / 1e6,
         (int)(chld.ru_utime.tv_sec / 60),
         (double)(chld.ru_utime.tv_sec % 60) + chld.ru_utime.tv_usec / 1e6,
         (int)(chld.ru_stime.tv_sec / 60),
         (double)(chld.ru_stime.tv_sec % 60) + chld.ru_stime.tv_usec / 1e6);

  return 0;
}

static int
truecmd(char **args)
{
  (void)args;
  return 0;
}

static int
typecmd(char **argv)
{
  int argc = 0;
  int status = 0;
  array_len(argv, argc);
  if (argc == 1)
    return 0;

  for (int i = 1; i < argc; i++)
    if (classify_cmd(argv[i], 1, 0))
      status = 1;
  return status;
}

/* NOLINTBEGIN(readability-magic-numbers) */
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
    }
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
  }

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
      fprintf(stderr, "%s: %s: %c:  not a valid operator \n", sh_argv0, argv0,
              *c);
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

    if (op == '=')
      mask = nm | (mask & ~(pos * 07));
    else if (op == '+')
      mask |= nm;
    else if (op == '-')
      mask &= ~nm;

    if (*c == ',')
      continue;
    if (*c == '\0')
      break;
  }
  umask((~mask) & 0777);
  return 0;
}
/* NOLINTEND(readability-magic-numbers) */
