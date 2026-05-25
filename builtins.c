/* builtins.c - builtin shell commands */
#include <bits/types/sigset_t.h>
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <err.h>
#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "arg.h"
#include "builtins.h"
#include "env.h"
#include "exec.h"
#include "job.h"
#include "lex.h"
#include "main.h"
#include "simpsh.h"
#include "utils.h"

static const char * const jstates[] = { "Running", "Stopped", "Done" };
#define bad_opt(a,b,c) (fprintf(stderr, "%s: %s: bad option %c\n", a,b,c))

static int aliascmd(char **);
static char *pwdpath(char *);
static int bgcmd(char **);
static int cdcmd(char **);
static int echocmd(char **);
static int execcmd(char **);
static int exitcmd(char **);
static int exportcmd(char **);
static int falsecmd(char **);
static int fgcmd(char **);
static int helpcmd(char **);
static int jobscmd(char **);
static int localcmd(char **);
static int pwdcmd(char **);
static int readonlycmd(char **);
// static int setcmd(char **);
static int truecmd(char **);
static int unaliascmd(char **);
static int unsetcmd(char **);

/* the array of builtin commands */ /* clang-format off */
const char *builtins[] = {
  "alias",
  "bg",
  "cd",
  "echo",
  "exec",
  "exit",
  "export",
  "false",
  "fg",
  "help",
  "jobs",
  "local",
  "pwd",
  "readonly",
  // "set",
  "true",
  "unalias",
  "unset",
};
int (* const builtin_funcs[])(char **) = {
  &aliascmd,
  &bgcmd,
  &cdcmd,
  &echocmd,
  &execcmd,
  &exitcmd,
  &exportcmd,
  &falsecmd,
  &fgcmd,
  &helpcmd,
  &jobscmd,
  &localcmd,
  &pwdcmd,
  &readonlycmd,
  // &setcmd,
  &truecmd,
  &unaliascmd,
  &unsetcmd,
};
int builtinnum(void) {
  return sizeof(builtins) / sizeof(char *);
} /* clang-format on */

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
    if (!(e = findalias(args[1])))
      return 1;
    else
      printf("alias %s=%s\n", e->name, e->value);
  } else {
    n = strndup_(args[1], strlen(args[1]) - strlen(delem));
    v = strdup_(delem + 1);
    setalias(n, v);
    free(n);
    free(v);
  }

  return 0;
}

static int
bgcmd(char **argv)
{
  size_t argc;

  // TODO: check interative mode or not
  argc = array_len(argv);
  if (argc < 2) {
    shwarnx(argv[0], "job id required");
    return 1;
  } else {
    job *cur, *sec;
    char c;

    cur = findjob(NULL);
    sec = findjob("%-");
    for (size_t i = 1; i < argc; i++) {
      job *j;

      j = findjob(argv[i]);
      if (!j) {
        shwarn_arg(argv[0], argv[i], "no such job");
        return 1;
      }
      if (j->state != JSTP) {
        shwarn_arg(argv[0], argv[i], "job not stopped");
        return 1;
      }

      sigset_t sigold;
      jobsig_blk(&sigold);
      if (kill(-j->pgid, SIGCONT) < 0) {
        jobsig_unblk(&sigold);
        err(1, "kill");
      }
      j->state = JRUN;
      jobsig_unblk(&sigold);
      if (cur == j)
        c = '+';
      else if (sec == j)
        c = '-';
      else
        c = ' ';
      printf("[%d]%c %s &\n", j->num, c, j->cmd);
    }
  }
  return 0;
}

