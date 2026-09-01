/* builtins.c - builtin shell commands */
#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */
#define _DEFAULT_SOURCE

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
#include "config.h"
#include "env.h"
#include "exec.h"
#include "errmsg.h"
#include "input.h"
#include "main.h"
#include "opts.h"
#include "path.h"
#include "pipe.h"
#include "simd.h"
#include "simpsh.h"
#include "utils.h"
#include "var.h"

/* builtins */
extern int aliascmd(char **);
extern int bgcmd(char **);
static int breakcmd(char **);
extern int cdcmd(char **);
static int continuecmd(char **);
static int echocmd(char **);
static int exitcmd(char **);
extern int exportcmd(char **);
static int falsecmd(char **);
extern int fccmd(char **);
extern int fgcmd(char **);
extern int getoptscmd(char **);
extern int hashcmd(char **);
extern int helpcmd(char **);
extern int jobscmd(char **);
extern int killcmd(char **);
extern int localcmd(char **);
extern int printfcmd(char **);
extern int pwdcmd(char **);
static int readcmd(char **);
extern int readonlycmd(char **);
extern int setcmd(char **);
static int shiftcmd(char **);
extern int testcmd(char **);
static int timescmd(char **);
extern int trapcmd(char **);
static int truecmd(char **);
static int typecmd(char **);
static int ulimitcmd(char **);
extern int unaliascmd(char **);
static int umaskcmd(char **);
extern int unsetcmd(char **);
extern int waitcmd(char **);


/* extra builtins (no fork+exec = fast) */
#if ENABLE_BASENAME
extern int basenamecmd(char **);
#endif /* ENABLE_BASENAME */
#if ENABLE_CAT
extern int catcmd(char **);
#endif /* ENABLE_CAT */
#if ENABLE_COMM
extern int commcmd(char **);
#endif /* ENABLE_COMM */
#if ENABLE_CUT
extern int cutcmd(char **);
#endif /* ENABLE_CUT */
#if ENABLE_DIRNAME
extern int dirnamecmd(char **);
#endif /* ENABLE_DIRNAME */
#if ENABLE_EXPAND
extern int expandcmd(char **);
#endif /* ENABLE_EXPAND */
#if ENABLE_FOLD
extern int foldcmd(char **);
#endif /* ENABLE_FOLD */
#if ENABLE_HEAD
extern int headcmd(char **);
#endif /* ENABLE_HEAD */
#if ENABLE_PASTE
extern int pastecmd(char **);
#endif /* ENABLE_PASTE */
#if ENABLE_READLINK
extern int readlinkcmd(char **);
#endif /* ENABLE_READLINK */
#if ENABLE_REALPATH
extern int realpathcmd(char **);
#endif /* ENABLE_REALPATH */
#if ENABLE_SLEEP
extern int sleepcmd(char **);
#endif /* ENABLE_SLEEP */
#if ENABLE_SORT
extern int sortcmd(char **);
#endif /* ENABLE_SORT */
#if ENABLE_TAIL
extern int tailcmd(char **);
#endif /* ENABLE_TAIL */
#if ENABLE_TEE
extern int teecmd(char **);
#endif /* ENABLE_TEE */
#if ENABLE_TR
extern int trcmd(char **);
#endif /* ENABLE_TR */
#if ENABLE_EXPAND
extern int unexpandcmd(char **);
#endif /* ENABLE_EXPAND */
#if ENABLE_UNIQ
extern int uniqcmd(char **);
#endif /* ENABLE_UNIQ */
#if ENABLE_WC
extern int wccmd(char **);
#endif /* ENABLE_WC */


static int classify_cmd(char *, int, int);

