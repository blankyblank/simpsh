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
static int exitcmd(char **);
static int falsecmd(char **);
static int getoptscmd(char **);
static int pwdcmd(char **);
static int readcmd(char **);
static int returncmd(char **);
static int shiftcmd(char **);
static int timescmd(char **);
static int truecmd(char **);
static int typecmd(char **);
static int ulimitcmd(char **);
static int umaskcmd(char **);

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
  { "getopts",  &getoptscmd  },
  { "hash",     &hashcmd     },
  { "help",     &helpcmd     },
  { "jobs",     &jobscmd     },
  { "kill",     &killcmd     },
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
  { "ulimit",   &ulimitcmd   },
  { "umask",    &umaskcmd    },
  { "unalias",  &unaliascmd  },
  { "unset",    &unsetcmd    },
  { "wait",     &waitcmd     },
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

/* convert char to int, doing extra checks, and handling error messages */
int
bltin_atoi(char *s, char *b, char *msg)
{
  int n;
  for (i32 i = 0; s[i]; i++) {
    if (!isdigit_(s[i])) {
      shwarn_arg(b, s, msg);
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
    if ((e = chkpath((def) ? defpath : getvar(STR("PATH")), s, X_OK, 0))) {
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
  if ((e = chkpath((def) ? defpath : getvar(STR("PATH")), s, X_OK, 0))) {
    printf("%s\n", e);
    return 0;
  }
  return 1;
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
    shwarn(argv[0], "not in a loop");
    return 1;
  }
  array_len(argv, argc);
  if (argc < 2) {
    n = 1;
  } else if (argc == 2) {
    n = bltin_atoi(argv[1], argv[0], "a numeric arguement is required");
    if (n <= 0) {
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
    shwarn(bargv0, "Too many arguements"); /*NOLINT*/
    return 1;
  }

  oldpwd = findvar(STR("OLDPWD"));
  pwd = findvar(STR("PWD"));
  if (pwd)
    pwdval = shvar_val(pwd);
  else
    pwdval = getcwd(respath, PATH_MAX);
  cdpth = findvar(STR("CDPATH"));
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
      shwarn(bargv0, "OLDPWD not set"); /*NOLINT*/
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
      shwarn(bargv0, "path normalization failure"); /*NOLINT*/
      return 1;
    }
  }
  setvar(STR("OLDPWD"), pwdval, VEXPRT);
  setvar(STR("PWD"), respath, VEXPRT);
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
  if ((path = (def) ? defpath : getvar(STR("PATH"))))
    path = defpath;
  if (!(fpath = chkpath(path, argv[0], X_OK, 0))) {
    shwarn(argv[0], "command not found");
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
    shwarn(argv[0], "not in a loop");
    return 1;
  }
  array_len(argv, argc);
  if (argc < 2) {
    n = 1;
  } else if (argc == 2) {
    if ((n = bltin_atoi(argv[1], argv[0], "a numeric arguement is required")) <=
        0) {
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
    if ((path = getvar(STR("PATH"))))
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
  setinputf(fd, file, 0);

  if (argc == 2) {
    o_argv0 = strdup_(shargv0);
    shargv0 = strdup_(file);
  } else {
    o_argc = shargc;
    shargc = argc - 2;
    o_argv0 = strdup_(shargv0);
    shargv0 = strdup_(file);

    o_argv = salloc(sizeof(char *) * (o_argc + 1));
    for (int i = 0; i < o_argc; i++)
      o_argv[i] = strdup_(shargv[i]);
    o_argv[o_argc] = NULL;
    if (alloc_shargv && shargv) {
      for (int i = 0; i < o_argc; i++)
        slfree(shargv[i]);
      slfree(shargv);
    }
    shargv = salloc(sizeof(char *) * (argc + 1));
    size_t j = 0;
    for (size_t i = 2; argv[i]; i++)
      shargv[j++] = strdup_(argv[i]);
    shargv[shargc] = NULL;
    alloc_shargv = 1;
  }

  eval_run();
  retnow = 0;
  popinput();

restore:
  if (o_argv) {
    for (size_t i = 0; shargv[i]; i++)
      slfree(shargv[i]);
    slfree(shargv);
    shargv = o_argv;
  }
  if (o_argv0) {
    slfree(shargv0);
    shargv0 = o_argv0;
  }
  if (o_argc)
    shargc = o_argc;
  if (st)
    perror(argv[1]);
  return st ? st : lstatus;
}

static int
echocmd(char *argv[])
{
  int nf = 0;
  size_t argc = 0;
  char *argv0 = argv[0];

  array_len(argv, argc);

  if (argv[1] && argv[1][0] == '-' && argv[1][1] == 'n' && !argv[1][2])
    nf = FLAG_N, argv++, argc--;
  argv++, argc--;

  if (fcntl(STDOUT_FILENO, F_GETFD) < 0) {
    sherr(1, argv0, "could not write to stdout");
  }
  for (size_t i = 0; i < argc; i++) {
    if (fputs(argv[i], stdout) == EOF) {
      sherr(1, argv0, "could not write to stdout");
    }
    if (i < argc - 1)
      if (fputc(' ', stdout) == EOF) {
        sherr(1, argv0, "could not write to stdout");
      }
  }
  if (!(nf & FLAG_N))
    if (fputc('\n', stdout) == EOF) {
      warn("%s: %s", argv0, "could not write to stdout");
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
exitcmd(char **argv)
{
  size_t argc = 0;
  int exnum;

  exnum = 0;
  array_len(argv, argc);
  if (argc > 2) {
    shwarn(argv[0], "too many arguements"); /*NOLINT*/
    return 1;
  }

  if (argc == 2) {
    exnum = bltin_atoi(argv[1], argv[0], "a numeric arguement is required");
    if (exnum < 0)
      return 1;
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
getoptscmd(char **argv)
{
  (void)argv;
  return 0;
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
    char *pwd = getvar(STR("PWD"));
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

  setinputf(STDIN_FILENO, NULL, 1);
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
    shwarn(argv[0], "too many arguements");
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
    n = bltin_atoi(argv[1], argv[0], "a numeric arguement is required");
    if (n < 0)
      return 1;
  } else {
    shwarn_arg(argv[0], argv[2], "too many arguements");
    return 1;
  }
  if (!n)
    return 0;
  if (n > shargc) {
    shwarn(argv[0], "can't shift that many");
    return 1;
  }

  if (alloc_shargv)
    for (int i = 0; i < n; i++)
      slfree(shargv[i]);
  memmove(shargv, shargv + n, (shargc - n) * sizeof(char *));
  for (int i = shargc - n; i < shargc; i++)
    shargv[i] = NULL;
  shargc -= n;

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

typedef struct {
  const char *name;
  int resource; /* cmd to get/set */
  int factor; /* multiply by to get rlim_{cur,max} values */
  char option; /* option character (-d, -f, ...) */
} limit;
#define SOFT 1 << 0
#define HARD 1 << 1

int
ulimitcmd(char **argv)
{
  int argc = 0;
  int ltype = SOFT, all = 0;
  size_t optc = 0;
  const limit *l;
  char *opt = st_alloc(10 * sizeof(char));

  static const limit limits[] = {
    { "time(cpu-seconds)",    RLIMIT_CPU,     1,    't'  },
    { "file(blocks)",         RLIMIT_FSIZE,   512,  'f'  },
    { "coredump(blocks)",     RLIMIT_CORE,    512,  'c'  },
    { "data(kbytes)",         RLIMIT_DATA,    1024, 'd'  },
    { "stack(kbytes)",        RLIMIT_STACK,   1024, 's'  },
    { "lockedmem(kbytes)",    RLIMIT_MEMLOCK, 1024, 'l'  },
    { "memory(kbytes)",       RLIMIT_RSS,     1024, 'm'  },
    { "nofiles(descriptors)", RLIMIT_NOFILE,  1,    'n'  },
    { "processes",            RLIMIT_NPROC,   1,    'p'  },
    { NULL,                   0,              0,    '\0' },
  };

  array_len(argv, argc);
  ARGBEGIN
  {
    case 'a':
      all = 1;
      break;
    case 'c':
    case 'd':
    case 'f':
    case 'l':
    case 'm':
    case 'n':
    case 'p':
    case 's':
    case 't':
      opt[optc++] = ARGC();
      break;
    case 'H':
      ltype = HARD;
      break;
    case 'S':
      ltype = SOFT;
      break;
    default:
      bad_opt(argv0, ARGC());
      return 1;
  }
  ARGEND
  opt[optc] = '\0';

  if (all) {
    if (*argv) {
      usage(argv0, helpmsgs[ULIMITH].usage);
      return 1;
    }
    opt = "cdflmnpst";
  }
  if (!*argv) {
    for (char *s = opt; *s; s++) {
      struct rlimit lim;
      rlim_t val = 0;

      l = limits;
      while (l->name && l->option != *s)
        l++;
      if (!l->name) {
        shwarn_arg(argv0, s, "unknown option");
        return 1;
      }
      getrlimit(l->resource, &lim);
      if (ltype & HARD)
        val = lim.rlim_max;
      else
        val = lim.rlim_cur;
      if (all)
        printf("%s\t\t", l->name);
      if (val == RLIM_INFINITY) {
        printf("unlimited\n");
        continue;
      }
      val /= l->factor;
      printf("%lu\n", val);
      continue;
    }
    return 0;
  }

  if (argv[1]) {
    shwarn_arg(argv0, *argv, "too many arguements");
    return 1;
  }
  char *arg = *argv;
  int n = 0;
  rlim_t val;

  if (*arg == 'u' && strcmp(arg, "unlimited") == 0) {
    val = RLIM_INFINITY;
  } else if (isdigit_(*arg)) {
    if ((n = bltin_atoi(arg, argv0, "invalid number")) < 0)
      return 1;
    val = n;
  } else {
    shwarn_arg(argv0, arg, "invalid number");
    return 1;
  }

  for (char *s = opt; *s; s++) {
    struct rlimit lim;
    l = limits;
    while (l->name && l->option != *s)
      l++;
    if (!l->name) {
      shwarn_arg(argv0, s, "unknown option");
      return 1;
    }

    getrlimit(l->resource, &lim);
    if (ltype & HARD)
      lim.rlim_max = (val == RLIM_INFINITY) ? val : val * l->factor;
    else
      lim.rlim_cur = (val == RLIM_INFINITY) ? val : val * l->factor;

    if (setrlimit(l->resource, &lim) < 0) {
      sherr(1, argv0, "setrlimit");
    }
  }
  return 0;
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
      int c = (unsigned char)argv[0][i];
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
      fprintf(stderr, "%s: %s: %c:  not a valid operator \n", shargv0, argv0,
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