static int
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
    shwarnx(bargv0, "Too many arguements");
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
      shwarnx(bargv0, "OLDPWD not set");
      return 1;
    } else {
      dest = shvar_val(oldpwd);
      prnt = 1;
    }
  } else {
    dest = (char *)dir;
  }

      // warn("cd: %s", dest); // XXX: these were there before shwarn
      // return 1;
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
    getcwd(respath, PATH_MAX);
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
      plen = strlen(pwdval);
      end = mempcpy_(respath, pwdval, plen);
      *end++ = '/';
      end = mempcpy_(end, dest, dlen);
      *end = '\0';
    }
    if (!pwdpath(respath)) {
      shwarnx(bargv0, "path normalization failure");
      return 1;
    }
  }
  setvar("OLDPWD", pwdval, VEXPRT);
  setvar("PWD", respath, VEXPRT);
  return 0;
}


static int
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
    shwarnx(argv[0], "failed to get environ");

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

static int
exitcmd(char **argv)
{
  size_t argc;
  int exnum;

  exnum = 0;
  argc = array_len(argv);
  if (argc > 2) {
    shwarnx(argv[0], "too many arguements");
    // fprintf(stderr, "%s: %s: \n", sh_argv0 ,argv[0]);
    return 1;
  } else if (argc == 2) {
    for (size_t i = 0; argv[1][i]; i++) {
      if (!isdigit_(argv[1][i])) {
        shwarn_arg(argv[0], argv[1], "a number is required");
        // fprintf(stderr, "%s: %s: a number is required\n", sh_argv0, argv[1]);
        return 1;
      }
    }
    exnum = atoi_(argv[1]);
  }
  exit(exnum);
}