/* the array of builtin commands */
const builtin builtins[] = {
  { ".",        &dotcmd,      0     },
  { "[",        &testcmd,     0     },
  { ":",        &truecmd,     0     },
  { "alias",    &aliascmd,    0     },
#if ENABLE_BASENAME
  { "basename", &basenamecmd, 0     },
#endif  /* ENABLE_BASENAME */
  { "bg",       &bgcmd,       0     },
  { "break",    &breakcmd,    SBLTN },
#if ENABLE_CAT
  { "cat",      &catcmd,      0     },
#endif  /* ENABLE_CAT */
#if ENABLE_COMM
  { "comm",     &commcmd,     0     },
#endif  /* ENABLE_COMM */
  { "cd",       &cdcmd,       0     },
  { "command",  &commandcmd,  0     },
  { "continue", &continuecmd, SBLTN },
#if ENABLE_CUT
  { "cut",      &cutcmd,      0     },
#endif  /* ENABLE_CUT */
#if ENABLE_DIRNAME
  { "dirname",  &dirnamecmd,  0     },
#endif  /* ENABLE_DIRNAME */
  { "echo",     &echocmd,     0     },
  { "eval",     &evalcmd,     SBLTN },
  { "exec",     &execcmd,     SBLTN },
  { "exit",     &exitcmd,     SBLTN },
#if ENABLE_EXPAND
  { "expand",   &expandcmd,   0     },
#endif  /* ENABLE_EXPAND */
  { "export",   &exportcmd,   SBLTN },
  { "false",    &falsecmd,    0     },
  { "fc",       &fccmd,       0     },
  { "fg",       &fgcmd,       0     },
#if ENABLE_FOLD
  { "fold",     &foldcmd,     0     },
#endif  /* ENABLE_FOLD */
  { "getopts",  &getoptscmd,  0     },
  { "hash",     &hashcmd,     0     },
#if ENABLE_HEAD
  { "head",     &headcmd,     0     },
#endif  /* ENABLE_HEAD */
  { "help",     &helpcmd,     0     },
  { "jobs",     &jobscmd,     0     },
  { "kill",     &killcmd,     0     },
  { "local",    &localcmd,    0     },
#if ENABLE_PASTE
  { "paste",    &pastecmd,    0     },
#endif  /* ENABLE_PASTE */
  { "printf",   &printfcmd,   0     },
  { "pwd",      &pwdcmd,      0     },
  { "read",     &readcmd,     0     },
#if ENABLE_READLINK
  { "readlink", &readlinkcmd, 0     },
#endif  /* ENABLE_READLINK */
#if ENABLE_REALPATH
  { "realpath", &realpathcmd, 0     },
#endif  /* ENABLE_REALPATH */
  { "readonly", &readonlycmd, SBLTN },
  { "return",   &returncmd,   SBLTN },
  { "set",      &setcmd,      SBLTN },
  { "shift",    &shiftcmd,    SBLTN },
#if ENABLE_SLEEP
  { "sleep",    &sleepcmd,    0     },
#endif  /* ENABLE_SLEEP */
#if ENABLE_SORT
  { "sort",     &sortcmd,     0     },
#endif  /* ENABLE_SORT */
#if ENABLE_TAIL
  { "tail",     &tailcmd,     0     },
#endif  /* ENABLE_TAIL */
#if ENABLE_TEE
  { "tee",      &teecmd,      0     },
#endif  /* ENABLE_TEE */
  { "test",     &testcmd,     0     },
  { "times",    &timescmd,    SBLTN },
#if ENABLE_TR
  { "tr",      &trcmd,      0     },
#endif  /* ENABLE_TR */
  { "trap",     &trapcmd,     SBLTN },
  { "true",     &truecmd,     SBLTN },
  { "type",     &typecmd,     0     },
  { "ulimit",   &ulimitcmd,   0     },
  { "umask",    &umaskcmd,    0     },
  { "unalias",  &unaliascmd,  0     },
#if ENABLE_EXPAND
  { "unexpand", &unexpandcmd, 0     },
#endif  /* ENABLE_EXPAND */
#if ENABLE_UNIQ
  { "uniq",     &uniqcmd,     0     },
#endif  /* ENABLE_UNIQ */
  { "unset",    &unsetcmd,    SBLTN },
  { "wait",     &waitcmd,     0     },
#if ENABLE_WC
  { "wc",       &wccmd,       0     },
#endif  /* ENABLE_WC */
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
  int h;
  size_t len;
  len = strlen(str);
  h = kwhash(str, len);
  if (kw[h].word && kw[h].len == len && memcmp(kw[h].word, str, len) == 0)
    return 1;
  return 0;
}

static int
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
    if ((e = chkpath((def) ? defpath : getvar("PATH"), s, X_OK, 0))) {
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
  if ((e = chkpath((def) ? defpath : getvar("PATH"), s, X_OK, 0))) {
    printf("%s\n", e);
    return 0;
  }
  return 1;
}

static int
breakcmd(char **argv)
{
  size_t argc = 0;
  int n;

  if (!LOOPDEPTH)
    return shwarn(argv[0], "not in a loop");
  array_len(argv, argc);
  if (argc < 2) {
    n = 1;
  } else if (argc == 2) {
    n = bltin_atoi(argv[1], argv[0], "a numeric argument is required");
    if (n <= 0) {
      if (!n)
        shwarn_arg(argv[0], argv[1], "must be a positive integer");
      return 1;
    }
  } else {
    return shwarn_arg(argv[0], argv[2], "too many arguments");
  }

  if (n > LOOPDEPTH)
    n = LOOPDEPTH;
  LOOPBREAK = n;
  return 0;
}

int
commandcmd(char **argv)
{
  int flags = 0, argc = 0;
  int def = 0, status = 0;
  const char *path;

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
      return bad_opt(argv0, ARGC());
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
  if ((path = (def) ? defpath : getvar("PATH")))
    path = defpath;
  if (!(fpath = chkpath(path, argv[0], X_OK, 0)))
    return shwarn(argv[0], "command not found");
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

  if (!LOOPDEPTH)
    return shwarn(argv[0], "not in a loop");
  array_len(argv, argc);
  if (argc < 2) {
    n = 1;
  } else if (argc == 2) {
    if ((n = bltin_atoi(argv[1], argv[0], "a numeric argument is required")) <=
        0) {
      if (!n)
        shwarn_arg(argv[0], argv[1], "must be a positive integer");
      return 1;
    }
  } else {
    return shwarn_arg(argv[0], argv[2], "too many arguments");
  }

  if (n > LOOPDEPTH)
    n = LOOPDEPTH;
  LOOPCONT = n;
  return 0;
}

int
dotcmd(char **argv)
{
  size_t argc = 0;
  char *file;
  int fd, status;

  array_len(argv, argc);

  if (argc < 2)
    return shwarn(argv[0], "filename argument required");

  if (strchr(argv[1], '/')) {
    if (access(argv[1], R_OK) < 0)
      return 1;
    file = argv[1];
  } else {
    char *fpath, *path;
    if ((path = getvar("PATH")))
      fpath = chkpath(path, argv[1], R_OK, 0);
    else
      fpath = chkpath(defpath, argv[1], R_OK, 0);
    if (fpath) {
      file = fpath;
    } else {
      if (access(argv[1], R_OK) < 0)
        return 1;
      file = argv[1];
    }
  }
  if (!file)
    return 1;
  if ((fd = open(file, O_RDONLY)) < 0)
    return 1;
  setinputf(fd, file, 0);

  pushframe();
  SHARGV0 = strdup_(file);

  if (argc > 2) {
    SHARGC = argc - 2;
    SHARGV = salloc(sizeof(char *) * (SHARGC + 1));
    for (int i = 0; i < SHARGC; i++)
      SHARGV[i] = strdup_(argv[i + 2]);
    SHARGV[SHARGC] = NULL;
    ALLOCED = 1;
  }
  RETNOW = LOOPDEPTH = LOOPBREAK = LOOPCONT = 0;

  eval_run();
  RETNOW = 0;
  popinput();

  status = LSTATUS;
  if (argc == 2) {
    char **pargv;
    int pargc, palloced;

    pargv = SHARGV;
    pargc = SHARGC;
    palloced = ALLOCED;
    sfree(SHARGV0);
    popframe();
    SHARGV = pargv;
    SHARGC = pargc;
    ALLOCED = palloced;
  } else {
    if (argc > 2)
      freeshargv();
    sfree(SHARGV0);
    popframe();
  }
  return status;
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

  if (fcntl(STDOUT_FILENO, F_GETFD) < 0)
    return sherr(1, argv0, "could not write to stdout");
  for (size_t i = 0; argv[i]; i++) {
    if (fputs(argv[i], shout) == EOF)
      return sherr(1, argv0, "could not write to stdout");
    if (i < argc - 1)
      if (fputc(' ', shout) == EOF)
        return sherr(1, argv0, "could not write to stdout");
  }
  if (!(nf & FLAG_N))
    if (fputc('\n', shout) == EOF) {
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
  if (argc > 2)
    return shwarn(argv[0], "too many arguments"); /*NOLINT*/

  if (argc == 2) {
    exnum = bltin_atoi(argv[1], argv[0], "a numeric argument is required");
    if (exnum < 0)
      return 1;
  }
  if (fakectx) {
    RETVAL = exnum;
    RETNOW = 1;
    return exnum;
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
fdgetc(int fd)
{
  unsigned char c;
  ssize_t n;
  n = read(fd, &c, 1);
  return n == 1 ? (int)c : EOF;
}

static int
readcmd(char **argv)
{
  int rflag = 1 << 0;
  int pflag = 1 << 1;
  int flag = 0, rfd;
  size_t argc = 0;
  char *prompt = NULL;

  array_len(argv, argc);
  ARGBEGIN
  {
    case 'r':
      flag |= rflag;
      break;
    case 'p':
      flag |= pflag;
      if (!(prompt = EARGF(no_opt(argv0, ARGC()))))
        return 1;
      break;
    default:
      return bad_opt(argv0, ARGC());
  }
  ARGEND;
  if (!argc)
    return shwarn_arg(argv0, "1", "requires variable name");

  rfd = fileno(shin);
  stmark rmark;
  int c, status = 0;
  size_t len = 0, ifslen, cleft, nws = 0;
  char *ifs = NULL;
  char ifsws[4],  *line, *p;

  rmark = stack_mark();

  if ((flag & pflag)) {
    fputs(prompt, stderr);
    fflush_unlocked(stderr);
  }
  stcheck(32);
  clearerr(shin);
  while ((c = (rfd >= 0 ? fdgetc(rfd) : fgetc(shin)))) {
    switch (c) {
      case EOF:
        status = 1;
        goto rend;
      case '\0':
        continue;
      case '\\':
        if ((c = (rfd >= 0 ? fdgetc(rfd) : fgetc(shin))) == EOF) {
          status = 1;
          goto rend;
        }
        if (flag & rflag) {
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
        setvar(argv[j], 0, 0);
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

int
returncmd(char **argv)
{
  size_t argc = 0;
  int status = 0;
  array_len(argv, argc);

  if (argc > 2)
    return shwarn(argv[0], "too many arguments");

  if (argc == 2) {
    for (size_t i = 0; argv[1][i]; i++) {
      if (!isdigit_(argv[1][i]))
        return shwarn_arg(argv[0], argv[1], "a numeric argument is required");
    }
    status = atoi_(argv[1]);
  }
  RETVAL = status, RETNOW = 1;
  return status;
}

static int
shiftcmd(char **argv)
{
  int argc = 0;
  int n = 0;
  array_len(argv, argc);
  if (fakectx)
    svfkargv(fkstate);

  if (argc == 1) {
    n = 1;
  } else if (argc == 2) {
    n = bltin_atoi(argv[1], argv[0], "a numeric argument is required");
    if (n < 0)
      return 1;
  } else {
    return shwarn_arg(argv[0], argv[2], "too many arguments");
  }
  if (!n)
    return 0;
  if (n > SHARGC)
    return shwarn(argv[0], "can't shift that many");

  if (ALLOCED)
    for (int i = 0; i < n; i++)
      sfree(SHARGV[i]);
  memmove(SHARGV, SHARGV + n, (SHARGC - n) * sizeof(char *));
  for (int i = SHARGC - n; i < SHARGC; i++)
    SHARGV[i] = NULL;
  SHARGC -= n;

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
      return bad_opt(argv0, ARGC());
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

#define SOFT 1 << 0
#define HARD 1 << 1

const limit limits[] = {
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

int
ulimitcmd(char **argv)
{
  int argc = 0;
  int ltype = SOFT, all = 0;
  size_t optc = 0;
  const limit *l;
  char *opt = st_alloc(10 * sizeof(char));

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
      return bad_opt(argv0, ARGC());
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
      if (!l->name)
        return shwarn_arg(argv0, s, "unknown option");
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
      printf("%llu\n", (unsigned long long)val);
      continue;
    }
    return 0;
  }

  if (argv[1])
    return shwarn_arg(argv0, *argv, "too many arguments");
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
    return shwarn_arg(argv0, arg, "invalid number");
  }

  for (char *s = opt; *s; s++) {
    struct rlimit lim;
    l = limits;
    while (l->name && l->option != *s)
      l++;
    if (!l->name)
      return shwarn_arg(argv0, s, "unknown option");

    getrlimit(l->resource, &lim);
    if (fakectx)
      savefkulimit(fkstate, l->resource, lim.rlim_cur, lim.rlim_max);
    if (ltype & HARD)
      lim.rlim_max = (val == RLIM_INFINITY) ? val : val * l->factor;
    else
      lim.rlim_cur = (val == RLIM_INFINITY) ? val : val * l->factor;

    if (setrlimit(l->resource, &lim) < 0)
      return sherr(1, argv0, "setrlimit");
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
      return bad_opt(argv0, ARGC());
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
      putc(ugo[i], shout);
      putc('=', shout);
      if (val[i] & 4)
        putc('r', shout);
      if (val[i] & 2)
        putc('w', shout);
      if (val[i] & 1)
        putc('x', shout);
      if (i < 2)
        putc(',', shout);
    }
    putc('\n', shout);
    return 0;
  }

  if (!symb) {
    mode_t val = 0;

    for (int i = 0; argv[0][i]; i++) {
      int c = (unsigned char)argv[0][i];
      if (c < '0' || c > '7')
        return shwarn_arg(argv0, argv[0], "octal number out of range");
      val = (val << 3) | (c - '0');
    }
    if (fakectx)
      svfkumask(fkstate);
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
      fprintf(stderr, "%s: %s: %c:  not a valid operator \n", SHARGV0, argv0,
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
  if (fakectx)
    svfkumask(fkstate);
  umask((~mask) & 0777);
  return 0;
}

/* NOLINTEND(readability-magic-numbers) */