static int
exportcmd(char **args)
{
  size_t argc;
  argc = array_len(args);

  /* nvar = s_strndup(args[i], nvarlen + 1); */ 
  if (argc > 1) {
    for (size_t i = 1; i < argc; i++) {
      char *eq;
      char *name, *val;
      shvar *v;

      eq = strchrnul_(args[i], '=');
      if (*eq == '\0') {
        v = findvar(args[i]);
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
fgcmd(char **argv)
{
  size_t argc;
  job *j;
  job *cur, *sec;
  char c;
  int status;

  argc = array_len(argv);
  if (argc < 2) {
    j = findjob(NULL);
    if (!j) {
      shwarnx(argv[0], "no current job");
      return 1;
    }
  } else {
    j = findjob(argv[argc - 1]);
    if (!j) {
      shwarn_arg(argv[0], argv[argc - 1], "no such job");
      return 1;
    }
  }

  if (j->state == JDONE) {
    shwarn_arg(argv[0], argv[argc-1], "job already completed");
    return 1;
  }

  cur = findjob(NULL);
  sec = findjob("%-");
  if (startjob(j->pgid) < 0)
    return 1;
  sigset_t sigold;
  jobsig_blk(&sigold);
  if (j->state == JSTP) {
    if (kill(-j->pgid, SIGCONT)) {
      endjob();
      jobsig_unblk(&sigold);
      err(1, "kill");
    }
  }

  int wstatus;
  status = 0;
  if (waitpid(-j->pgid, &wstatus, WUNTRACED) < 0) {
    endjob();
    jobsig_unblk(&sigold);
    err(1, "waitpid");
  }
  if (WIFEXITED(wstatus)) {
    status = WEXITSTATUS(wstatus);
  } else if (WIFSIGNALED(wstatus)) {
    status = 128 + WTERMSIG(wstatus);
  } else if (WIFSTOPPED(wstatus)) {
    j->state = JSTP;
    if (cur == j)
      c = '+';
    else if (sec == j)
      c = '-';
    else
      c = ' ';
    printf("[%d]%c Stopped\t\t\t\t\t\t\t\t\t\t\t\t%s\n",j->num, c, j->cmd);
    status = 128 + WSTOPSIG(wstatus);
  }

  endjob();
  jobsig_unblk(&sigold);
  if (j->state == JDONE) {
    rmjob(j);
  }

  return status;
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
jobscmd(char **argv)
{
  job *j;
  job *cur, *sec;
  size_t argc;
  const char *status;
  char c;

  j = job_list;
  argc = array_len(argv);
  status = 0;

  cur = findjob(NULL);
  sec = findjob("%-");
  if (argc < 2) {
    for (job *t = j; t;) {
      if (cur == t)
        c = '+';
      else if (sec == t)
        c = '-';
      else
        c = ' ';
      status = jstates[t->state];
      printf("[%d]%c %-8s\t\t\t\t\t\t\t\t\t\t\t\t%s\n", t->num, c, status, t->cmd);
      if (t->state == JDONE)
        t = rmjob(t);
      else
        t = t->next;
    }
  } else {
    for (size_t i = 1; i < argc; i++) {
      job *t;
      if ((t = findjob(argv[i]))) {
        if (cur == t)
          c = '+';
        else if (sec == t)
          c = '-';
        else
          c = ' ';
        status = jstates[t->state];
        printf("[%d]%c %s\t%s\n", t->num, c, status, t->cmd);
        if (t->state == JDONE)
          rmjob(t);
      } else {
        shwarn_arg(argv[0], argv[i], "no such job");
      }
    }
  }

  return 0;
}

static int localcmd(char **argv)
{
  size_t argc;

  argc = array_len(argv);

  if (func_depth == 0) {
    fprintf(stderr, "simpsh: local: can only be used in a function\n");
    return 1;
  }
  if (argc > 1) {
    for (size_t i = 1; i < argc; i++) {
      char *name, *val;

      st_read_assn(argv[i], &name, &val);
      localvars[localsp++] = grabvar(name);
      if (localsp >= LOCAL_MAX)
        err(1, "local variables exceeded max");
      if (val) {
        setvar(name, val, 0);
      } else {
        val = localvars[localsp - 1].val ? localvars[localsp - 1].val : "";
        setvar(name, val, 0);
      }
    }
  }
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

static int
readonlycmd(char **argv)
{
  int argc, i;
  argc = array_len(argv);

  /* nvar = s_strndup(args[i], nvarlen + 1); */ 
  if (argc > 1) {
    for (i = 1; i < argc; i++) {
      char *eq;
      char *name, *val;
      shvar *v;

      eq = strchrnul_(argv[i], '=');
      if (*eq == '\0') {
        v = findvar(argv[i]);
        if (v)
          v->flags |= VREADONLY;
        else
          setvar(argv[i], NULL, VEXPRT);
      } else {
        read_assn(argv[i], &name, &val);
        setvar(name, val, VREADONLY);
        free(name);
        free(val);
      }
    }
  }

  return 0;
}

// static int
// setcmd(char **argv)
// {
//   (void)argv;
//   return 0;
// }

static int
truecmd(char **args)
{
  (void)args;
  return 0;
}

static int
unaliascmd(char **argv)
{
  alias *e;
  /* int i;
   char *n, *v; */

  e = findalias(argv[1]);
  if (e) {
    rmalias(argv[1]);
  } else {
    shwarnx(argv[0], "alias not found");
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
  char *bargv0;
  argc = array_len(argv);
  enum {
    VARS,
    FUNC,
  } flag;

  flag = 0;
  bargv0 = argv[0];
  ARGBEGIN
  {
    case 'v':
      flag = VARS;
      break;
    case 'f':
      flag = FUNC;
      break;
    default:
      bad_opt(sh_argv0, argv0, ARGC());
      return 1;
  }
  ARGEND

  err = 0;
  c = 0;
  if (argc < 1) {
    return 0;

  } else {
    for (; c < argc; c++) {
      if (flag == FUNC) {
        rmfunc(argv[c]);
      } else {
        v = findvar(argv[c]);
        if (v) {
          if (v->flags & VREADONLY) {
            shwarn_arg(bargv0, argv[c], "cannot unset: readonly variable");
            err = 1;
          } else {
            rmvar(argv[c]);
          }
        }
      }
      if (err)
        return 1;
    }
  }

  return 0;
}
